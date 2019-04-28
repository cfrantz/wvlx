#ifndef WVLX_IMWIDGET_FFT_CACHE_H
#define WVLX_IMWIDGET_FFT_CACHE_H
#include "audio/fft_channel.h"
#include "imwidget/glbitmap.h"

namespace audio {

class FFTCache {
  public:
    FFTCache(const FFTChannel* channel)
      : channel_(channel),
      fftsz_(channel->fftsz()),
      winsz_(channel->winsz()),
      fragsz_(channel->fragsz()),
      rate_(channel->rate()),
      length_(channel->length()) { Redraw(); }

    void Redraw();
    GLBitmap* at(double tm) const {
        size_t n = size_t(tm * rate_) / fragsz_;
        return bitmap(n);
        
    }
    GLBitmap* bitmap(size_t n) const {
        return n < bitmap_.size() ? bitmap_[n].get() : nullptr;
    }
    inline int fftsz() const { return fftsz_; }
    inline int winsz() const { return winsz_; }
    inline int fragsz() const { return fragsz_; }
    inline double rate() const { return rate_; }
    inline double length() const { return length_; }
    inline size_t size() { return bitmap_.size(); }

    // Return a ref so imgui can adjust it.
    inline float& gain() { return gain_; }
  private:
    const FFTChannel* channel_;
    int fftsz_;
    int winsz_;
    int fragsz_;
    double rate_ = 0;
    double length_ = 0;
    float gain_ = 1.0;
    std::vector<std::unique_ptr<GLBitmap>> bitmap_;
};

}  // namespace audio
#endif // WVLX_IMWIDGET_FFT_CACHE_H
