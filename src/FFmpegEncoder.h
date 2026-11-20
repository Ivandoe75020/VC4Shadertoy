#pragma once

#include "IVideoEncoder.h"

#include <memory>
#include <string>

class FFmpegEncoder final : public IVideoEncoder
{
public:
    FFmpegEncoder();
    explicit FFmpegEncoder(std::string videoCodecName);
    ~FFmpegEncoder() override;

    FFmpegEncoder(const FFmpegEncoder&) = delete;
    FFmpegEncoder& operator=(const FFmpegEncoder&) = delete;

    bool open(const EncoderConfig& config) override;
    bool writeVideoFrame(const std::uint8_t* rgbaPixels, std::size_t strideBytes) override;
    bool finalize() override;
    void abort() override;

    const std::string& lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
