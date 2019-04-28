#ifndef PROJECT_APP_H
#define PROJECT_APP_H
#include <memory>
#include <string>
#include <SDL2/SDL.h>

#include "imwidget/imapp.h"
#include "util/sound/file.h"
#include "audio/fft_channel.h"
#include "imwidget/fft_cache.h"
#include "imwidget/transport.h"

namespace project {

class App: public ImApp {
  public:
    App(const std::string& name) : ImApp(name, 1280, 720) {}
    ~App() override {}

    void Init() override;
    void ProcessEvent(SDL_Event* event) override;
    void ProcessMessage(const std::string& msg, const void* extra) override;
    void Draw() override;
    void Load(const std::string& filename);

    void Help(const std::string& topickey);
    void AudioCallback(float* stream, int len) override;
  private:
    std::string save_filename_;
    std::unique_ptr<sound::File> wav_;
    std::unique_ptr<audio::FFTCache> cache_;
    audio::FFTChannel fft_;
    double time0_ = 0;
    double zoom_ = 1;
    double vzoom_ = 1;
    Transport transport_ = {};

};

}  // namespace project
#endif // PROJECT_APP_H
