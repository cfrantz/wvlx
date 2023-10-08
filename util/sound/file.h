#ifndef WVLX_UTIL_SOUND_FILE_H
#define WVLX_UTIL_SOUND_FILE_H
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <sndfile.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace sound {

enum class Interpolation {
    None = 0,
    Linear,
    Cosine,
    Cubic,
};

class Channel {
  public:
    Channel(const float* data, size_t samples, double rate, size_t stride=1);
    Channel(size_t samples, double rate)
      : data_(samples),
       rate_(rate),
       length_(samples / rate) {}

    static Channel ConstantValue(size_t samples, double rate, float value);

    void Resample(double rate);
    float Peak(size_t s0, size_t s1);
    float at(double tm) const;
    // Gets the sample at index, mirrors signal at the endpoints.
    inline float sample(int64_t index) const {
        int64_t len = int64_t(data_.size());
        if (index < 0) {
            return -sample(-index);
        } else if (index >= len) {
            index = (len - 1) - (index - len);
            return -sample(-index);
        } else {
            return data_.at(index);
        }
    }
    inline float* data() { return data_.data(); }
    inline size_t size() const { return data_.size(); }
    inline double rate() const { return rate_; }
    inline double length() const { return length_; }
    inline void resize(size_t samples) { data_.resize(samples); }
    inline void set_interpolation(Interpolation i) { interp_ = i; }

  private:
    float interp_linear(size_t x, double f) const;
    float interp_cosine(size_t x, double f) const;
    float interp_cubic (size_t x, double f) const;

    std::vector<float> data_;
    Interpolation interp_;
    double rate_;
    double length_;
};

class File {
  public:
    File() = default;
    static absl::StatusOr<std::unique_ptr<File>> Load(const std::string& filename);
    static absl::StatusOr<std::unique_ptr<File>> LoadAsMono(const std::string& filename);
    absl::Status Save(const std::string& filename);

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
