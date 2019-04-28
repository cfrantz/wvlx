#ifndef WVLX_IMWIDGET_WAVE_DISPLAY_H
#define WVLX_IMWIDGET_WAVE_DISPLAY_H
#include <memory>
#include "imgui.h"
#include "util/sound/file.h"
#include "imwidget/transport.h"

void WaveDisplay(const char* label, std::shared_ptr<sound::Channel> channel,
                 double time0=0.0, double time1=1.0,
                 ImVec2 graph_size=ImVec2(0, 128.0f));

void WaveDisplay2(const char* label, std::shared_ptr<sound::Channel> channel,
                 double *time0, double *zoom,
                 Transport* transport=nullptr,
                 ImVec2 graph_size=ImVec2(0, 128.0f));


#endif // WVLX_IMWIDGET_WAVE_DISPLAY_H
