#include "imwidget/audio_data.h"

#include <cstdint>

#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "imgui.h"
#include "implot.h"

ABSL_FLAG(double, A4, 440.0, "Pitch of A4 in hertz");

const char* notes[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

const char* ApproximateNote(double f) {
    static char buf[64];
    // Compute the frequency of C-1 from A440 (A440/32 = A-1, 0.5946 is the
    // ration between C and A).
    double cm1 = (absl::GetFlag(FLAGS_A4) / 32.0) * 0.5946;
    static constexpr double LOG2 = std::log(2.0);
    int cents_from_cm1 = 1200.0 * (std::log(f / cm1) / LOG2);
    int semitones = cents_from_cm1 / 100;
    int cents = cents_from_cm1 % 100;
    if (cents > 50) {
        semitones += 1;
        cents = -(100 - cents);
    }
    int octave = (semitones / 12) - 1;
    int note = semitones % 12;
    int midi_bin = (24 + (octave * 12) + note) * 10 + (cents / 10);
    if (cents_from_cm1 < 0) {
        snprintf(buf, sizeof(buf) - 1, "C-1 (%+d cents) [%d]", cents_from_cm1,
                 midi_bin);
    } else {
        snprintf(buf, sizeof(buf) - 1, "%s%d (%+d cents) [%d]", notes[note],
                 octave, cents, midi_bin);
    }
    return buf;
}

AudioData::AudioData(std::unique_ptr<sound::File> file) : ImWindowBase() {
    Init();
    wave_ = std::move(file);
    fft_.set_fragsz(wave_->rate() / 50.0);
    fft_.Analyze(wave_->data(), 0);
    fcache_.set_channel(&fft_);
    ncache_.set_channel(&fft_);
    ncache_.set_style(wvx::FFTImageCache::Logarithmic);
}

void AudioData::Init() {
    for (int y = 12; y < 128; ++y) {
        note_ys_.push_back(double(y - 12) * 5.0);
        snprintf(labels_[y - 12], 4, "%s%d", notes[y % 12], (y / 12) - 1);
        note_labels_[y - 12] = labels_[y - 12];
    }
}

int AudioData::XFormat(double value, char* buf, int size, void* data) {
    AudioData* self = reinterpret_cast<AudioData*>(data);
    double seconds = value / self->wave_->rate();
    int minutes = seconds / 60.0;
    seconds = fmod(seconds, 60.0);
    return snprintf(buf, size, "%d:%02.3f", minutes, seconds);
}

void AudioData::SpectrumPopup(const char* name) {
    if (ImPlot::BeginLegendPopup(name)) {
        if (ImGui::RadioButton("Spectrum", !display_notes_)) {
            display_notes_ = false;
        }
        if (ImGui::RadioButton("Notes", display_notes_)) {
            display_notes_ = true;
        }
        float floor = display_notes_ ? ncache_.floor() : fcache_.floor();
        if (ImGui::InputFloat("Floor", &floor, 1.0f, 5.0f, "%.0f")) {
            if (display_notes_) {
                ncache_.set_floor(floor);
            } else {
                fcache_.set_floor(floor);
            }
        }
        ImPlot::EndLegendPopup();
    }
}

bool AudioData::Draw() {
    ImGui::Begin("Audio Data", &visible_, ImGuiWindowFlags_MenuBar);
    DrawMenu();
    DrawGraph();
    ImGui::End();
    if (!visible_) want_dispose_ = true;
    return true;
}

void AudioData::DrawMenu() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Close")) {
                visible_ = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void AudioData::DrawGraph() {
    static ImPlotRect lims(0, 1, 0, 1);

    if (ImPlot::BeginAlignedPlots("AlignedData")) {
        if (ImPlot::BeginPlot("Wave")) {
            ImPlot::SetupAxisLinks(ImAxis_X1, &lims.X.Min, &lims.X.Max);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisFormat(ImAxis_X1, &AudioData::XFormat, this);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImPlotCond_Always);
            ImVec2 psz = ImPlot::GetPlotSize();
            auto bounds = ImPlot::GetPlotLimits();
            // Compute the ratio of plot size to pixels;
            double xratio = bounds.X.Size() / psz.x;
            int pixels = psz.x, count = 0;
            double xs[pixels], ys1[pixels], ys2[pixels];
            double x0 = std::max(0.0, bounds.X.Min);
            double xmax = std::min(bounds.X.Max, double(wave_->size()));

            if (xratio > 1.0) {
                for (; count < pixels; ++count) {
                    double x1 = x0 + xratio;
                    if (x0 > xmax || x1 > xmax) break;
                    double p = wave_->Peak(0, x0, x1);
                    xs[count] = x0;
                    ys1[count] = p;
                    ys2[count] = -p;
                    x0 = x1;
                }
                ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.30f);
                ImPlot::PlotShaded("Data", xs, ys1, ys2, count);
                ImPlot::PopStyleVar();
                ImPlot::PlotLine("Data", xs, ys1, count);
                ImPlot::PlotLine("Data", xs, ys2, count);
            } else {
                xratio = 1.0 / xratio;
                for (; x0 < xmax; ++count) {
                    xs[count] = x0;
                    ys1[count] = wave_->Sample(0, int(x0));
                    x0 += xratio;
                }
                if (xratio > 5.0)
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
                ImPlot::PlotLine("Data", xs, ys1, count);
            }
            ImPlot::EndPlot();
        }
        if (!display_notes_ &&
            ImPlot::BeginPlot("Spectrum", ImVec2(-1, 1024))) {
            double fmax = wave_->rate() / 2.0;
            ImPlot::SetupAxisLinks(ImAxis_X1, &lims.X.Min, &lims.X.Max);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, fmax);
            ImVec2 psz = ImPlot::GetPlotSize();
            auto bounds = ImPlot::GetPlotLimits();
            // Compute the ratio of plot size to pixels;
            double xratio = bounds.X.Size() / psz.x;
            int pixels = psz.x, count = 0;
            double x0 = std::max(0.0, bounds.X.Min);
            double xmax = std::min(bounds.X.Max, double(wave_->size()));
            double bins = fft_.fftsz() / 2.0;

            int images = 0;
            for (; count < pixels; ++count) {
                double x1 = x0 + xratio;
                if (x0 > xmax || x1 > xmax) break;
                auto p = fcache_.at(x0 / wave_->rate());
                auto* image = p.first;
                if (image) {
                    ImTextureID id(
                        reinterpret_cast<void*>(image->texture_id()));
                    ImPlot::PlotImage("Spectrum", id, ImPlotPoint(x0, 0.0),
                                      ImPlotPoint(x1, fmax));
                }
                images += p.second;
                x0 = x1;
            }
            if (ImPlot::IsPlotHovered()) {
                ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                double tm = mouse.x / wave_->rate();
                size_t bin = mouse.y / fmax * bins;
                float power, freq;
                std::tie(power, freq) = fft_.MagnitudeAt(tm, bin);
                if (power >= fcache_.floor()) {
                    ImGui::BeginTooltip();
                    ImGui::Text(" Time: %.3f", tm);
                    ImGui::Text("  Bin: %ld", bin);
                    ImGui::Text("Power: %.2f", power);
                    ImGui::Text(" Freq: %.2f", freq);
                    ImGui::Text(" Note: %s", ApproximateNote(freq));
                    ImGui::EndTooltip();
                }
            }
            SpectrumPopup("Spectrum");
            ImPlot::EndPlot();
        }
        if (display_notes_ && ImPlot::BeginPlot("Notes", ImVec2(-1, 1024))) {
            double nmax = 640;
            ImPlot::SetupAxisLinks(ImAxis_X1, &lims.X.Min, &lims.X.Max);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, nmax);
            ImPlot::SetupAxisTicks(ImAxis_Y1, note_ys_.data(), note_ys_.size(),
                                   note_labels_, false);
            ImVec2 psz = ImPlot::GetPlotSize();
            auto bounds = ImPlot::GetPlotLimits();
            // Compute the ratio of plot size to pixels;
            double xratio = bounds.X.Size() / psz.x;
            int pixels = psz.x, count = 0;
            double x0 = std::max(0.0, bounds.X.Min);
            double xmax = std::min(bounds.X.Max, double(wave_->size()));

            int images = 0;
            for (; count < pixels; ++count) {
                double x1 = x0 + xratio;
                if (x0 > xmax || x1 > xmax) break;
                auto p = ncache_.at(x0 / wave_->rate());
                auto* image = p.first;
                if (image) {
                    ImTextureID id(
                        reinterpret_cast<void*>(image->texture_id()));
                    ImPlot::PlotImage("Notes", id, ImPlotPoint(x0, 0.0),
                                      ImPlotPoint(x1, nmax));
                }
                images += p.second;
                x0 = x1;
            }
            SpectrumPopup("Notes");
            ImPlot::EndPlot();
        }

        ImPlot::EndAlignedPlots();
    }
}
