#include <gnuradio-4.0/Plugin.hpp>
#include <gnuradio-4.0/Scheduler.hpp>

GR_PLUGIN("QA Plugin", "GNU Radio Contributors", "MIT", "v1")

namespace qa_plugin {

struct QaBlock : public gr::Block<QaBlock> {
    gr::PortIn<float>  in;
    gr::PortOut<float> out;

    GR_MAKE_REFLECTABLE(QaBlock, in, out);

    [[nodiscard]] constexpr float processOne(float x) const noexcept { return x; }
};

class QaScheduler : public gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> {
public:
    explicit QaScheduler(const gr::property_map& = {}) : gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded>({}) {}
};

} // namespace qa_plugin

auto registerQaBlock =
    gr::registerBlock<qa_plugin::QaBlock>(static_cast<gr::BlockRegistry&>(grPluginInstance()));

auto registerQaScheduler =
    [](auto& registry) {
        registry.template insert<qa_plugin::QaScheduler>();
        return true;
    }(static_cast<gr::SchedulerRegistry&>(grPluginInstance()));