#include <memory>

#include "util/os.h"
#include "audio/fft_channel.h"
#include "absl/log/log.h"

namespace audio {
namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
}

float FFTChannel::Rectangular(float n) {
    return 1.0f;
}

float FFTChannel::BlackmanHarris(float n) {
    // https://en.wikipedia.org/wiki/Window_function#Blackman_window
    const float a0 = 0.35875;
    const float a1 = 0.48829;
    const float a2 = 0.14128;
    const float a3 = 0.01168;
    const float N = fftsz_ - 1;                                               
    const float k = pi*n/N;
    return a0 - a1*cosf(2.0*k) + a2*cosf(4.0*k) - a3*cosf(6.0*k);
}

float FFTChannel::Blackman(float n) {
    // https://en.wikipedia.org/wiki/Window_function#Blackman_window
    const float a0 = 7938.0/18608.0;                                             
    const float a1 = 9240.0/18608.0;                                             
    const float a2 = 1430.0/18608.0;                                             
    const float N = fftsz_ - 1;
    return a0 - a1*cosf(2.0*pi*n/N) + a2*cosf(4.0*pi*n/N);
}

float FFTChannel::Gaussian(float n) {
    const float sigma = 0.4;
    const float N2 = (fftsz_ - 1) / 2;
    const float arg = (n - N2) / (sigma * N2);
    return expf(-0.5 * arg*arg);
}

void FFTChannel::Init(void) {
    if (in_) {
        fftwf_destroy_plan(plan_);
        fftwf_free(in_);
        fftwf_free(out_);
    }
    in_ = fftwf_alloc_complex(fftsz_);
    out_ = fftwf_alloc_complex(fftsz_);
    plan_ = fftwf_plan_dft_1d(fftsz_, in_, out_, FFTW_FORWARD, FFTW_ESTIMATE);
    empty_ = std::make_unique<Fragment>(fftsz_);
    // Pre-cacluate the window so the Analyze loop can simply multiply.
    for(int i=0; i<fftsz_; ++i) {
        switch(winfn_) {
            case RECTANGULAR:
                window_[i] = Rectangular(i);
                break;
            case BLACKMAN:
                window_[i] = Blackman(i);
                break;
            case BLACKMAN_HARRIS:
                window_[i] = BlackmanHarris(i);
                break;
            case GAUSSIAN:
                window_[i] = Gaussian(i);
                break;
        }
    }
}

FFTChannel::~FFTChannel() {
    fftwf_destroy_plan(plan_);
    fftwf_free(in_);
    fftwf_free(out_);
}

std::pair<float, float> FFTChannel::RawMagnitudeAt(size_t index, size_t bin) const {
    const auto* f1 = fft(index);
    if (bin >= f1->size()) {
        return std::make_pair(-std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity());
    }
    float re = (*f1)[bin][0];
    float im = (*f1)[bin][1];
    float phase = atan2(im, re);
    const auto* f0 = fft(index - 1);
    float p0 = atan2((*f0)[bin][1], (*f0)[bin][0]);

    float oversamp = float(fftsz_) / float(fragsz_);
    float expect = float(bin) * 2.0 * pi * float(fragsz_) / float(fftsz_);
    phase = phase - p0 -expect;
    // Map deltaphase onto +/- pi.
    int qpd = phase / pi;
    if (qpd >=0) qpd += qpd&1;
    else qpd -= qpd&1;
    phase -= pi * float(qpd);
    //phase = fmodf(phase-p0-expect, pi);
    phase = phase * oversamp / (2.0 * pi);
    float freq = (rate_ / fftsz_) * (float(bin) + phase);

    float mag = sqrtf(re*re + im*im);
    return std::make_pair(mag, freq);
}

std::pair<float, float> FFTChannel::MagnitudeAt(double tm, size_t bin) const {
    size_t index = size_t(tm * rate_) / fragsz_;
    auto raw = RawMagnitudeAt(index, bin);
    float power = 20.0f * log10f(2.0f * raw.first);
    return std::make_pair(power, raw.second);
}

void FFTChannel::Analyze(const sound::Channel& channel) {
    length_ = channel.length();
    rate_ = channel.rate();
    int samples = length_ * rate_;
    int buckets = (samples + fragsz_ - 1) / fragsz_;
    
    int64_t s = -fftsz_ / 2;
    int64_t t0 = os::utime_now();
    for(int b=0; b<buckets; ++b) {
        for(int i=0; i<fftsz_; ++i) {
            in_[i][0] = window_[i] * channel.sample(s + i);
            in_[i][1] = 0;
        }
        s += fragsz_;
        fftwf_execute(plan_);
        auto f = std::make_unique<Fragment>(fftsz_);
        double scale = 1.0 / double(fftsz_);
        for(int i=0; i<fftsz_; ++i) {
            (*f)[i][0] = out_[i][0] * scale;
            (*f)[i][1] = out_[i][1] * scale;
        }
        cache_.emplace_back(std::move(f));
    }
    int64_t t1 = os::utime_now();
    LOG(INFO) << "Computed " << buckets << " FFTs in "
        << ((t1-t0)/1000) << "ms.";

}

}  // namespace
