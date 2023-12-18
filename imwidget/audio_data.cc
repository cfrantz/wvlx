#include "imwidget/audio_data.h"

#include <algorithm>
#include <cstdint>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "imgui.h"
#include "implot.h"
#include "util/os.h"
#include "util/proto_file.h"

ABSL_FLAG(double, A4, 440.0, "Pitch of A4 in hertz");

const char* notes[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

const char* AudioData::ApproximateNote(double f) const {
    static char buf[64];
    // Compute the frequency of C-1 from A440 (A440/32 = A-1, 0.5946 is the
    // ration between C and A).
    double cm1 = (A4_ / 32.0) * 0.5946;
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
    fcache_.set_a4(A4_);
    ncache_.set_channel(&fft_);
    ncache_.set_a4(A4_);
    ncache_.set_style(wvx::FFTImageCache::Logarithmic);
    project_.set_a4(A4_);

    project_.set_file_type_detector("WvxProjectFile");
    project_.set_allocated_wave(wave_->release());
    project_.set_allocated_fft(fft_.release());
}

AudioData::AudioData(wvx::proto::Project&& project) : ImWindowBase() {
    Init();
    project_ = project;

    if (project_.a4()) A4_ = project_.a4();
    wave_ = std::make_unique<sound::File>(project_.mutable_wave());
    fft_ = wvx::FFTChannel(project_.mutable_fft());
    fcache_.set_channel(&fft_);
    fcache_.set_a4(A4_);
    ncache_.set_channel(&fft_);
    ncache_.set_a4(A4_);
    ncache_.set_style(wvx::FFTImageCache::Logarithmic);
}

void AudioData::Init() {
    A4_ = absl::GetFlag(FLAGS_A4);
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
    TransportWidget(&transport_);
    DrawGraph();
    ImGui::End();
    if (!visible_) want_dispose_ = true;
    return true;
}

void AudioData::DrawMenu() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save")) {
                if (filename_.empty()) goto save_as;
                (void)ProtoFile::Save(filename_, project_);
            }
            if (ImGui::MenuItem("Save As")) {
            save_as:
                file_dialog_.OpenDialog("SaveAs", "Save Project", ".*", ".", 1,
                                        nullptr, ImGuiFileDialogFlags_Modal);
            }
            if (ImGui::MenuItem("Close")) {
                visible_ = false;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (file_dialog_.Display("SaveAs")) {
        if (file_dialog_.IsOk()) {
            filename_ = file_dialog_.GetFilePathName();
            (void)ProtoFile::Save(filename_, project_);
        }
        file_dialog_.Close();
    }
}

void AudioData::CalculateMarkers() {
    event_.clear();
    if (project_.event_size() == 0)
            return;
    std::stable_sort(
                    project_.mutable_event()->begin(),
                    project_.mutable_event()->end(),
                    [](const wvx::proto::Event& a, const wvx::proto::Event& b) {
                        return a.sample() < b.sample();
                    });
    double tempo = 120.0;
    double rate = project_.wave().rate();
    // samples per beat.
    double spb = rate * 60.0 / tempo;
    double sample = project_.event(0).sample();

    for(int i=0; i<project_.event_size(); ++i) {
        const auto& e = project_.event(i);
        switch(e.event_case()) {
            case wvx::proto::Event::EventCase::kEnd:
                continue;
            case wvx::proto::Event::EventCase::kTempo:
                sample = e.sample();
                tempo = e.tempo().bpm();
                spb = rate * 60.0 / tempo;
                event_.emplace_back(Event{
                    EventType::Tempo,
                    i,
                    sample,
                    0.0,
                });
                break;
            default:
                //event_.push_back(e);
                ;
        }
        double end = (i+1 < project_.event_size()) ? project_.event(i+1).sample() : project_.wave().channel(0).data_size();
        double binc = 1.0;
        ImVec2 p0 = ImPlot::PlotToPixels(sample, 0.0);
        ImVec2 p1 = ImPlot::PlotToPixels(sample + spb, 0.0);
        if (p1.x - p0.x > 127) {
            while(0.5*(p1.x - p0.x) > 64) {
                spb /= 2.0;
                binc /= 2.0;
                p1 = ImPlot::PlotToPixels(sample + spb, 0.0);
            }
        } else {
            while(p1.x - p0.x < 64) {
                spb *= 2.0;
                binc *= 2.0;
                p1 = ImPlot::PlotToPixels(sample + spb, 0.0);
            }
        }
        double beats = binc;
        sample += spb;
        for(; sample < end; sample += spb) {
            event_.emplace_back(Event {
                EventType::Adjust,
                i,
                sample,
                beats,
            });
            beats += binc;
        }
    }
}

