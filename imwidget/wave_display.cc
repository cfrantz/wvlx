#include <memory>
#include "imwidget/wave_display.h"

#include "imgui.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"
#include "util/sound/file.h"

using sound::Channel;

void WaveDisplay(const char* label, std::shared_ptr<Channel> channel,
                 double time0, double time1, ImVec2 graph_size) {
    static ImVec2 ticksize = ImGui::CalcTextSize("00:00.000", nullptr, true);
    static const float divisors[] = {500, 200, 100, 50, 20, 10, 5, 2, 1};
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    if (graph_size.x == 0.0f)
        //graph_size.x = ImGui::CalcItemWidth();
        graph_size.x = ImGui::GetContentRegionAvailWidth();
    if (graph_size.y == 0.0f)
        graph_size.y = label_size.y + ticksize.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    ImGui::ItemSize(frame_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, 0, nullptr))
        return;
    const bool hovered = ImGui::ItemHoverable(inner_bb, 0);

    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max,
            ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    const ImU32 color = ImGui::GetColorU32(hovered ? ImGuiCol_PlotLinesHovered
                                                   : ImGuiCol_PlotLines);
    double t0 = channel->length() * time0;
    double t1 = channel->length() * time1;
    double ts = (t1 - t0) / (inner_bb.Max.x - inner_bb.Min.x);
    float width = inner_bb.Max.x - inner_bb.Min.x;
    float mid = (inner_bb.Max.y - inner_bb.Min.y) / 2.0;
    float hh = mid - label_size.y - ticksize.y;

    float y0 = channel->at(t0);
    ImVec2 pos0 = inner_bb.Min + ImVec2(0, mid + hh*y0);
    double t = t0 + ts;
    for(float x=1; x<width; x+=1.0f, t+=ts) {
        float y1 = channel->at(t);
        ImVec2 pos1 = inner_bb.Min + ImVec2(x, mid + hh*y1);
        window->DrawList->AddLine(pos0, pos1, color);
        y0 = y1;
        pos0 = pos1;
    }
    window->DrawList->AddLine(inner_bb.Min + ImVec2(0, mid),
                              inner_bb.Min + ImVec2(width, mid), 0xFFFFFFFF);
    ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y),
                      frame_bb.Max, label, nullptr, nullptr, ImVec2(0.5f,0.0f));

    int d = 0;
    while(divisors[d] > 1 && width / divisors[d] < ticksize.x * 1.7f) {
        ++d;
    }
    float ww = width / divisors[d];
    float bot = inner_bb.Max.y - inner_bb.Min.y - ticksize.y;
    window->DrawList->AddLine(inner_bb.Min + ImVec2(0, bot - 1),
                              inner_bb.Min + ImVec2(width, bot - 1), 0xFFFFFFFF);

    t = t0; ts = (t1 - t0) / divisors[d];
    for(float x=0; x < width+ww/2.0f; x+=ww, t+=ts) {
        char buf[32];
        float m = int(t / 60.0);
        float s = t - 60.0*m;
        snprintf(buf, sizeof(buf),
                s < 10.0f ?  "%02d:0%2.3f" : "%02d:%2.3f", int(m), s);
        ImVec2 txtsz = ImGui::CalcTextSize(buf, nullptr, true);
        if (x == 0.0) {
            txtsz.x = 0;
        } else if (x < width - ww/2.0f) {
            txtsz.x *= 0.5f;
        }
        ImGui::RenderText(inner_bb.Min + ImVec2(x - txtsz.x, bot), buf, nullptr, false);
        window->DrawList->AddLine(inner_bb.Min + ImVec2(x, bot + 1),
                                  inner_bb.Min + ImVec2(x, bot - 2), 0xFFFFFFFF);
    }
}

