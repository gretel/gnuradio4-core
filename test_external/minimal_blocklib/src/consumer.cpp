#include <gnuradio-4.0/Block.hpp>
#include <minimal_blocklib/Gain.hpp>

int main() {
    gr::minimal_blocklib::Gain<float> gain;
    gain.gain = 2.0F;
    return gain.processOne(3.0F) == 6.0F ? 0 : 1;
}
