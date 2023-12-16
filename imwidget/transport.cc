#include "imwidget/transport.h"

#include "imgui.h"
#include "util/fontawesome.h"

void TransportWidget(struct Transport* transport) {
    if (ImGui::Button(ICON_FA_BACKWARD_FAST "##rewind")) {
        transport->time = 0;
        transport->frame_time = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_BACKWARD_STEP "##back")) {
        transport->time -= 1.0;
        transport->frame_time = transport->time;
    }
    ImGui::SameLine();
    if (!transport->playing) {
        if (ImGui::Button(ICON_FA_PLAY "##play")) {
            transport->playing = true;
        }
    } else {
        if (ImGui::Button(ICON_FA_PAUSE "##pause")) {
            transport->playing = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FORWARD_STEP "##fwd")) {
        transport->time += 1.0;
        transport->frame_time = transport->time;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FORWARD_FAST "##ffwd")) {
        transport->time += 10.0;
        transport->frame_time = transport->time;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP "##stop")) {
        transport->playing = false;
    }
    ImGui::SameLine();
    ImGui::DragFloat("##speed", &transport->speed, 0.001, 0.5, 2.0, "%.3f");
}
