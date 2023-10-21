#ifndef WVLX_AUDIO_FFT_CHANNEL_H
#define WVLX_AUDIO_FFT_CHANNEL_H
#include <fftw3.h>

#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "proto/sound.pb.h"
#include "proto/wvx.pb.h"

namespace wvx {
class FFTChannel {
  public:
    using Fragment = std::vector<fftwf_complex>;
    enum WindowFn {
        RECTANGULAR,
        BLACKMAN,
        BLACKMAN_HARRIS,
        GAUSSIAN,
    };

    FFTChannel(proto::FFTCache* cache, bool take_ownership = false) {
        Init(cache->fftsz(), WindowFn::RECTANGULAR, cache);
        owning_ = take_ownership;
    }
    FFTChannel(int fftsz, WindowFn wf) { Init(fftsz, wf, nullptr); }
    FFTChannel() : FFTChannel(4096, WindowFn::BLACKMAN_HARRIS) {}
    ~FFTChannel();

    void Analyze(const sound::proto::File& file, int chan);

    const proto::Fragment& at(double tm) const {
        int n = size_t(tm * rate()) / fragsz();
        return fft(n);
    }
    const proto::Fragment& fft(int i) const {
        if (i >= 0 && i < cache_->fragment_size()) {
            return cache_->fragment(i);
        } else {
            return empty_;
        }
    }

    std::pair<float, float> RawMagnitudeAt(int index, int bin) const;
    std::pair<float, float> MagnitudeAt(double tm, int bin) const;

    inline int fftsz() const { return cache_->fftsz(); }
    inline int fragsz() const { return cache_->fragsz(); }
    inline double rate() const { return cache_->rate(); }
    inline double length() const { return cache_->length(); }
    inline size_t size() const { return cache_->fragment_size(); }

    inline void set_fragsz(int f) { cache_->set_fragsz(f); }

    const proto::FFTCache& data() const { return *cache_; }
    proto::FFTCache* release() {
        owning_ = false;
        return cache_;
    };
    void replace(proto::FFTCache* c) {
        if (owning_) delete cache_;
        cache_ = c;
    }

  private:
    void Init(int fftsz, WindowFn wf, proto::FFTCache* cache);
    float Rectangular(float n);
    float Blackman(float n);
    float BlackmanHarris(float n);
    float Gaussian(float n);
    float Windowed(float samp, int wpos);

    fftwf_plan plan_;
    fftwf_complex* in_ = nullptr;
    fftwf_complex* out_ = nullptr;
    proto::FFTCache* cache_ = nullptr;
    bool owning_ = false;
    proto::Fragment empty_;
    std::vector<float> window_;
};
}  // namespace wvx

#endif  // WVLX_AUDIO_FFT_CHANNEL_H
