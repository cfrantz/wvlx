#ifndef WVLX_UTIL_SOUND_FILE_H
#define WVLX_UTIL_SOUND_FILE_H
#include <memory>
#include <string>
#include <vector>
#include <sndfile.h>

#include "util/logging.h"
#include "util/status.h"
#include "util/statusor.h"
#include "absl/memory/memory.h"

namespace sound {

enum class Interpolation {
    None = 0,
    Linear = 1,
};

class Channel {
  public:
    Channel(const float* data, size_t samples, double rate, size_t stride=1);
    Channel(size_t samples, double rate)
      : data_(samples),
       rate_(rate),
       length_(samples / rate) {}

    float at(double tm) const;
    inline float sample(size_t index) const {
        return index < data_.size() ? data_.at(index) : 0.0;
    }
    inline float* data() { return data_.data(); }
    inline size_t size() const { return data_.size(); }
    inline double rate() const { return rate_; }
    inline double length() const { return length_; }
    inline void resize(size_t samples) { data_.resize(samples); }
    inline void set_interpolation(Interpolation i) { interp_ = i; }

  private:
    std::vector<float> data_;
    Interpolation interp_;
    double rate_;
    double length_;
};

class File {
  public:
    File() = default;
    static std::unique_ptr<File> Load(const std::string& filename);
    static std::unique_ptr<File> LoadAsMono(const std::string& filename);
    util::Status Save(const std::string& filename);

    inline size_t channels() { return channel_.size(); }
    std::shared_ptr<Channel> channel(size_t i) { return channel_.at(i); }
    void add_channel(std::shared_ptr<Channel> c) { channel_.emplace_back(c); }
    const SF_INFO& info() { return info_; }

  private:
    static std::vector<float> SFRead(SNDFILE* sf, SF_INFO* info);
    std::vector<std::shared_ptr<Channel>> channel_;
    SF_INFO info_;
};

}  // namespace
#endif // WVLX_UTIL_SOUND_FILE_H
