#pragma once

#include "IAudioSource.h"

#include <string>

class FFmpegAudioSource final : public IAudioSource
{
public:
    bool open(const std::filesystem::path& path) override;
    double durationSeconds() const override;

    int sampleRate() const;
    const std::string& lastError() const;

private:
    double durationSeconds_ = 0.0;
    int sampleRate_ = 0;
    std::string lastError_;
};
