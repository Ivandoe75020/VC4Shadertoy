#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

struct EncoderConfig
{
    std::filesystem::path outputPath;
    int width = 0;
    int height = 0;
    int fps = 0;
    std::filesystem::path audioPath;
    double durationSeconds = 0.0;
};

class IVideoEncoder
{
public:
    virtual ~IVideoEncoder() = default;
    virtual bool open(const EncoderConfig& config) = 0;
    virtual bool writeVideoFrame(const std::uint8_t* rgbaPixels, std::size_t strideBytes) = 0;
    virtual bool finalize() = 0;
    virtual void abort() = 0;
};
