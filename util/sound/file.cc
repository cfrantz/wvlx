#include "util/sound/file.h"

#include <sndfile.h>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/text_format.h"

namespace sound {

constexpr float PI = 3.14159265358979323846f;
constexpr double EPSILON = 0.000001;

absl::StatusOr<std::unique_ptr<File>> File::Load(const std::string& filename) {
    SF_INFO info;
    SNDFILE* fp = sf_open(filename.c_str(), SFM_READ, &info);
    if (fp == nullptr) {
        LOG(ERROR) << "File::Load could not open " << filename;
        return absl::InvalidArgumentError(sf_strerror(fp));
    }
    LOG(INFO) << "soundfile:"
              << " frames:" << info.frames << " rate:" << info.samplerate
              << " ch:" << info.channels << " format:" << info.format;

    std::vector<float> buf(size_t(info.samplerate * info.channels));
    size_t count;
    proto::File* data = new proto::File;
    data->set_name(filename);
    data->set_rate(double(info.samplerate));
    for (int ch = 0; ch < info.channels; ++ch) {
        data->mutable_channel()->Add();
    }
    do {
        count = sf_readf_float(fp, buf.data(), info.samplerate);
        for (size_t i = 0, n = 0; n < count; ++n) {
            for (int ch = 0; ch < info.channels; ++ch) {
                data->mutable_channel(ch)->mutable_data()->Add(buf[i++]);
            }
        }
    } while (count);
    sf_close(fp);
    return absl::make_unique<File>(data, true);
}

absl::Status File::SaveProto(const std::string& filename,
                             const proto::File& file) {
    std::fstream out(filename, std::ios_base::out | std::ios_base::trunc |
                                   std::ios_base::binary);
    if (!file.SerializeToOstream(&out)) {
        return absl::InternalError("serialization failed");
    }
    return absl::OkStatus();
}

absl::Status File::SaveText(const std::string& filename,
                            const proto::File& file) {
    FILE* fp = fopen(filename.c_str(), "wt");
    if (fp == nullptr) {
        return absl::InternalError("failed to open file");
    }
    google::protobuf::io::FileOutputStream out(fileno(fp));
    if (!google::protobuf::TextFormat::Print(file, &out)) {
        return absl::InternalError("serialization failed");
    }
    return absl::OkStatus();
}

absl::Status File::SaveWav(const std::string& filename,
                           const proto::File& file) {
    SF_INFO info{(sf_count_t)file.channel(0).data_size(), (int)file.rate(),
                 (int)file.channel_size(), SF_FORMAT_WAV | SF_FORMAT_FLOAT};
    for (const auto& ch : file.channel()) {
        if (ch.data_size() != info.frames) {
            return absl::FailedPreconditionError(
                "Not all channels the same length");
        }
    }
    LOG(INFO) << "soundfile:"
              << " frames:" << info.frames << " rate:" << info.samplerate
              << " ch:" << info.channels << " format:" << info.format;
    LOG(INFO) << "format_check " << sf_format_check(&info);

    // Take the dumb way out: allocate a buffer big enough to interleave
    // all of the sample data.
    std::vector<float> data;
    for (int i = 0; i < info.frames; ++i) {
        for (int ch = 0; ch < info.channels; ++ch) {
            data.emplace_back(file.channel(ch).data(i));
        }
    }

    SNDFILE* fp = sf_open(filename.c_str(), SFM_WRITE, &info);
    if (fp == nullptr) {
        return absl::InternalError(sf_strerror(fp));
    }
    sf_write_float(fp, data.data(), data.size());
    sf_close(fp);
    return absl::OkStatus();
}

absl::Status File::Save(const std::string& filename, const proto::File& file) {
    if (absl::EndsWith(filename, ".wav")) {
        return SaveWav(filename, file);
    } else if (absl::EndsWith(filename, ".pb")) {
        return SaveProto(filename, file);
    } else if (absl::EndsWith(filename, ".textpb")) {
        return SaveText(filename, file);
    } else {
        return absl::InvalidArgumentError("bad file extension");
    }
}

absl::Status File::Save(const std::string& filename) const {
    return Save(filename, *file_);
}

void File::ConvertToMono() {
    if (file_->channel_size() == 1) {
        // File is already mono.
        return;
    }
    proto::File* file = new proto::File;
    file->set_name(file_->name());
    file->set_rate(file_->rate());
    auto* data = file->mutable_channel()->Add();
    float n = file_->channel_size();
    for (int i = 0; i < file_->channel(0).data_size(); ++i) {
        float sample = 0;
        for (int ch = 0; ch < file_->channel_size(); ++ch) {
            sample += file_->channel(ch).data(i);
        }
        data->mutable_data()->Add(sample / n);
    }
    replace(file);
}

float File::Peak(int ch, int s0, int s1) const {
    auto& channel = file_->channel(ch);
    float peak = 0.0f;
    for (int i = s0; i < s1; ++i) {
        float sample =
            (i >= 0 && i < channel.data_size()) ? channel.data(i) : 0.0f;
        peak = std::max(peak, std::abs(sample));
    }
    return peak;
}

}  // namespace sound
