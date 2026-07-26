#ifndef THREADAFFINITY_HPP
#define THREADAFFINITY_HPP

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gnuradio-4.0/meta/formatter.hpp>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__))) || defined(__MINGW32__) // UNIX-style OS
#include <unistd.h>
#if defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#endif
#endif

#if defined(__EMSCRIPTEN__)
#include <cstdlib> // for atoi()
#include <emscripten.h>
#else
#if !defined(_WIN32)
#include <sys/resource.h>
#endif
#endif

namespace gr::thread_pool::thread {

constexpr size_t THREAD_MAX_NAME_LENGTH  = 16;
constexpr int    THREAD_UNINITIALISED    = 1;
constexpr int    THREAD_ERROR_UNKNOWN    = 2;
constexpr int    THREAD_VALUE_RANGE      = 3;
constexpr int    THREAD_INVALID_ARGUMENT = 22;
constexpr int    THREAD_ERANGE           = 34;

class thread_exception : public std::error_category {
    using std::error_category::error_category;

public:
    constexpr thread_exception() : std::error_category() {};

    const char* name() const noexcept override { return "thread_exception"; };

    std::string message(int errorCode) const override {
        switch (errorCode) {
        case THREAD_UNINITIALISED: return "thread uninitialised or user does not have the appropriate rights (ie. CAP_SYS_NICE capability)";
        case THREAD_ERROR_UNKNOWN: return "thread error code 2";
        case THREAD_INVALID_ARGUMENT: return "invalid argument";
        case THREAD_ERANGE: return std::format("length of the string specified pointed to by name exceeds the allowed limit THREAD_MAX_NAME_LENGTH = '{}'", THREAD_MAX_NAME_LENGTH);
        case THREAD_VALUE_RANGE: return std::format("priority out of valid range for scheduling policy", THREAD_MAX_NAME_LENGTH);
        default: return std::format("unknown threading error code {}", errorCode);
        }
    };
};

template<class type>
#if __cpp_lib_jthread >= 201911L
concept thread_type = std::is_same_v<type, std::thread> || std::is_same_v<type, std::jthread>;
#else
concept thread_type = std::is_same_v<type, std::thread>;
#endif

namespace detail {
template<typename Tp, typename... Us>
constexpr decltype(auto) firstElement(Tp&& t, Us&&...) noexcept {
    return std::forward<Tp>(t);
}
#if defined(_WIN32)
inline void* getThreadHandle(thread_type auto&... t) noexcept {
    if constexpr (sizeof...(t) > 0) {
        return firstElement(t...).native_handle(); // returns HANDLE (void*) on MinGW
    } else {
        return GetCurrentThread(); // pseudo-handle for current thread
    }
}
#elif defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)

inline constexpr pthread_t getPosixHandler(thread_type auto&... t) noexcept {
    if constexpr (sizeof...(t) > 0) {
        return firstElement(t...).native_handle();
    } else {
        return pthread_self();
    }
}
#endif

#if defined(_WIN32)
inline std::string getThreadName(const void* handle) {
    // Use GetThreadDescription on Win10+; fallback for older Windows
    wchar_t* buf = nullptr;
    if (handle == GetCurrentThread()) {
        HRESULT hr = GetThreadDescription(GetCurrentThread(), &buf);
        if (SUCCEEDED(hr) && buf) {
            int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
            std::string name;
            if (len > 0) {
                name.resize(static_cast<std::size_t>(len) - 1);
                WideCharToMultiByte(CP_UTF8, 0, buf, -1, name.data(), len, nullptr, nullptr);
            }
            LocalFree(buf);
            return name.empty() ? "unnamed thread" : name;
        }
    }
    return "unnamed thread";
}
#elif defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)
inline std::string getThreadName(const pthread_t& handle) {
    if (handle == 0U) {
        return "uninitialised thread";
    }
    char threadName[THREAD_MAX_NAME_LENGTH];
    if (int rc = pthread_getname_np(handle, threadName, THREAD_MAX_NAME_LENGTH); rc != 0) {
        throw std::system_error(rc, thread_exception(), "getThreadName(thread_type)");
    }
    return std::string{threadName, strnlen(threadName, THREAD_MAX_NAME_LENGTH)};
}
#endif

#if !defined(_WIN32)
inline int getPid() { 
#if defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)
    return getpid();
#else
    return 0;
#endif
}
#endif
} // namespace detail

#if defined(_WIN32)
inline std::string getThreadName(thread_type auto&... thread) {
    void* handle = detail::getThreadHandle(thread...);
    if (handle == nullptr) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "getThreadName(thread_type)");
    }
    return detail::getThreadName(handle);
}
#elif defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)
inline std::string getThreadName(thread_type auto&... thread) {
    const pthread_t handle = detail::getPosixHandler(thread...);
    if (handle == 0U) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "getThreadName(thread_type)");
    }
    return detail::getThreadName(handle);
}
#else
inline std::string getThreadName(thread_type auto&... /*thread*/) { return "unknown thread name"; }
#endif

