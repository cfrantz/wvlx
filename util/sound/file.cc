#include "util/sound/file.h"

#include <memory>
#include <string>
#include <vector>
#include <sndfile.h>

#include "util/logging.h"
#include "util/status.h"
#include "util/statusor.h"
#include "absl/memory/memory.h"

namespace sound {

Channel::Channel(const float* data, size_t samples, double rate, size_t stride)
  : data_(samples),
  interp_(Interpolation::None),
  rate_(rate),
  length_(samples / rate) {
    for(size_t i=0; i<samples; ++i) {
        data_[i] = *data;
        data += stride;
    }
}

float Channel::at(double tm) const {
    if (tm < 0.0 || tm >= length_) return 0.0;
    double n = tm * rate_;
    switch(interp_) {
        case Interpolation::None: {
            return data_.at(int(n));
        }
        case Interpolation::Linear: {
            double f = n - floor(n);
            float sa = sample(n), sb = sample(n+1);
            return sa * (1.0 - f) + sb * f;
        }
    }
    return 0.0f;
}

std::unique_ptr<File> File::Load(const std::string& filename) {
    SF_INFO info;
    SNDFILE* fp = sf_open(filename.c_str(), SFM_READ, &info);
    if (fp == nullptr) {
        LOGF(ERROR, "File::Load could not open %s: %s",
             filename.c_str(), sf_strerror(fp));
        return nullptr;
    }
    auto data = SFRead(fp, &info);
    sf_close(fp);

    auto file = absl::make_unique<File>();
    file->info_ = info;
    LOG(INFO, "format:"
            "\n  frames:   ", info.frames,
            "\n  rate:     ", info.samplerate,
            "\n  channels: ", info.channels,
            "\n  format:   ", info.format, "\n");
    for(int i=0; i<info.channels; i++) {
        file->channel_.emplace_back(new Channel(data.data()+i,
                                                info.frames,
                                                info.samplerate,
                                                info.channels));
    }
    return file;
}

std::unique_ptr<File> File::LoadAsMono(const std::string& filename) {
    SF_INFO info;
    SNDFILE* fp = sf_open(filename.c_str(), SFM_READ, &info);
    if (fp == nullptr) {
        LOGF(ERROR, "File::Load could not open %s: %s",
             filename.c_str(), sf_strerror(fp));
        return nullptr;
    }
    auto data = SFRead(fp, &info);
    sf_close(fp);

    auto file = absl::make_unique<File>();
    file->info_ = info;
    LOG(INFO, "format:"
            "\n  frames:   ", info.frames,
            "\n  rate:     ", info.samplerate,
            "\n  channels: ", info.channels,
            "\n  format:   ", info.format, "\n");

    // Sum and average all channels into channel 0
    for(int i=0; i<info.frames; i++) {
        int k = i * info.channels;
        for(int j=1; j<info.channels; j++) {
            data[k] += data[k+j];
        }
        data[k] /= float(info.channels);
    }

    // Data still strided as channels, so tell Channel about the stride.
    file->channel_.emplace_back(new Channel(data.data(),
                                            info.frames,
                                            info.samplerate,
                                            info.channels));

    return file;
}

util::Status File::Save(const std::string& filename) {
    if (channel_.empty()) {
        return util::Status(util::error::Code::INVALID_ARGUMENT,
                "No channels");
    }
    SF_INFO info{(sf_count_t)channel_.at(0)->size(),
                 (int)channel_.at(0)->rate(),
                 (int)channel_.size(),
                 SF_FORMAT_WAV | SF_FORMAT_FLOAT};
    for(const auto& c : channel_) {
        if (c->size() != (size_t)info.frames) {
            return util::Status(util::error::Code::INVALID_ARGUMENT,
                    "Not all channels the same length");
        }
        if (c->rate() != info.samplerate) {
            return util::Status(util::error::Code::INVALID_ARGUMENT,
                    "Not all channels the same samplerate");
        }
    }
    LOG(INFO, "format:"
            "\n  frames:   ", info.frames,
            "\n  rate:     ", info.samplerate,
            "\n  channels: ", info.channels,
            "\n  format:   ", info.format, "\n");

    LOG(INFO, "format_check ", sf_format_check(&info));
    SNDFILE *fp = sf_open(filename.c_str(), SFM_WRITE, &info);
    if (fp == nullptr) {
        LOGF(ERROR, "File::Save error %s", sf_strerror(fp));
        return util::Status(util::error::Code::INTERNAL,
                sf_strerror(fp));
    }
    size_t frames = channel_.size() * channel_.at(0)->size();
    std::vector<float> data;
    for(size_t i=0; i<frames; ++i) {
        for(const auto& c : channel_) {
            data.emplace_back(c->sample(i));
        }
    }
    sf_count_t n = sf_write_float(fp, data.data(), data.size());
    LOGF(INFO, "File::Save wrote %d/%d items", data.size(),n);
    sf_close(fp);
    return util::Status::OK;
}

std::vector<float> File::SFRead(SNDFILE* sf, SF_INFO* info) {
    std::vector<float> data(info->frames * info->channels);
    size_t count = sf_read_float(sf, data.data(), data.size());
    if (count != data.size()) {
        LOGF(ERROR, "SFRead short read: got %z, expected %z",
             count, data.size());
    }
    return data;
}

}  // namespace
