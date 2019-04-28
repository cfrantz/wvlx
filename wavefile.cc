#include <cstdio>
#include <string>
#include <memory>

#include <gflags/gflags.h>
#include "util/logging.h"
#include "util/sound/file.h"
#include "util/sound/math.h"

DEFINE_string(out, "", "File to write to");
DEFINE_double(samplerate, 48000, "Sample rate");
DEFINE_double(frequency, 440, "Frequency");
DEFINE_double(time, 1, "Length (in seconds)");
DEFINE_string(function, "sin", "Frequency");

std::shared_ptr<sound::Channel> NewChannel(
        const std::string& function,
        double freq, double time, double rate) {
    size_t samples = time*rate;
    auto channel = std::make_shared<sound::Channel>(samples, rate);
    float* data = channel->data();

    for(size_t i=0; i<samples; ++i, ++data) {
        double theta = i * freq * tau / rate;
        if (function == "sin") {
            *data = sin(theta);
        } else if (function == "cos") {
            *data = cos(theta);
        } else if (function == "sqr") {
            *data = sound::Square(theta);
        } else if (function == "saw") {
            *data = sound::Saw(theta);
        } else if (function == "tri") {
            *data = sound::Triangle(theta);
        } else {
            return nullptr;
        }
    }
    return channel;
}


const char kUsage[] =
R"ZZZ(<optional flags>

Description:
  Generate a wav file with a simple waveform.
)ZZZ";

int main(int argc, char *argv[]) {
    gflags::SetUsageMessage(kUsage);
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto channel = NewChannel(
            FLAGS_function, FLAGS_frequency, FLAGS_time, FLAGS_samplerate);
    sound::File sf;
    sf.add_channel(channel);
    auto sts = sf.Save(FLAGS_out);
    LOG(INFO, "Save status = ", sts.ToString());

    return 0;
}