#if defined(_WIN32)
inline void setThreadName(const std::string_view& threadName, thread_type auto&... thread) {
    void* handle = detail::getThreadHandle(thread...);
    if (handle == nullptr) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "setThreadName(thread_type)");
    }
    // Convert to wide string for SetThreadDescription
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, threadName.data(), static_cast<int>(threadName.size()), nullptr, 0);
    std::wstring wideName(static_cast<std::size_t>(wideLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, threadName.data(), static_cast<int>(threadName.size()), wideName.data(), wideLen);
    HRESULT hr = SetThreadDescription(static_cast<HANDLE>(handle), wideName.c_str());
    if (FAILED(hr)) {
        throw std::system_error(hr, thread_exception(), std::format("setThreadName({}) - SetThreadDescription failed", threadName));
    }
}
#elif defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)
inline void setThreadName(const std::string_view& threadName, thread_type auto&... thread) {
    const pthread_t handle = detail::getPosixHandler(thread...);
    if (handle == 0U) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("setThreadName({}, thread_type)", threadName, detail::getThreadName(handle)));
    }
    if (int rc = pthread_setname_np(handle, threadName.data()); rc < 0) {
        throw std::system_error(rc, thread_exception(), std::format("setThreadName({},{}) - error code '{}'", threadName, detail::getThreadName(handle), rc));
    }
}
#else
inline void setThreadName(const std::string_view& /*threadName*/, thread_type auto&... /*thread*/) {}
#endif

#if defined(_POSIX_VERSION) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__) && !defined(_WIN32)
inline std::string getProcessName(const int pid = detail::getPid()) {
    if (std::ifstream in(std::format("/proc/{}/comm", pid), std::ios::in); in.is_open()) {
        std::string fileContent;
        std::getline(in, fileContent, '\n');
        return fileContent;
    }
    return "unknown_process";
}
#else
inline std::string getProcessName(const int /*pid*/ = -1) { return "unknown_process"; }
#endif

#if defined(_POSIX_VERSION) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__) && !defined(_WIN32)
inline void setProcessName(const std::string_view& processName, int pid = detail::getPid()) {
    std::ofstream out(std::format("/proc/{}/comm", pid), std::ios::out);
    if (!out.is_open()) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("setProcessName({},{})", processName, pid));
    }
    out << std::string{processName.cbegin(), std::min(15LU, processName.size())};
    out.close();
}
#else
inline void setProcessName(const std::string_view& /*processName*/, int /*pid*/ = -1) {}
#endif

// ---- CPU affinity (Windows) ----
#if defined(_WIN32)
namespace detail {
inline std::vector<bool> getAffinityMask(DWORD_PTR mask) {
    std::vector<bool> bitMask(std::thread::hardware_concurrency());
    for (size_t i = 0; i < bitMask.size(); i++) {
        bitMask[i] = (mask & (1ULL << i)) != 0;
    }
    return bitMask;
}

inline DWORD_PTR getAffinityMask(const std::vector<bool>& threadMap) {
    DWORD_PTR mask = 0;
    size_t nMax = std::min(threadMap.size(), static_cast<size_t>(std::thread::hardware_concurrency()));
    for (size_t i = 0; i < nMax; i++) {
        if (threadMap[i]) {
            mask |= (1ULL << i);
        }
    }
    return mask;
}
} // namespace detail

inline std::vector<bool> getThreadAffinity(thread_type auto&... thread) {
    void* handle = detail::getThreadHandle(thread...);
    if (handle == nullptr) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "getThreadAffinity(thread_type)");
    }
    // Windows has no GetThreadAffinityMask; query the process affinity as proxy
    DWORD_PTR processMask, systemMask;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), "getThreadAffinity() - GetProcessAffinityMask failed");
    }
    return detail::getAffinityMask(processMask & systemMask);
}

