#ifndef PROJECT_APP_H
#define PROJECT_APP_H
#include <SDL2/SDL.h>

#include <memory>
#include <string>

#include "imwidget/imapp.h"

namespace project {

class App : public ImApp {
  public:
    enum FileType {
        Error = -1,
        Unknown = 0,
        Wvx,
    };
    App(const std::string& name);
    ~App() override;

    void Init() override;
    void ProcessEvent(SDL_Event* event) override;
    void ProcessMessage(const std::string& msg, const void* extra) override;
    void Draw() override;
    void Load(const std::string& filename);

    void Help(const std::string& topickey);
    static FileType DetectFileType(const std::string& filename);

  private:
    std::string save_filename_;
    bool plot_demo_ = false;
};

}  // namespace project
#endif  // PROJECT_APP_H
