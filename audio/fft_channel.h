#ifndef WVLX_AUDIO_FFT_CHANNEL_H
#define WVLX_AUDIO_FFT_CHANNEL_H
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#include <fftw3.h>
#include "util/sound/file.h"

namespace audio {
class FFTChannel {
  public:
    using Fragment = std::vector<fftwf_complex>;
    enum WindowFn {
        RECTANGULAR,
        BLACKMAN,
        BLACKMAN_HARRIS,
        GAUSSIAN,
    };

    FFTChannel(int fftsz, WindowFn wf)
      : fftsz_(fftsz),
      fragsz_(fftsz),
      winfn_(wf),
      window_(fftsz) { Init(); }
    FFTChannel() : FFTChannel(4096, WindowFn::BLACKMAN_HARRIS) {}
    ~FFTChannel();

    void Analyze(const sound::Channel& channel);

    const Fragment* at(double tm) const {
        size_t n = size_t(tm * rate_) / fragsz_;
        return fft(n);
    }
    const Fragment* fft(size_t n) const {
        return n < cache_.size() ? cache_.at(n).get()
                                 : empty_.get();
    }

    std::pair<float, float> RawMagnitudeAt(size_t index, size_t bin) const;
    std::pair<float, float> MagnitudeAt(double tm, size_t bin) const;

    inline int fftsz() const { return fftsz_; }
    inline int fragsz() const { return fragsz_; }
    inline double rate() const { return rate_; }
    inline double length() const { return length_; }
    inline size_t size() const { return cache_.size(); }

    inline void set_fragsz(int f) { fragsz_ = f; }

  private:
    void Init(void);
    float Rectangular(float n);
    float Blackman(float n);
    float BlackmanHarris(float n);
    float Gaussian(float n);
    float Windowed(float samp, int wpos);

    int fftsz_;
    int fragsz_;
    WindowFn winfn_;
    fftwf_plan plan_;
    fftwf_complex *in_ = nullptr;
    fftwf_complex *out_ = nullptr;
    double rate_ = 0;
    double length_ = 0;
    std::vector<float> window_;
    std::vector<std::unique_ptr<Fragment>> cache_;
    std::unique_ptr<Fragment> empty_;
};
}  // namespace

#endif // WVLX_AUDIO_FFT_CHANNEL_H