template<class T>
requires requires(T value) { value[0]; }
inline constexpr void setThreadAffinity(const T& threadMap, thread_type auto&... thread) {
    void* handle = detail::getThreadHandle(thread...);
    if (handle == nullptr) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "setThreadAffinity(thread_type)");
    }
    std::vector<bool> map;
    if constexpr (std::is_same_v<std::decay_t<T>, std::vector<bool>>) {
        map = threadMap;
    } else {
        map.assign(std::begin(threadMap), std::end(threadMap));
    }
    DWORD_PTR mask = detail::getAffinityMask(map);
    if (SetThreadAffinityMask(static_cast<HANDLE>(handle), mask) == 0) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), "setThreadAffinity() - SetThreadAffinityMask failed");
    }
}

inline std::vector<bool> getProcessAffinity(const int /*pid*/ = -1) {
    DWORD_PTR processMask, systemMask;
    if (!GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), "getProcessAffinity() - GetProcessAffinityMask failed");
    }
    return detail::getAffinityMask(processMask);
}

template<class T>
requires requires(T value) { std::get<0>(value); }
inline constexpr bool setProcessAffinity(const T& threadMap, const int /*pid*/ = -1) {
    std::vector<bool> map;
    if constexpr (std::is_same_v<std::decay_t<T>, std::vector<bool>>) {
        map = threadMap;
    } else {
        map.assign(std::begin(threadMap), std::end(threadMap));
    }
    DWORD_PTR mask = detail::getAffinityMask(map);
    if (!SetProcessAffinityMask(GetCurrentProcess(), mask)) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), "setProcessAffinity() - SetProcessAffinityMask failed");
    }
    return true;
}

#elif defined(_POSIX_VERSION) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__)
namespace detail {
inline std::vector<bool> getAffinityMask(const cpu_set_t& cpuSet) {
    std::vector<bool> bitMask(std::min(sizeof(cpu_set_t), static_cast<size_t>(std::thread::hardware_concurrency())));
    for (size_t i = 0; i < bitMask.size(); i++) {
        bitMask[i] = CPU_ISSET(i, &cpuSet);
    }
    return bitMask;
}

template<class T>
requires requires(T value) { value[0]; }
inline constexpr cpu_set_t getAffinityMask(const T& threadMap) {
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    size_t nMax = std::min(threadMap.size(), static_cast<size_t>(std::thread::hardware_concurrency()));
    for (size_t i = 0; i < nMax; i++) {
        if (threadMap[i]) {
            CPU_SET(i, &cpuSet);
        } else {
            CPU_CLR(i, &cpuSet);
        }
    }
    return cpuSet;
}
} // namespace detail

inline std::vector<bool> getThreadAffinity(thread_type auto&... thread) {
    const pthread_t handle = detail::getPosixHandler(thread...);
    if (handle == 0U) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("getThreadAffinity(thread_type)"));
    }
    cpu_set_t cpuSet;
    if (int rc = pthread_getaffinity_np(handle, sizeof(cpu_set_t), &cpuSet); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("getThreadAffinity({})", detail::getThreadName(handle)));
    }
    return detail::getAffinityMask(cpuSet);
}

template<class T>
requires requires(T value) { value[0]; }
inline constexpr void setThreadAffinity(const T& threadMap, thread_type auto&... thread) {
    const pthread_t handle = detail::getPosixHandler(thread...);
    if (handle == 0U) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("setThreadAffinity(std::vector<bool, {}> = {{{}}}, thread_type)", threadMap.size(), gr::join(threadMap, ", ")));
    }
    cpu_set_t cpuSet = detail::getAffinityMask(threadMap);
    if (int rc = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuSet); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("setThreadAffinity(std::vector<bool, {}> = {{{}}}, {})", threadMap.size(), gr::join(threadMap, ", "), detail::getThreadName(handle)));
    }
}

inline std::vector<bool> getProcessAffinity(const int pid = detail::getPid()) {
    if (pid <= 0) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("getProcessAffinity({}) -- invalid pid", pid));
    }
    cpu_set_t cpuSet;
    if (int rc = sched_getaffinity(pid, sizeof(cpu_set_t), &cpuSet); rc != 0) {
        const std::vector<bool> mask = detail::getAffinityMask(cpuSet);
        throw std::system_error(rc, thread_exception(), std::format("getProcessAffinity({}> = {}", pid, mask));
    }
    return detail::getAffinityMask(cpuSet);
}

