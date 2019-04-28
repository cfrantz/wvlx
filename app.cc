#include <cstdio>

#include <gflags/gflags.h>
#include "app.h"
#include "imgui.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "imwidget/error_dialog.h"
#include "imwidget/fft_display.h"
#include "imwidget/wave_display.h"
#include "util/browser.h"
#include "util/os.h"
#include "util/logging.h"
#include "util/imgui_impl_sdl.h"

#include "version.h"

#ifdef HAVE_NFD
#include "nfd.h"
#endif


namespace project {

void App::Init() {
    InitAudio(48000, 1, 1024, AUDIO_F32);
}

void App::ProcessEvent(SDL_Event* event) {
}

void App::ProcessMessage(const std::string& msg, const void* extra) {
}

void App::Load(const std::string& filename) {
    wav_ = std::move(sound::File::LoadAsMono(filename));
    fft_.Init(8192, 8192, audio::FFTChannel::WindowFn::BLACKMAN);
    fft_.set_fragsz(512);
    fft_.Analyze(*wav_->channel(0));
    cache_ = absl::make_unique<audio::FFTCache>(&fft_);
}

void App::Draw() {
    ImGui::SetNextWindowSize(ImVec2(500,300), ImGuiSetCond_FirstUseEver);
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                char *filename = nullptr;
                auto result = NFD_OpenDialog("z2prj;nes", nullptr, &filename);
                if (result == NFD_OKAY) {
                    // DOSTUFF
                    save_filename_.assign(filename);
                }
                free(filename);
            }

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (save_filename_.empty())
                    goto save_as;
                // DOSTUFF
            }
            if (ImGui::MenuItem("Save As")) {
save_as:
                char *filename = nullptr;
                auto result = NFD_SaveDialog("z2prj", nullptr, &filename);
                if (result == NFD_OKAY) {
                    std::string savefile = filename;
                    if (absl::EndsWith(savefile, ".z2prj")) {
                        save_filename_.assign(savefile);
                        // DOSTUFF
                    } else {
                        ErrorDialog::Spawn("Bad File Extension",
                            ErrorDialog::OK | ErrorDialog::CANCEL,
                            "Project files should have the extension .z2prj\n"
                            "If you want to save a .nes file, use File | Export\n\n"
                            "Press 'OK' to save anyway.\n")->set_result_cb(
                                [=](int result) {
                                    if (result == ErrorDialog::OK) {
                                        save_filename_.assign(savefile);
                                        // DOSTUFF
                                    }
                                });
                    }
                }
                free(filename);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Debug Console", nullptr,
                            &console_.visible());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Online Help")) {
                Help("root");
            }
            if (ImGui::MenuItem("About")) {
                ErrorDialog::Spawn("About App",
                    "Empty Project\n\n",
#ifdef BUILD_GIT_VERSION
                    "Version: ", BUILD_GIT_VERSION, "-", BUILD_SCM_STATUS
#else
                    "Version: Unknown"
#warning "Built without version stamp"
#endif
                    );

            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

#if 0
    if (!loaded_) {
        char *filename = nullptr;
        auto result = NFD_OpenDialog("z2prj,nes", nullptr, &filename);
        if (result == NFD_OKAY) {
            project_.Load(filename, false);
            if (ends_with(filename, ".z2prj")) {
                save_filename_.assign(filename);
            }
        }
        free(filename);
    }
#endif

    if (wav_) {
        if (ImGui::Begin("Wave")) {
            TransportWidget(&transport_);
            WaveDisplay2("Waveform", wav_->channel(0), &time0_, &zoom_, &transport_);
            FFTDisplay("Spectrogram", cache_.get(), &time0_, &zoom_, &vzoom_,
                    &transport_,
                    ImVec2(0, 640));
            ImGui::End();
        }
    }
}

void App::AudioCallback(float* stream, int len) {
    if (transport_.playing) {
        transport_.frame_time = transport_.time;
        while(len--) {
            *stream++ = wav_->channel(0)->at(transport_.time);
            transport_.time += (1.0/48000.0);
        }
    } else {
        while(len--) {
            *stream++ = 0.0;
        }
    }
}

void App::Help(const std::string& topickey) {
}

}  // namespace project