void WaveDisplay2(const char* label, std::shared_ptr<Channel> channel,
                 double *time0, double *zoom,
                 Transport* transport,
                 ImVec2 graph_size) {
    static ImVec2 ticksize = ImGui::CalcTextSize("00:00.000", nullptr, true);
    static ImVec2 zoomsize = ImGui::CalcTextSize("Zoom", nullptr, true);
    static const float divisors[] = {500, 200, 100, 50, 20, 10, 5, 2, 1};
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    if (graph_size.x == 0.0f)
        //graph_size.x = ImGui::CalcItemWidth();
        graph_size.x = ImGui::GetContentRegionAvailWidth();
    if (graph_size.y == 0.0f)
        graph_size.y = label_size.y + ticksize.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    ImGui::ItemSize(frame_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, 0, nullptr))
        return;
    const bool hovered = ImGui::ItemHoverable(inner_bb, 0);

    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max,
            ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    const ImU32 color = ImGui::GetColorU32(hovered ? ImGuiCol_PlotLinesHovered
                                                   : ImGuiCol_PlotLines);
    double izoom = 1.0 / *zoom;
    double length = channel->length() * izoom;
    double playhead = -1;
    double t0 = *time0;
    if (transport) {
        playhead = transport->frame_time;
        if (transport->playing) {
            t0 = playhead - length / 2;
            if (t0 < 0) t0 = 0;
            *time0 = t0;
        }
    }
    double t1 = t0 + length;
    if (t1 > channel->length()) {
        t0 -= (t1 - channel->length());
        t1 = channel->length();
        if (t0 < 0) {
            t0 = 0;
            *zoom = izoom = 1.0;
        }
        *time0 = t0;
    }
    double ts = (t1 - t0) / (inner_bb.Max.x - inner_bb.Min.x);
    float width = inner_bb.Max.x - inner_bb.Min.x;
    float mid = (inner_bb.Max.y - inner_bb.Min.y) / 2.0;
    float hh = mid - label_size.y - ticksize.y;

    float y0 = channel->at(t0);
    ImVec2 pos0 = inner_bb.Min + ImVec2(0, mid + hh*y0);
    double t = t0 + ts;
    for(float x=1; x<width; x+=1.0f, t+=ts) {
        float y1 = channel->at(t);
        ImVec2 pos1 = inner_bb.Min + ImVec2(x, mid + hh*y1);
        window->DrawList->AddLine(pos0, pos1, color);
        y0 = y1;
        pos0 = pos1;
        if (t <= playhead && (t+ts) > playhead) {
            window->DrawList->AddLine(inner_bb.Min + ImVec2(x, mid-hh),
                                      inner_bb.Min + ImVec2(x, mid+hh),
                                      0xFF0000FF);
        }
    }
    window->DrawList->AddLine(inner_bb.Min + ImVec2(0, mid),
                              inner_bb.Min + ImVec2(width, mid), 0xFFFFFFFF);
    ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y),
                      frame_bb.Max, label, nullptr, nullptr, ImVec2(0.5f,0.0f));

    int d = 0;
    while(divisors[d] > 1 && width / divisors[d] < ticksize.x * 1.7f) {
        ++d;
    }
    float ww = width / divisors[d];
    float bot = inner_bb.Max.y - inner_bb.Min.y - ticksize.y;
    window->DrawList->AddLine(inner_bb.Min + ImVec2(0, bot - 1),
                              inner_bb.Min + ImVec2(width, bot - 1), 0xFFFFFFFF);

    t = t0; ts = (t1 - t0) / divisors[d];
    for(float x=0; x < width+ww/2.0f; x+=ww, t+=ts) {
        char buf[32];
        float m = int(t / 60.0);
        float s = t - 60.0*m;
        snprintf(buf, sizeof(buf),
                s < 10.0f ?  "%02d:0%2.3f" : "%02d:%2.3f", int(m), s);
        ImVec2 txtsz = ImGui::CalcTextSize(buf, nullptr, true);
        if (x == 0.0) {
            txtsz.x = 0;
        } else if (x < width - ww/2.0f) {
            txtsz.x *= 0.5f;
        }
        ImGui::RenderText(inner_bb.Min + ImVec2(x - txtsz.x, bot), buf, nullptr, false);
        window->DrawList->AddLine(inner_bb.Min + ImVec2(x, bot + 1),
                                  inner_bb.Min + ImVec2(x, bot - 2), 0xFFFFFFFF);
    }
    ImGui::PushID(channel.get());
    ImGui::PushItemWidth(100);
    ImGui::InputDouble("Zoom", zoom, 1.0, 10.0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    double vmin = 0.0;
    double vmax = channel->length() - length;
    ImGui::PushItemWidth(graph_size.x - (100 + zoomsize.x + style.FramePadding.x * 3.0));
    ImGui::SliderScalar("##Scroll", ImGuiDataType_Double, time0, &vmin, &vmax);
    ImGui::PopItemWidth();
    ImGui::PopID();
}

