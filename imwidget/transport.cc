#include "imwidget/transport.h"

#include "imgui.h"
#include "IconsFontAwesome.h"

void TransportWidget(struct Transport* transport) {
    if (ImGui::Button(ICON_FA_FAST_BACKWARD "##rewind")) {
        transport->time = 0;
        transport->frame_time = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STEP_BACKWARD "##back")) {
        transport->time -= 1.0;
        transport->frame_time = transport->time;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLAY "##play")) {
        transport->playing = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STEP_FORWARD "##fwd")) {
        transport->time += 1.0;
        transport->frame_time = transport->time;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FAST_FORWARD "##ffwd")) {
        transport->time += 10.0;
        transport->frame_time = transport->time;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PAUSE "##pause")) {
        transport->playing = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP "##stop")) {
        transport->playing = false;
    }
}