void AudioData::AddTempoMarker(double sample) {
    auto* event = project_.add_event();
    event->set_sample(sample);
    event->mutable_tempo()->set_bpm(120);
    event->mutable_tempo()->mutable_time()->set_numerator(4);
    event->mutable_tempo()->mutable_time()->set_denominator(4);
}

void AudioData::CalculateTimeline() {
    int n = 1, d = 1;
    int beat, measure;
    double frac;
    timeline_.clear();
    timeline_label_.clear();
    for(size_t i=0; i<event_.size(); ++i) {
        auto& event = event_.at(i);
        auto *e = project_.mutable_event(event.ref);
        switch(event.type) {
            case EventType::Adjust:
                measure = event.beats / double(n);
                beat = fmod(event.beats, n);
                frac = fmod(event.beats, 1.0);
                if (frac == 0.0) {
                    timeline_.push_back(event.sample);
                    timeline_label_.push_back(absl::StrFormat("%d|%d", measure+1, beat+1));
                } else if (frac == 0.5) {
                    timeline_.push_back(event.sample);
                    timeline_label_.push_back("+");
                } else if (frac == 0.25) {
                    timeline_.push_back(event.sample);
                    timeline_label_.push_back("e");
                } else if (frac == 0.75) {
                    timeline_.push_back(event.sample);
                    timeline_label_.push_back("a");
                } else {
                    timeline_.push_back(event.sample);
                    timeline_label_.push_back("");
                }
                break;
            case EventType::Tempo:
                n = e->tempo().time().numerator();
                d = e->tempo().time().denominator();
                measure = event.beats / double(n);
                beat = fmod(event.beats, n);
                timeline_.push_back(event.sample);
                timeline_label_.push_back(absl::StrFormat("%d|%d", measure+1, beat+1));
                break;
            case EventType::End:
                break;
        }
    }
}

void AudioData::DrawTimeline() {
    const char* labels[timeline_.size()];
    int n = 0;
    for(const auto& lbl : timeline_label_) {
        labels[n++] = lbl.c_str();
    }
    ImPlot::SetupAxisTicks(ImAxis_X1, timeline_.data(), timeline_.size(), labels, false);
}

