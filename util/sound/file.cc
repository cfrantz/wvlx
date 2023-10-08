#include "util/sound/file.h"

#include <memory>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <sndfile.h>

#include "absl/log/log.h"
#include "absl/status/status.h"

namespace sound {

constexpr float PI = 3.14159265358979323846f;
constexpr double EPSILON = 0.000001;

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

Channel Channel::ConstantValue(size_t samples, double rate, float value) {
    Channel cv = Channel(samples, rate);
    for(size_t i=0; i<samples; ++i) {
        cv.data_[i] = value;
    }
    return cv;
}


void Channel::Resample(double rate) {
    std::vector<float> data(length_ * rate);
    for (size_t i=0; i<data.size(); ++i) {
        double tm = double(i) / rate;
        data[i] = this->at(tm);
    }
    data_ = data;
    rate_ = rate;
}

float Channel::Peak(size_t s0, size_t s1) {
    float peak = 0.0;
    for(size_t i=s0; i<s1; ++i) {
        float val = fabsf(data_[i]);
        peak = std::fmaxf(peak, val);
    }
    return peak;
}

float Channel::at(double tm) const {
    if (tm < 0.0 || tm >= length_) return 0.0;
    double n = tm * rate_;
    double f = n - floor(n);
    size_t x = n;

    Interpolation i = interp_;
    if (f < EPSILON) i = Interpolation::None;
    if (i == Interpolation::Cubic && (x<1 || x > data_.size()-3)) {
        i = Interpolation::Cosine;
    }

    switch(i) {
        case Interpolation::None:
            return data_[x];
        case Interpolation::Linear:
            return interp_linear(x, f);
        case Interpolation::Cosine:
            return interp_cosine(x, f);
        case Interpolation::Cubic:
            return interp_cubic(x, f);
    }
    return 0.0f;
}

float Channel::interp_linear(size_t x, double f) const {
    float y1 = data_[x];
    float y2 = data_[x+1];
    return y1 * (1.0 - f) + y2 * f;
}

float Channel::interp_cosine(size_t x, double f) const {
    float y1 = data_[x];
    float y2 = data_[x+1];
    float mu = (1.0 - cosf(f * PI)) / 2.0;
    return y1 * (1.0 - mu) + y2 * mu;
}

float Channel::interp_cubic(size_t x, double f) const {
    float y0 = data_[x-1];
    float y1 = data_[x];
    float y2 = data_[x+1];
    float y3 = data_[x+2];

    // Cubic interpolation:                                                  
    //                                                                       
    // [(-1/2)p0 + (3/2)p1 = (3/2)p2 + (1/2)p3] * x^3                        
    // + [p0 - (5/2)p1 + 2p2 - (1/2)p3) * x^2                                
    // + [-(1/2)p0 + (1/2)p2] * x                                            
    // + p1                                                                  
    return y1 + 0.5 * f*(y2 - y0 + f*(2.0*y0 - 5.0*y1 + 4.0*y2 - y3 + f*(3.0*(y1-y2) + y3 - y0)));
}

absl::StatusOr<std::unique_ptr<File>> File::Load(const std::string& filename) {
    SF_INFO info;
    SNDFILE* fp = sf_open(filename.c_str(), SFM_READ, &info);
    if (fp == nullptr) {
        LOG(ERROR) << "File::Load could not open " << filename;
        return absl::InvalidArgumentError(sf_strerror(fp));
    }
    auto data = SFRead(fp, &info);
    sf_close(fp);

    auto file = absl::make_unique<File>();
    file->info_ = info;
    LOG(INFO) << "soundfile:"
        << " frames:" << info.frames
        << " rate:" << info.samplerate
        << " ch:" << info.channels
        << " format:" << info.format;

    for(int i=0; i<info.channels; i++) {
        file->channel_.emplace_back(new Channel(data.data()+i,
                                                info.frames,
                                                info.samplerate,
                                                info.channels));
    }
    return file;
}

absl::StatusOr<std::unique_ptr<File>> File::LoadAsMono(const std::string& filename) {
    SF_INFO info;
    SNDFILE* fp = sf_open(filename.c_str(), SFM_READ, &info);
    if (fp == nullptr) {
        LOG(ERROR) << "File::Load could not open " << filename;
        return absl::InvalidArgumentError(sf_strerror(fp));
    }
    auto data = SFRead(fp, &info);
    sf_close(fp);

    auto file = absl::make_unique<File>();
    file->info_ = info;
    LOG(INFO) << "soundfile:"
        << " frames:" << info.frames
        << " rate:" << info.samplerate
        << " ch:" << info.channels
        << " format:" << info.format;

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

absl::Status File::Save(const std::string& filename) {
    if (channel_.empty()) {
        return absl::FailedPreconditionError("no channels");
    }
    SF_INFO info{(sf_count_t)channel_.at(0)->size(),
                 (int)channel_.at(0)->rate(),
                 (int)channel_.size(),
                 SF_FORMAT_WAV | SF_FORMAT_FLOAT};
    for(const auto& c : channel_) {
        if (c->size() != (size_t)info.frames) {
            return absl::FailedPreconditionError(
                    "Not all channels the same length");
        }
        if (c->rate() != info.samplerate) {
            return absl::FailedPreconditionError(
                    "Not all channels the same samplerate");
        }
    }
    LOG(INFO) << "soundfile:"
        << " frames:" << info.frames
        << " rate:" << info.samplerate
        << " ch:" << info.channels
        << " format:" << info.format;

    LOG(INFO) << "format_check " << sf_format_check(&info);
    SNDFILE *fp = sf_open(filename.c_str(), SFM_WRITE, &info);
    if (fp == nullptr) {
        return absl::InternalError(sf_strerror(fp));
    }
    size_t frames = channel_.size() * channel_.at(0)->size();
    std::vector<float> data;
    for(size_t i=0; i<frames; ++i) {
        for(const auto& c : channel_) {
            data.emplace_back(c->sample(i));
        }
    }
    sf_write_float(fp, data.data(), data.size());
    sf_close(fp);
    return absl::OkStatus();
}

std::vector<float> File::SFRead(SNDFILE* sf, SF_INFO* info) {
    std::vector<float> data(info->frames * info->channels);
    size_t count = sf_read_float(sf, data.data(), data.size());
    if (count != data.size()) {
        LOG(ERROR) << "SFRead short read: got "
            << count << " expected " << data.size();
    }
    return data;
}

}  // namespace
