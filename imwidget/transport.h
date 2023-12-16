#ifndef WVLX_IMWIDGET_TRANSPORT_H
#define WVLX_IMWIDGET_TRANSPORT_H
#include "imgui.h"

struct Transport {
    bool playing;
    float speed;
    double frame_time;
    double time;
};

void TransportWidget(struct Transport* transport);

#endif  // WVLX_IMWIDGET_TRANSPORT_H