bool AudioData::DrawTempoMarkers() {
    bool wavectx = true;
    int erase = -1;
    for(size_t i=0; i<event_.size(); ++i) {
        auto& event = event_.at(i);
        auto *e = project_.mutable_event(event.ref);
        int next = i+1<event_.size() ? event_.at(i+1).sample : wave_->size();
        ImVec4 color;
        ImVec2 p0, p1;
        ImPlotDragToolFlags flags = ImPlotDragToolFlags_NoFit;
        switch(event.type) {
            case EventType::Adjust:
                p0 = ImPlot::PlotToPixels(event.sample, 0.0);
                p1 = ImPlot::PlotToPixels(next, 0.0);
                if (p1.x-p0.x < 32) continue;
                color = ImVec4(0.5, 0.5, 0.5, 0.5);
                flags |= ImPlotDragToolFlags_GrabHandles;
                break;
            case EventType::Tempo:
                color = ImVec4(0, 1, 0, 1);
                break;
            case EventType::End:
                color = ImVec4(1, 0, 0, 1);
                break;
        }
        ImGui::PushID(event.id());
        bool hovered = false;
        if (ImPlot::DragLineX(event.id(), &event.sample, color, 1,
                              flags, nullptr, &hovered)) {
            switch(event.type) {
                case EventType::Adjust: {
                    double rate = project_.wave().rate();
                    double time = (event.sample - e->sample()) / (rate * 60.0);
                    double bpm = event.beats / time;
                    e->mutable_tempo()->set_bpm(bpm);
                    break;
                }
                case EventType::Tempo:
                case EventType::End:
                    e->set_sample(event.sample);
                    break;
            }
        }
        if (ImGui::BeginPopup("##DragContext")) {
            switch(event.type) {
                case EventType::Adjust:
                    ImGui::Text("Adjust");
                    break;
                case EventType::End:
                    ImGui::Text("End");
                    break;
                case EventType::Tempo: {
                    double tempo = e->tempo().bpm();
                    int n = e->tempo().time().numerator();
                    int d = e->tempo().time().denominator();
                    if (ImGui::InputDouble("Tempo", &tempo)) {
                        e->mutable_tempo()->set_bpm(tempo);
                    }
                    if (ImGui::InputInt("Time Signature", &n)) {
                        e->mutable_tempo()->mutable_time()->set_numerator(n);
                    }
                    if (ImGui::InputInt("##denominator", &d)) {
                        e->mutable_tempo()->mutable_time()->set_denominator(d);
                    }
                    if (ImGui::Button("Delete")) {
                        erase = event.ref;
                    }
                    break;
                }
            }
            ImGui::EndPopup();
        }
        if (hovered) {
            ImGui::OpenPopupOnItemClick("##DragContext");
            wavectx = false;
        }
        ImGui::PopID();
    }
    if (erase != -1) {
        auto *p = project_.mutable_event();
        p->erase(p->begin() + erase);
    }
    return wavectx;
}

void AudioData::DrawPlayHead(bool follow) {
    ImGui::PushID("Playhead");
    if (ImPlot::DragLineX(0, &frame_time_, ImVec4(1,0,0,1), ImPlotDragToolFlags_NoFit)) {
        transport_.time = frame_time_ / wave_->rate();
    }
    ImGui::PopID();
}