template<class T>
requires requires(T value) { std::get<0>(value); }
inline constexpr bool setProcessAffinity(const T& threadMap, const int pid = detail::getPid()) {
    if (pid <= 0) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("setProcessAffinity(std::vector<bool, {}> = {{{}}}, {})", threadMap.size(), gr::join(threadMap, ", "), pid));
    }
    cpu_set_t cpuSet = detail::getAffinityMask(threadMap);
    if (int rc = sched_setaffinity(pid, sizeof(cpu_set_t), &cpuSet); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("setProcessAffinity(std::vector<bool, {}> = {{{}}}, {})", threadMap.size(), gr::join(threadMap, ", "), pid));
    }
    return true;
}
#else
std::vector<bool> getThreadAffinity(thread_type auto&...) {
    return std::vector<bool>(std::thread::hardware_concurrency()); // cannot set affinity for non-posix threads
}

template<class T>
requires requires(T value) { value[0]; }
constexpr bool setThreadAffinity(const T& /*threadMap*/, thread_type auto&...) {
    return false; // cannot set affinity for non-posix threads
}

inline std::vector<bool> getProcessAffinity(const int /*pid*/ = -1) {
    return std::vector<bool>(std::thread::hardware_concurrency()); // cannot set affinity for non-posix threads
}

template<class T>
requires requires(T value) { std::get<0>(value); }
inline constexpr bool setProcessAffinity(const T& /*threadMap*/, const int /*pid*/ = -1) {
    return false; // cannot set affinity for non-posix threads
}
#endif

enum Policy { UNKNOWN = -1, OTHER = 0, FIFO = 1, ROUND_ROBIN = 2 };
} // namespace gr::thread_pool::thread

template<>
struct std::formatter<gr::thread_pool::thread::Policy> {
    using Policy = gr::thread_pool::thread::Policy;

    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(Policy policy, FormatContext& ctx) const {
        std::string policy_name;
        switch (policy) {
        case Policy::UNKNOWN: policy_name = "UNKNOWN"; break;
        case Policy::OTHER: policy_name = "OTHER"; break;
        case Policy::FIFO: policy_name = "FIFO"; break;
        case Policy::ROUND_ROBIN: policy_name = "ROUND_ROBIN"; break;
        default: policy_name = "INVALID_POLICY"; break;
        }
        return std::format_to(ctx.out(), "{}", policy_name);
    }
};

namespace gr::thread_pool::thread {

struct SchedulingParameter {
    Policy policy; // e.g. SCHED_OTHER, SCHED_RR, FSCHED_FIFO
    int    priority;
};

namespace detail {
inline Policy getEnumPolicy(const int policy) {
    switch (policy) {
#if defined(_WIN32)
    case THREAD_PRIORITY_LOWEST: return Policy::OTHER;
    case THREAD_PRIORITY_BELOW_NORMAL: return Policy::OTHER;
    case THREAD_PRIORITY_NORMAL: return Policy::OTHER;
    case THREAD_PRIORITY_ABOVE_NORMAL: return Policy::OTHER;
    case THREAD_PRIORITY_HIGHEST: return Policy::OTHER;
    case THREAD_PRIORITY_TIME_CRITICAL: return Policy::FIFO;
    case THREAD_PRIORITY_IDLE: return Policy::OTHER;
#elif defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__) && !defined(_WIN32)
    case SCHED_FIFO: return Policy::FIFO;
    case SCHED_RR: return Policy::ROUND_ROBIN;
    case SCHED_OTHER: return Policy::OTHER;
#endif
    default: return Policy::UNKNOWN;
    }
}
} // namespace detail

// ---- Scheduling parameters (Windows) ----
#if defined(_WIN32)
inline struct SchedulingParameter getThreadSchedulingParameter(thread_type auto&... thread) {
    void* handle = detail::getThreadHandle(thread...);
    if (handle == nullptr) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "getThreadSchedulingParameter(thread_type)");
    }
    int priority = GetThreadPriority(static_cast<HANDLE>(handle));
    if (priority == THREAD_PRIORITY_ERROR_RETURN) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), "getThreadSchedulingParameter() - GetThreadPriority failed");
    }
    return SchedulingParameter{.policy = detail::getEnumPolicy(priority), .priority = priority};
}

