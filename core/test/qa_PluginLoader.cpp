#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <boost/ut.hpp>

#include <gnuradio-4.0/BlockRegistry.hpp>
#include <gnuradio-4.0/PluginLoader.hpp>

namespace ut = boost::ut;

namespace {

std::vector<std::string> pluginPathsFromEnv() {
    const char* env = std::getenv("GNURADIO4_PLUGIN_DIRECTORIES");
    if (env == nullptr) {
        return {};
    }
    return {env};
}

gr::PluginLoader makeLoader() {
    static gr::BlockRegistry     registry;
    static gr::SchedulerRegistry schedulerRegistry;
    return gr::PluginLoader(registry, schedulerRegistry, pluginPathsFromEnv());
}

} // namespace

const boost::ut::suite PluginLoaderTests = [] {
    using namespace ut;
    using namespace std::string_literals;

    auto loader = makeLoader();

    "qa_plugin block discovered and instantiable"_test = [&] {
        expect(loader.isBlockAvailable("qa_plugin::QaBlock"s)) << "qa_plugin::QaBlock not found in available blocks";
        auto block = loader.instantiate("qa_plugin::QaBlock"s);
        expect(block != nullptr) << "instantiate returned null for qa_plugin::QaBlock";
    };

    "qa_plugin scheduler discovered and instantiable"_test = [&] {
        expect(loader.isSchedulerAvailable("qa_plugin::QaScheduler"s)) << "qa_plugin::QaScheduler not found";
        auto scheduler = loader.instantiateScheduler("qa_plugin::QaScheduler"s);
        expect(scheduler != nullptr) << "instantiateScheduler returned null for qa_plugin::QaScheduler";
    };

    "bad_plugin recorded as failed"_test = [&] {
        const auto& failed = loader.failedPlugins();
        expect(!failed.empty()) << "expected at least one failed plugin (bad_plugin)";
        const auto it = std::ranges::find_if(failed, [](const auto& pair) {
            return pair.first.find("bad_plugin") != std::string::npos;
        });
        expect(it != failed.end()) << "bad_plugin not found in failedPlugins";
    };
};

int main() { return boost::ut::cfg<boost::ut::override>.run(); }