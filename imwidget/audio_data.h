#ifndef WVLX2_IMWIDGET_AUDIO_DATA_H
#define WVLX2_IMWIDGET_AUDIO_DATA_H
#include <memory>

#include "ImGuiFileDialog.h"
#include "audio/fft_channel.h"
#include "audio/fft_image_cache.h"
#include "imwidget/imwidget.h"
#include "proto/wvx.pb.h"
#include "util/sound/file.h"

class AudioData : public ImWindowBase {
  public:
    AudioData(std::unique_ptr<sound::File> file);
    AudioData(wvx::proto::Project&& project);
    ~AudioData() override {}

    bool Draw() override;

  private:
    void Init();
    void SpectrumPopup(const char* name);
    void DrawMenu();
    void DrawGraph();
    static int XFormat(double value, char* buf, int size, void* data);

    std::unique_ptr<sound::File> wave_;
    wvx::FFTChannel fft_;
    wvx::proto::Project project_;
    wvx::FFTImageCache fcache_;
    wvx::FFTImageCache ncache_;
    bool display_notes_ = false;

    std::vector<double> note_ys_;
    char labels_[128][4];
    const char* note_labels_[128];
    ImPlotRect limits_ = ImPlotRect(0, 1, 0, 1);
    std::string filename_;
    ImGuiFileDialog file_dialog_;
};

#endif  // WVLX2_IMWIDGET_AUDIO_DATA_H