inline void setThreadSchedulingParameter(Policy scheduler, int priority, thread_type auto&... thread) {
    void* handle = detail::getThreadHandle(thread...);
    if (handle == nullptr) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), "setThreadSchedulingParameter(thread_type)");
    }
    // Map Policy to Windows priority class
    int winPriority;
    switch (scheduler) {
    case Policy::FIFO:
    case Policy::ROUND_ROBIN:
        winPriority = THREAD_PRIORITY_TIME_CRITICAL;
        break;
    default:
        // Clamp priority to valid Windows range (-2 to 2 for normal, -15 to 15 with special classes)
        winPriority = std::clamp(priority, THREAD_PRIORITY_IDLE, THREAD_PRIORITY_TIME_CRITICAL);
        break;
    }
    if (!SetThreadPriority(static_cast<HANDLE>(handle), winPriority)) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), std::format("setThreadSchedulingParameter({}, {}) - SetThreadPriority failed", scheduler, priority));
    }
}

inline struct SchedulingParameter getProcessSchedulingParameter(const int /*pid*/ = -1) {
    DWORD priorityClass = GetPriorityClass(GetCurrentProcess());
    int winPriority = BELOW_NORMAL_PRIORITY_CLASS; // default
    switch (priorityClass) {
    case IDLE_PRIORITY_CLASS:         winPriority = THREAD_PRIORITY_IDLE; break;
    case BELOW_NORMAL_PRIORITY_CLASS: winPriority = THREAD_PRIORITY_BELOW_NORMAL; break;
    case NORMAL_PRIORITY_CLASS:       winPriority = THREAD_PRIORITY_NORMAL; break;
    case ABOVE_NORMAL_PRIORITY_CLASS: winPriority = THREAD_PRIORITY_ABOVE_NORMAL; break;
    case HIGH_PRIORITY_CLASS:         winPriority = THREAD_PRIORITY_HIGHEST; break;
    case REALTIME_PRIORITY_CLASS:     winPriority = THREAD_PRIORITY_TIME_CRITICAL; break;
    }
    return SchedulingParameter{.policy = detail::getEnumPolicy(winPriority), .priority = winPriority};
}

inline void setProcessSchedulingParameter(Policy scheduler, int priority, const int /*pid*/ = -1) {
    DWORD priorityClass;
    switch (scheduler) {
    case Policy::FIFO:
    case Policy::ROUND_ROBIN:
        priorityClass = REALTIME_PRIORITY_CLASS;
        break;
    default:
        if (priority <= THREAD_PRIORITY_IDLE) priorityClass = IDLE_PRIORITY_CLASS;
        else if (priority <= THREAD_PRIORITY_LOWEST) priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
        else if (priority <= THREAD_PRIORITY_NORMAL) priorityClass = NORMAL_PRIORITY_CLASS;
        else if (priority <= THREAD_PRIORITY_ABOVE_NORMAL) priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
        else if (priority <= THREAD_PRIORITY_HIGHEST) priorityClass = HIGH_PRIORITY_CLASS;
        else priorityClass = REALTIME_PRIORITY_CLASS;
        break;
    }
    if (!SetPriorityClass(GetCurrentProcess(), priorityClass)) {
        throw std::system_error(static_cast<int>(GetLastError()), thread_exception(), std::format("setProcessSchedulingParameter({}, {}) - SetPriorityClass failed", scheduler, priority));
    }
}

#elif defined(_POSIX_THREADS) && not defined(__EMSCRIPTEN__) && not defined(__APPLE__) && !defined(_WIN32)
inline struct SchedulingParameter getProcessSchedulingParameter(const int pid = detail::getPid()) {
    struct sched_param param;
    const int          policy = sched_getscheduler(pid);
    if (int rc = sched_getparam(pid, &param); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("getProcessSchedulingParameter({}) - sched_getparam error", pid));
    }
    return SchedulingParameter{.policy = detail::getEnumPolicy(policy), .priority = param.sched_priority};
}

inline void setProcessSchedulingParameter(Policy scheduler, int priority, const int pid = detail::getPid()) {
    if (pid <= 0) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("setProcessSchedulingParameter({}, {}, {}) -- invalid pid", scheduler, priority, pid));
    }
    const int minPriority = sched_get_priority_min(scheduler);
    const int maxPriority = sched_get_priority_max(scheduler);
    if (priority < minPriority || priority > maxPriority) {
        throw std::system_error(THREAD_VALUE_RANGE, thread_exception(), std::format("setProcessSchedulingParameter({}, {}, {}) -- requested priority out-of-range [{}, {}]", scheduler, priority, pid, minPriority, maxPriority));
    }

    struct sched_param param {
        .sched_priority = priority
    };

    if (int rc = sched_setscheduler(pid, scheduler, &param); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("setProcessSchedulingParameter({}, {}, {}) - sched_setscheduler return code: {}", scheduler, priority, pid, rc));
    }
}

