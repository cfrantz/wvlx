#include "imwidget/fft_display.h"
#include <memory>
#include "imgui.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"
#include "util/sound/file.h"

using sound::Channel;

void FFTDisplay(const char* label, audio::FFTCache* channel,
                double *time0, double *zoom, double *vzoom,
                double *vzero,
                Transport* transport,
                ImVec2 graph_size) {
    static ImVec2 ticksize = ImGui::CalcTextSize("00:00.000", nullptr, true);
    static const float divisors[] = {500, 200, 100, 50, 20, 10, 5, 2, 1};
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    if (graph_size.x == 0.0f)
        graph_size.x = ImGui::GetContentRegionAvailWidth();
    if (graph_size.y == 0.0f)
        graph_size.y = label_size.y + ticksize.y + (style.FramePadding.y * 2);

    // Calculate bounding boxes and add item.
    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    ImGui::ItemSize(frame_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(frame_bb, 0, nullptr))
        return;

    const bool hovered = ImGui::ItemHoverable(inner_bb, 0);
    ImGui::RenderFrame(frame_bb.Min, frame_bb.Max,
            ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

    // Compute start and end based on zoom parameters
    double izoom = 1.0 / *zoom;
    double length = channel->length() * izoom;
    double t0 = *time0;
    double playhead = -1;
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
    double t = t0 + ts;
    float width = inner_bb.Max.x - inner_bb.Min.x;
    float mid = (inner_bb.Max.y - inner_bb.Min.y) / 2.0;
    float hh = mid - label_size.y - ticksize.y;

    double v0 = vzero ? *vzero : 0.0;
    double ivz = vzoom ? 1.0 / *vzoom : 1.0;
    if (v0 + ivz > 1.0) {
        v0 -= (v0+ivz) - 1.0;
        if (vzero) *vzero = v0;
    }

    const ImVec2 uvb(0.0, v0);
    const ImVec2 uva(0.0, v0 + ivz);

    ImVec2 cursor = ImGui::GetCursorPos();
    for(float x=0; x<width; x+=1.0f, t+=ts) {
        GLBitmap* bm = channel->at(t);
        if (!bm) continue;
        ImVec2 pos0 = inner_bb.Min + ImVec2(x, mid - hh);
        ImVec2 pos1 = inner_bb.Min + ImVec2(x+1, mid + hh);
        window->DrawList->AddImage(ImTextureID(bm->imtexture()), pos0, pos1, uva, uvb);
        if (t <= playhead && (t+ts) > playhead) {
            window->DrawList->AddLine(inner_bb.Min + ImVec2(x, mid-hh),
                                      inner_bb.Min + ImVec2(x, mid+hh),
                                      0xFF0000FF);
        }
    }

    if (vzero) {
        ImVec2 pos = inner_bb.Max - ImVec2(32, mid+hh);
        ImGui::SetCursorScreenPos(pos);
        double vmin = 0, vmax = 1;
        ImGui::VSliderScalar("##vert", ImVec2(32, 2*hh), ImGuiDataType_Double, vzero, &vmin, &vmax);
    }

    ImGui::SetCursorPos(cursor);
    ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y),
                      frame_bb.Max, label, nullptr, nullptr, ImVec2(0.5f,0.0f));

    // Compute the horizontal scale
    int d = 0;
    while(divisors[d] > 1 && width / divisors[d] < ticksize.x * 1.7f) {
        ++d;
    }
    float ww = width / divisors[d];
    float bot = inner_bb.Max.y - inner_bb.Min.y - ticksize.y;
    window->DrawList->AddLine(inner_bb.Min + ImVec2(0, bot - 1),
                              inner_bb.Min + ImVec2(width, bot - 1), 0xFFFFFFFF);

    // Draw the horizontal scale
    t = t0;
    float ss = (t1 - t0) / divisors[d];
    for(float x=0; x < width+ww/2.0f; x+=ww, t+=ss) {
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

    // Compute the vertical scale
    float vn = truncf(2.0*hh / ticksize.y) - 2;
    float vs = 2*hh / vn;
    double bsz = channel->rate() / double(channel->fftsz());
    double vscale = double(channel->fftsz() / 2) / (2.0*hh);
    double vz = ivz * vscale;
    bot = v0 / ivz * 2*hh;
    window->DrawList->AddLine(inner_bb.Min + ImVec2(0, mid-hh),
                              inner_bb.Min + ImVec2(0, mid+hh), 0xFFFFFFFF);
    int lastb = -1.0;
    float lasty = -100;
    for(float y=0; y<2.0*hh; y+=1) {
        if (y - lasty < vs) continue;
        int bucket = (bot + y) * vz;
        if (bucket == lastb) continue;
        char buf[32];
        //snprintf(buf, sizeof(buf), "%d Hz", int(bucket+0.5));
        snprintf(buf, sizeof(buf), "%.0f Hz", bucket*bsz);
        ImVec2 txtsz = ImGui::CalcTextSize(buf, nullptr, true);
        ImGui::RenderText(inner_bb.Min + ImVec2(8, mid+hh-y-txtsz.y/2.0), buf, nullptr, false);

        window->DrawList->AddLine(inner_bb.Min + ImVec2(0, mid+hh-y),
                                  inner_bb.Min + ImVec2(4, mid+hh-y), 0xFFFFFFFF);
        lasty = y;
        lastb = bucket;
    }

    float my = (g.IO.MousePos.y - inner_bb.Min.y);
    if (hovered && my >= mid-hh && my < mid+hh) {
        float t = t0 + (g.IO.MousePos.x - inner_bb.Min.x) * ts;
        my = (mid+hh - my);
        int bucket = (bot + my) * vz;
        auto mf = channel->fft()->MagnitudeAt(t, bucket);
        ImGui::SetTooltip("bin=%.0f Hz\nfreq=%.1f\nmag=%.2f dB",
                bucket*bsz, mf.second, mf.first);
    }

    ImGui::PushID(channel);
    ImGui::PushItemWidth(100);
    ImGui::InputDouble("Zoom", zoom, 1.0, 10.0);
    ImGui::SameLine();
    ImGui::InputDouble("VZoom", vzoom, 1.0, 10.0);
    ImGui::SameLine();
    if (ImGui::InputFloat("Floor", &channel->floor(), 1.0, 10.0)) {
        channel->Redraw();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    double vmin = 0.0;
    double vmax = channel->length() - length;
    ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
    ImGui::SliderScalar("##Scroll", ImGuiDataType_Double, time0, &vmin, &vmax);
    ImGui::PopItemWidth();
    ImGui::PopID();
}

