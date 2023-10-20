#ifndef WVLX2_AUDIO_FFT_IMAGE_CACHE_H
#define WVLX2_AUDIO_FFT_IMAGE_CACHE_H
#include <memory>
#include <vector>

#include "audio/fft_channel.h"
#include "implot.h"
#include "imwidget/glbitmap.h"

namespace wvx {

class FFTImageCache {
  public:
    enum Style {
        Linear,
        Logarithmic,
    };
    FFTImageCache() : channel_(nullptr), style_(Style::Linear) {}

    FFTImageCache(const FFTChannel* channel, Style style = Style::Linear)
        : channel_(channel), style_(style), bitmap_(channel->size()) {}

    ~FFTImageCache() {}

    std::pair<GLBitmap*, int> at(double tm) {
        size_t n = size_t(tm * channel_->rate()) / channel_->fragsz();
        return bitmap(n);
    }

    std::pair<GLBitmap*, int> bitmap(size_t index);

    void set_channel(const FFTChannel* channel) {
        channel_ = channel;
        clear();
    }

    void set_colormap(ImPlotColormap cmap, bool invert = false) {
        cmap_ = cmap;
        invert_ = invert;
        clear();
    }

    void set_style(Style style) {
        style_ = style;
        clear();
    }

    void set_floor(float floor) {
        floor_ = floor;
        clear();
    }
    float floor() const { return floor_; }

  private:
    GLBitmap* PlotLinear(size_t index);
    GLBitmap* PlotLogarithmic(size_t index);

    void clear() {
        bitmap_.clear();
        bitmap_.resize(channel_->size());
    }
    const FFTChannel* channel_;
    Style style_;
    std::vector<std::unique_ptr<GLBitmap>> bitmap_;
    float floor_ = -50.0f;
    ImPlotColormap cmap_ = ImPlotColormap_Spectral;
    bool invert_ = true;
};
}  // namespace wvx
#endif  // WVLX2_AUDIO_FFT_IMAGE_CACHE_H