void AudioData::DrawGraph() {
    frame_time_ = transport_.frame_time * wave_->rate();
    if (ImPlot::BeginAlignedPlots("AlignedData")) {
        if (ImPlot::BeginPlot("Wave", ImVec2(-1, 0), ImPlotFlags_NoMenus)) {
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
            ImPlot::SetupAxisLinks(ImAxis_X1, &limits_.X.Min, &limits_.X.Max);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, wave_->size());
            if (transport_.playing) {
                double tm = wave_->rate() * transport_.time;
                double sz = (limits_.X.Max - limits_.X.Min);
                double t0 = tm - sz / 2.0;
                if (t0 < 0.0) t0 = 0.0;

                double t1 = t0 + sz;
                if (t1 >= wave_->size()) {
                    t1 = wave_->size();
                    t0 = t1 - sz;
                }
                limits_.X.Min = t0;
                limits_.X.Max = t1;
                ImPlot::SetupAxisLinks(ImAxis_X1, &limits_.X.Min, &limits_.X.Max);
            }
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, wave_->size());
            DrawTimeline();
            //ImPlot::SetupAxisFormat(ImAxis_X1, &AudioData::XFormat, this);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1, 1, ImPlotCond_Always);
            ImVec2 psz = ImPlot::GetPlotSize();
            auto bounds = ImPlot::GetPlotLimits();
            // Compute the ratio of plot size to pixels;
            double xratio = bounds.X.Size() / psz.x;
            int pixels = psz.x, count = 0;
            double xs[pixels], ys1[pixels], ys2[pixels];
            double x0 = std::max(0.0, bounds.X.Min);
            double xmax = std::min(bounds.X.Max, double(wave_->size()));
            CalculateMarkers();
            CalculateTimeline();

            DrawPlayHead();
            bool wavectx = DrawTempoMarkers();
            if (ImGui::BeginPopup("##WaveContext")) {
                if (ImGui::MenuItem("Add Tempo Marker")) {
                    ImPlotPoint pos = ImPlot::GetPlotMousePos();
                    AddTempoMarker(pos.x);
                }
                ImGui::EndPopup();
            }
            if (wavectx) ImGui::OpenPopupOnItemClick("##WaveContext");

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
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
            ImPlot::SetupAxisLinks(ImAxis_X1, &limits_.X.Min, &limits_.X.Max);
            //ImPlot::SetupAxisLimits(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, fmax);
            DrawTimeline();

            ImVec2 psz = ImPlot::GetPlotSize();
            auto bounds = ImPlot::GetPlotLimits();
            // Compute the ratio of plot size to pixels;
            double xratio = bounds.X.Size() / psz.x;
            int pixels = psz.x, count = 0;
            double x0 = std::max(0.0, bounds.X.Min);
            double xmax = std::min(bounds.X.Max, double(wave_->size()));
            double bins = fft_.fftsz() / 2.0;

            DrawPlayHead();
            bool wavectx = DrawTempoMarkers();
            if (ImGui::BeginPopup("##FftContext")) {
                if (ImGui::MenuItem("Add Tempo Marker")) {
                    ImPlotPoint pos = ImPlot::GetPlotMousePos();
                    AddTempoMarker(pos.x);
                }
                ImGui::EndPopup();
            }
            if (wavectx) ImGui::OpenPopupOnItemClick("##FftContext");

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
            if (ImPlot::DragLineY(0, &A4_, ImVec4(1, 0, 0, 1), 1,
                                  ImPlotDragToolFlags_NoFit)) {
                project_.set_a4(A4_);
                fcache_.set_a4(A4_);
                ncache_.set_a4(A4_);
            }
            ImPlot::TagY(A4_, ImVec4(1, 0, 0, 1), "A4");
            ImPlot::EndPlot();
        }
        if (display_notes_ && ImPlot::BeginPlot("Notes", ImVec2(-1, 1024))) {
            double nmax = 640;
            ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoGridLines);
            ImPlot::SetupAxisLinks(ImAxis_X1, &limits_.X.Min, &limits_.X.Max);
            //ImPlot::SetupAxisLimits(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, wave_->size());
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0, nmax);
            ImPlot::SetupAxisTicks(ImAxis_Y1, note_ys_.data(), note_ys_.size(),
                                   note_labels_, false);
            DrawTimeline();

            ImVec2 psz = ImPlot::GetPlotSize();
            auto bounds = ImPlot::GetPlotLimits();
            // Compute the ratio of plot size to pixels;
            double xratio = bounds.X.Size() / psz.x;
            int pixels = psz.x, count = 0;
            double x0 = std::max(0.0, bounds.X.Min);
            double xmax = std::min(bounds.X.Max, double(wave_->size()));

            DrawPlayHead();
            bool wavectx = DrawTempoMarkers();
            if (ImGui::BeginPopup("##NoteContext")) {
                if (ImGui::MenuItem("Add Tempo Marker")) {
                    ImPlotPoint pos = ImPlot::GetPlotMousePos();
                    AddTempoMarker(pos.x);
                }
                ImGui::EndPopup();
            }
            if (wavectx) ImGui::OpenPopupOnItemClick("##NoteContext");

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

bool AudioData::AudioCallback(float *stream, int len) {
    if (!transport_.playing) {
        return false;
    }
    if (transport_.time < 0.0) transport_.time = 0.0;
    transport_.frame_time = transport_.time;
    double delta = (1.0 / 48000.0) * transport_.speed;
    for(int i=0; i<len; ++i) {
        if (transport_.time > wave_->length()) {
            transport_.playing = false;
            break;
        }
        stream[i] += wave_->InterpolateAt(0, transport_.time); 
        transport_.time += delta;
    }
    return true;
}
