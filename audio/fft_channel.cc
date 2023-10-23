#include "audio/fft_channel.h"

#include <memory>

#include "absl/log/log.h"
#include "util/os.h"

namespace wvx {
namespace {
constexpr double pi = 3.14159265358979323846264338327950288;
}

float FFTChannel::Rectangular(float n) { return 1.0f; }

float FFTChannel::BlackmanHarris(float n) {
    // https://en.wikipedia.org/wiki/Window_function#Blackman_window
    const float a0 = 0.35875;
    const float a1 = 0.48829;
    const float a2 = 0.14128;
    const float a3 = 0.01168;
    const float N = fftsz() - 1;
    const float k = pi * n / N;
    return a0 - a1 * cosf(2.0 * k) + a2 * cosf(4.0 * k) - a3 * cosf(6.0 * k);
}

float FFTChannel::Blackman(float n) {
    // https://en.wikipedia.org/wiki/Window_function#Blackman_window
    const float a0 = 7938.0 / 18608.0;
    const float a1 = 9240.0 / 18608.0;
    const float a2 = 1430.0 / 18608.0;
    const float N = fftsz() - 1;
    return a0 - a1 * cosf(2.0 * pi * n / N) + a2 * cosf(4.0 * pi * n / N);
}

float FFTChannel::Gaussian(float n) {
    const float sigma = 0.4;
    const float N2 = (fftsz() - 1) / 2;
    const float arg = (n - N2) / (sigma * N2);
    return expf(-0.5 * arg * arg);
}

void FFTChannel::Init(int fftsz, WindowFn wf, proto::FFTCache* cache) {
    if (cache == nullptr) {
        cache_ = new proto::FFTCache;
        owning_ = true;
        cache_->set_fftsz(fftsz);
        cache_->set_fragsz(fftsz);
    } else {
        cache_ = cache;
    }
    if (in_) {
        fftwf_destroy_plan(plan_);
        fftwf_free(in_);
        fftwf_free(out_);
    }
    in_ = fftwf_alloc_complex(cache_->fftsz());
    out_ = fftwf_alloc_complex(cache_->fftsz());
    plan_ = fftwf_plan_dft_1d(cache_->fftsz(), in_, out_, FFTW_FORWARD,
                              FFTW_ESTIMATE);

    // Fill the empty fragment.
    empty_.Clear();
    empty_.mutable_re()->Resize(cache_->fftsz() / 2, 0.0f);
    empty_.mutable_im()->Resize(cache_->fftsz() / 2, 0.0f);

    // Pre-cacluate the window so the Analyze loop can simply multiply.
    window_.resize(cache_->fftsz());
    for (int i = 0; i < cache_->fftsz(); ++i) {
        switch (wf) {
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
            default:
                LOG(FATAL) << "Bad window function: " << wf;
        }
    }
    LOG(INFO) << "Initialized FFTCache for fftsz " << cache_->fftsz()
              << " and wf=" << wf;
}

FFTChannel::~FFTChannel() {
    if (in_) {
        // Why does the library segfault here?
        //fftwf_destroy_plan(plan_);
        //fftwf_free(in_);
        //fftwf_free(out_);
    }
}

std::pair<float, float> FFTChannel::RawMagnitudeAt(int index, int bin) const {
    const auto& f1 = fft(index);
    if (bin < 0 || bin >= f1.re_size()) {
        return std::make_pair(-std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity());
    }
    float re = f1.re(bin);
    float im = f1.im(bin);
    float phase = atan2(im, re);
    const auto& f0 = fft(index - 1);
    float p0 = atan2(f0.im(bin), f0.re(bin));

    float oversamp = float(cache_->fftsz()) / float(cache_->fragsz());
    float expect = float(bin) * 2.0 * pi * float(cache_->fragsz()) /
                   float(cache_->fftsz());
    phase = phase - p0 - expect;
    // Map deltaphase onto +/- pi.
    int qpd = phase / pi;
    if (qpd >= 0)
        qpd += qpd & 1;
    else
        qpd -= qpd & 1;
    phase -= pi * float(qpd);
    // phase = fmodf(phase-p0-expect, pi);
    phase = phase * oversamp / (2.0 * pi);
    float freq = (cache_->rate() / cache_->fftsz()) * (float(bin) + phase);

    float mag = sqrtf(re * re + im * im);
    return std::make_pair(mag, freq);
}

std::pair<float, float> FFTChannel::MagnitudeAt(double tm, int bin) const {
    size_t index = size_t(tm * cache_->rate()) / cache_->fragsz();
    auto raw = RawMagnitudeAt(index, bin);
    float power = 20.0f * log10f(2.0f * raw.first);
    return std::make_pair(power, raw.second);
}

static inline float sample(const sound::proto::Samples& channel, int index) {
    if (index < 0) {
        return -sample(channel, -index);
    } else if (index >= channel.data_size()) {
        index = (channel.data_size() - 1) - (index - channel.data_size());
        return -sample(channel, index);
    } else {
        return channel.data(index);
    }
}

void FFTChannel::Analyze(const sound::proto::File& file, int chan) {
    const auto& channel = file.channel(chan);
    int size = channel.data_size();
    cache_->set_rate(file.rate());
    cache_->set_length(file.rate() * size);

    int32_t fragsz = cache_->fragsz();
    int buckets = (size + fragsz - 1) / fragsz;
    cache_->clear_fragment();

    int s = -cache_->fftsz() / 2;
    int64_t t0 = os::utime_now();
    for (int b = 0; b < buckets; ++b) {
        for (int i = 0; i < cache_->fftsz(); ++i) {
            in_[i][0] = window_[i] * sample(channel, s + i);
            in_[i][1] = 0;
        }
        s += fragsz;
        fftwf_execute(plan_);
        auto* frag = cache_->add_fragment();
        double scale = 1.0 / double(cache_->fftsz());
        // We only capture lower half of the buckets because the upper half is
        // negative frequencies.
        for (int i = 0; i < cache_->fftsz() / 2; ++i) {
            frag->add_re(out_[i][0] * scale);
            frag->add_im(out_[i][1] * scale);
        }
    }
    int64_t t1 = os::utime_now();
    LOG(INFO) << "Computed " << buckets << " FFTs in " << ((t1 - t0) / 1000)
              << "ms.";
}

}  // namespace wvx
