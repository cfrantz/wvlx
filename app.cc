#include "app.h"

#include <cstdio>

#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "imgui.h"
#include "imwidget/audio_data.h"
#include "imwidget/error_dialog.h"
#include "proto/wvx.pb.h"
#include "util/browser.h"
#include "util/file.h"
#include "util/os.h"
#include "util/proto_file.h"
#include "util/sound/file.h"
#include "version.h"

#ifdef HAVE_NFD
#include "nfd.h"
#endif
#include "ImGuiFileDialog.h"
#include "implot.h"

namespace project {

App::App(const std::string& name) : ImApp(name, 1280, 720) {
    ImPlot::CreateContext();
}
App::~App() { ImPlot::DestroyContext(); }

void App::Init() {
    InitAudio(48000, 1, 1024, AUDIO_F32);
}

void App::ProcessEvent(SDL_Event* event) {}

void App::ProcessMessage(const std::string& msg, const void* extra) {}

void App::Draw() {
    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    auto* igfd = ImGuiFileDialog::Instance();
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
#ifdef HAVE_NFD
                char* filename = nullptr;
                auto result = NFD_OpenDialog("z2prj;nes", nullptr, &filename);
                if (result == NFD_OKAY) {
                    // DOSTUFF
                    save_filename_.assign(filename);
                }
                free(filename);
#else
                igfd->OpenDialog("FileOpen", "Open File", ".*", ".", 1, nullptr,
                                 ImGuiFileDialogFlags_Modal);

#endif
            }

            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (save_filename_.empty()) goto save_as;
                // DOSTUFF
            }
            if (ImGui::MenuItem("Save As")) {
            save_as:
#ifdef HAVE_NFD
                char* filename = nullptr;
                auto result = NFD_SaveDialog("z2prj", nullptr, &filename);
                if (result == NFD_OKAY) {
                    std::string savefile = filename;
                    if (absl::EndsWith(savefile, ".z2prj")) {
                        save_filename_.assign(savefile);
                        // DOSTUFF
                    } else {
                        ErrorDialog::Spawn(
                            "Bad File Extension",
                            ErrorDialog::OK | ErrorDialog::CANCEL,
                            "Project files should have the extension .z2prj\n"
                            "If you want to save a .nes file, use File | "
                            "Export\n\n"
                            "Press 'OK' to save anyway.\n")
                            ->set_result_cb([=](int result) {
                                if (result == ErrorDialog::OK) {
                                    save_filename_.assign(savefile);
                                    // DOSTUFF
                                }
                            });
                    }
                }
                free(filename);
#else
                igfd->OpenDialog("FileSaveAs", "Save File", ".*", ".", 1,
                                 nullptr, ImGuiFileDialogFlags_Modal);
#endif
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Debug Console", nullptr, &console_.visible());
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Implot Demo", nullptr, &plot_demo_);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Online Help")) {
                Help("root");
            }
            if (ImGui::MenuItem("About")) {
                ErrorDialog::Spawn("About App", "Empty Project\n\n",
#ifdef BUILD_GIT_VERSION
                                   "Version: ", BUILD_GIT_VERSION, "-",
                                   BUILD_SCM_STATUS
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

    if (igfd->Display("FileOpen")) {
        if (igfd->IsOk()) {
            Load(igfd->GetFilePathName());
        }
        igfd->Close();
    }
    if (igfd->Display("FileSaveAs")) {
        if (igfd->IsOk()) {
            std::string filename = igfd->GetFilePathName();
            LOG(ERROR) << "File|SaveAs: " << filename;
        }
        igfd->Close();
    }
    ImPlot::ShowDemoWindow(&plot_demo_);
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
}

void App::Load(const std::string& filename) {
    switch (DetectFileType(filename)) {
        case FileType::Error:
            LOG(ERROR) << "Error opening " << filename;
            break;
        case FileType::Unknown: {
            auto sf = sound::File::Load(filename);
            if (sf.ok()) {
                (*sf)->ConvertToMono();
                AddDrawCallback(new AudioData(std::move(*sf)));
            } else {
                LOG(ERROR) << "Error opening " << filename << ": "
                           << sf.status();
            }
            break;
        }
        case FileType::Wvx: {
            auto pf = ProtoFile::Load<wvx::proto::Project>(filename);
            if (pf.ok()) {
                AddDrawCallback(new AudioData(std::move(*pf)));
            } else {
                LOG(ERROR) << "Error opening " << filename << ": "
                           << pf.status();
            }
            break;
        }
    }
}

App::FileType App::DetectFileType(const std::string& filename) {
    auto f = File::Open(filename, "rb");
    if (f == nullptr) {
        return FileType::Error;
    }
    std::string header;
    if (!f->Read(&header, 256).ok()) {
        return FileType::Error;
    }
    if (header.find("WvxProjectFile") != std::string::npos) {
        return FileType::Wvx;
    }
    return FileType::Unknown;
}

void App::Help(const std::string& topickey) {}

void App::AudioCallback(float* stream, int len) {
    memset(stream, 0, len*sizeof(float));
    float n = 0.0;
    for(auto& a : draw_callback_) {
        if (a->AudioCallback(stream, len)) {
            n += 1.0;
        }
    }
    if (n > 0.0) {
        for(int i=0; i<len; ++i) {
            stream[i] /= n;
        }
    }
}

}  // namespace project