inline struct SchedulingParameter getThreadSchedulingParameter(thread_type auto&... thread) {
    const pthread_t handle = detail::getPosixHandler(thread...);
    if (handle == 0U) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("getThreadSchedulingParameter(thread_type) -- invalid thread"));
    }
    struct sched_param param;
    int                policy;
    if (int rc = pthread_getschedparam(handle, &policy, &param); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("getThreadSchedulingParameter({}) - sched_getparam error", detail::getThreadName(handle)));
    }
    return {.policy = detail::getEnumPolicy(policy), .priority = param.sched_priority};
}

inline void setThreadSchedulingParameter(Policy scheduler, int priority, thread_type auto&... thread) {
    const pthread_t handle = detail::getPosixHandler(thread...);
    if (handle == 0U) {
        throw std::system_error(THREAD_UNINITIALISED, thread_exception(), std::format("setThreadSchedulingParameter({}, {}, thread_type) -- invalid thread", scheduler, priority));
    }
    const int minPriority = sched_get_priority_min(scheduler);
    const int maxPriority = sched_get_priority_max(scheduler);
    if (priority < minPriority || priority > maxPriority) {
        throw std::system_error(THREAD_VALUE_RANGE, thread_exception(), std::format("setThreadSchedulingParameter({}, {}, {}) -- requested priority out-of-range [{}, {}]", scheduler, priority, detail::getThreadName(handle), minPriority, maxPriority));
    }

    struct sched_param param {
        .sched_priority = priority
    };

    if (int rc = pthread_setschedparam(handle, scheduler, &param); rc != 0) {
        throw std::system_error(rc, thread_exception(), std::format("setThreadSchedulingParameter({}, {}, {}) - pthread_setschedparam return code: {}", scheduler, priority, detail::getThreadName(handle), rc));
    }
}
#else
inline struct SchedulingParameter getProcessSchedulingParameter(const int /*pid*/ = -1) { return {}; }
inline void setProcessSchedulingParameter(Policy /*scheduler*/, int /*priority*/, const int /*pid*/ = -1) {}
inline struct SchedulingParameter getThreadSchedulingParameter(thread_type auto&... /*thread*/) { return {}; }
inline void setThreadSchedulingParameter(Policy /*scheduler*/, int /*priority*/, thread_type auto&... /*thread*/) {}
#endif

/// retrieve getThreadLimit()
#if defined(__linux__) && !defined(__EMSCRIPTEN__) && !defined(_WIN32)
namespace detail {
inline std::size_t getUserProcessLimit() {
    struct rlimit rl;
    if (getrlimit(RLIMIT_NPROC, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) {
        return rl.rlim_cur;
    }
    return 50000UZ; // fallback
}

inline std::size_t getKernelThreadLimit() {
    std::ifstream file("/proc/sys/kernel/threads-max");
    std::string   line;
    if (file && std::getline(file, line)) {
        std::size_t val = 0;
        std::from_chars(line.data(), line.data() + line.size(), val);
        return val;
    }
    return 50000UZ; // fallback
}
} // namespace detail
#endif

inline std::size_t getThreadLimit() {
    static const std::size_t limit = []() -> std::size_t {
#if defined(__linux__) && !defined(__EMSCRIPTEN__) && !defined(_WIN32)
        // native Linux: use kernel/user process limits
        return std::min({detail::getUserProcessLimit(), detail::getKernelThreadLimit(), 50000UZ});
#elif defined(__EMSCRIPTEN__)
#ifdef GR_MAX_WASM_THREAD_COUNT
        // WASM: use compile-time override if defined
        return GR_MAX_WASM_THREAD_COUNT;
#else
        // WASM: query browser thread count dynamically
        return static_cast<std::size_t>(EM_ASM_INT({ return navigator.hardwareConcurrency || 2; }));
#endif
#elif defined(_WIN32) || defined(__APPLE__)
        // Windows or macOS: use default 50k fallback
        return 50000UZ;
#else
#ifdef GR_MAX_WASM_THREAD_COUNT
        // Embedded/non-POSIX: use compile-time override
        return GR_MAX_WASM_THREAD_COUNT;
#else
        // Embedded/non-POSIX: fallback to 0 (unknown concurrency)
        return 0UZ;
#endif
#endif
    }();
    return limit;
}

} // namespace gr::thread_pool::thread

#endif // THREADAFFINITY_HPP