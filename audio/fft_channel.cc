#include "audio/fft_channel.h"
#include "absl/memory/memory.h"

namespace audio {
namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
}

float FFTChannel::Rectangular(float n) {
    return 1.0f;
}

float FFTChannel::Blackman(float n) {
    // https://en.wikipedia.org/wiki/Window_function#Blackman_window
    const float a0 = 7938.0/18608.0;                                             
    const float a1 = 9240.0/18608.0;                                             
    const float a2 = 1430.0/18608.0;                                             
    const float N = winsz_ - 1;                                               
    return a0 - a1*cosf(2.0*pi*n/N) + a2*cosf(4.0*pi*n/N);
}

void FFTChannel::Init(int n, int w, WindowFn wf) {
    fftsz_ = n;
    winsz_ = w ? w : n;
    winfn_ = wf;
    if (in_) {
        fftwf_destroy_plan(plan_);
        fftwf_free(in_);
        fftwf_free(out_);
    }
    in_ = fftwf_alloc_complex(fftsz_);
    out_ = fftwf_alloc_complex(fftsz_);
    plan_ = fftwf_plan_dft_1d(fftsz_, in_, out_, FFTW_FORWARD, FFTW_ESTIMATE);
    empty_ = absl::make_unique<Fragment>(fftsz_);
    correlation_ = absl::make_unique<Fragment>(fftsz_);
    if (w) MakeCorrelation();
}

FFTChannel::~FFTChannel() {
    fftwf_destroy_plan(plan_);
    fftwf_free(in_);
    fftwf_free(out_);
}

std::pair<float, float> FFTChannel::MagnitudeAt(double tm, size_t bin) const {
    size_t n = size_t(tm * rate_) / fragsz_;
    const auto* f1 = fft(n);
    if (bin >= f1->size()) {
        return std::make_pair(-std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity());
    }
    float re = (*f1)[bin][0];
    float im = (*f1)[bin][1];
    float phase = atan2(im, re);
    const auto* f0 = fft(n-1);
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

    float power = 20.0f * log10f(2.0f * sqrtf(re*re + im*im));
    return std::make_pair(power, freq);
}

float FFTChannel::Windowed(float samp, int wpos) {
    if (wpos < winsz_) {
        switch(winfn_) {
            case RECTANGULAR:
                return samp;
            case BLACKMAN:
                return samp * Blackman(wpos);
        }
    }
    return 0;
}

void FFTChannel::MakeCorrelation() {
    for(int i=0; i<fftsz_; ++i) {
        in_[i][0] = i<winsz_ ? 1.0 : 0.0;
        in_[i][1] = 0;
    }
    fftwf_execute(plan_);
    double scale = 1.0 / double(fftsz_);
    for(int i=0; i<fftsz_; ++i) {
        (*correlation_)[i][0] = out_[i][0] * scale;
        (*correlation_)[i][1] = out_[i][1] * scale;
    }
}

void FFTChannel::Analyze(const sound::Channel& channel) {
    length_ = channel.length();
    rate_ = channel.rate();
    int samples = length_ * rate_;
    int buckets = (samples + fragsz_ - 1) / fragsz_;
    
    int s = 0;
    for(int b=0; b<buckets; ++b) {
        for(int i=0; i<fftsz_; ++i) {
            in_[i][0] = Windowed(channel.sample(s + i), i);
            in_[i][1] = 0;
        }
        s += fragsz_;
        fftwf_execute(plan_);
        auto f = absl::make_unique<Fragment>(fftsz_);
        double scale = 1.0 / double(fftsz_);
        for(int i=0; i<fftsz_; ++i) {
//            (*f)[i][0] = out_[i][0] * scale;
//            (*f)[i][1] = out_[i][1] * scale;
            (*f)[i][0] = out_[i][0] * scale - (*correlation_)[i][0];
            (*f)[i][1] = out_[i][1] * scale - (*correlation_)[i][1];
        }
        cache_.emplace_back(std::move(f));
    }
}

}  // namespace
