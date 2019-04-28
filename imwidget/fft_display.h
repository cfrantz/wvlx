#ifndef WVLX_IMWIDGET_FFT_DISPLAY_H
#define WVLX_IMWIDGET_FFT_DISPLAY_H
#include "imwidget/fft_cache.h"
#include "imwidget/transport.h"
#include "imgui.h"

void FFTDisplay(const char* label, audio::FFTCache* channel,
                double *time0, double *zoom,
                double *vzoom = nullptr,
                double *vzero = nullptr,
                Transport* transport = nullptr,
                ImVec2 graph_size=ImVec2(0, 256));

#endif // WVLX_IMWIDGET_FFT_DISPLAY_H
