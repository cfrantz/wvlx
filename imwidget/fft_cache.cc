#include <cmath>
#include <cstdint>
#include "imwidget/fft_cache.h"
#include "imgui.h"

namespace audio {

void FFTCache::Redraw() {
    constexpr float twothirds = 2.0/3.0;
    for(size_t i=0; i<channel_->size(); ++i) {
        const FFTChannel::Fragment* f = channel_->fft(i);
        if (i >= bitmap_.size()) {
            bitmap_.emplace_back(absl::make_unique<GLBitmap>(1, fftsz_ / 2));
        }
        auto& bm = bitmap_[i];
        for(int y=0; y<fftsz_/2; ++y) {
            float re = f->at(y)[0];
            float im = f->at(y)[1];
            float mag = 2.0 * sqrtf(re*re + im*im);

            float amp = -floor_ + 20.0f * log10f(mag);
            if (amp < 0.0) amp = 0.0;
            amp /= -floor_;

            float hue = twothirds - twothirds * amp;
            float val = 0.25f + 0.75f * amp;
            float r,g,b;
            ImGui::ColorConvertHSVtoRGB(hue, 1.0f, val, r, g, b);
            uint32_t color = uint8_t(r * 255.0) << 0 |
                             uint8_t(g * 255.0) << 8 |
                             uint8_t(b * 255.0) << 16 |
                             0xFF000000;
            bm->SetPixel(0, y, color);
        }
        bm->Update();
    }
}

}  // namespace audio
