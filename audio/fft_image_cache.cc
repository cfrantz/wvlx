#include "audio/fft_image_cache.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "implot.h"
#include "implot_internal.h"

ABSL_DECLARE_FLAG(double, A4);

namespace wvx {
namespace {
constexpr double LOG2 = std::log(2.0);
}

std::pair<GLBitmap*, int> FFTImageCache::bitmap(size_t index) {
    if (index >= bitmap_.size()) {
        // Beyond bounds, no image.
        return std::make_pair(nullptr, 0);
    }
    if (bitmap_[index]) {
        // Already have an image, return it.
        return std::make_pair(bitmap_[index].get(), 0);
    }

    // No current image, so create one.
    switch (style_) {
        case Style::Linear:
            return std::make_pair(PlotLinear(index), 1);
        case Style::Logarithmic:
            return std::make_pair(PlotLogarithmic(index), 1);
    }
    return std::make_pair(nullptr, 0);
}

GLBitmap* FFTImageCache::PlotLinear(size_t index) {
    auto& fragment = channel_->fft(index);
    int height = channel_->fftsz() / 2;
    bitmap_[index] = std::make_unique<GLBitmap>(1, height, nullptr, true);
    auto* bm = bitmap_[index].get();

    int n = height - 1;
    for (int y = 0; y < height; ++y) {
        float re = fragment.re(y);
        float im = fragment.im(y);
        float mag = 20.0f * log10f(2.0 * sqrtf(re * re + im * im));
        mag = std::max(0.0f, -floor_ + mag) / -floor_;
        uint32_t color = 0;
        if (mag > 0.0) {
            color = ImPlot::SampleColormapU32(invert_ ? 1.0 - mag : mag, cmap_);
        }
        bm->SetPixel(0, n - y, color);
    }
    bm->Update();
    return bm;
}

GLBitmap* FFTImageCache::PlotLogarithmic(size_t index) {
    constexpr int height = 640;
    bitmap_[index] = std::make_unique<GLBitmap>(1, height, nullptr, true);
    auto* bm = bitmap_[index].get();
    int fft_bins = channel_->fftsz() / 2;
    double cm1 = (absl::GetFlag(FLAGS_A4) / 16.0) * 0.5946;
    float bucket[height] = {
        0.0,
    };

    // Re-bin the FFT into midi note buckets (with 5 divisions per note)
    for (int y = 0; y < fft_bins; ++y) {
        float mag, freq;
        std::tie(mag, freq) = channel_->RawMagnitudeAt(index, y);
        double cents_from_cm1 = 1200.0 * (std::log(freq / cm1) / LOG2);
        int n = std::max(0.0, cents_from_cm1 / 20.0);
        if (n >= height) {
            // Frequency outside of midi note range.
            continue;
        }
        bucket[n] += mag;
    }

    // Create an image from the re-binned FFT.
    int n = height - 1;
    for (int y = 0; y < height; ++y) {
        float mag = 20.0f * log10f(2.0 * bucket[y]);
        mag = std::max(0.0f, -floor_ + mag) / -floor_;
        uint32_t color = 0;
        if (mag > 0.0) {
            color = ImPlot::SampleColormapU32(invert_ ? 1.0 - mag : mag, cmap_);
        }
        bm->SetPixel(0, n - y, color);
    }
    bm->Update();
    return bm;
}

}  // namespace wvx
