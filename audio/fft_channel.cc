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
