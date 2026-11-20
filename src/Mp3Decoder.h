#pragma once

#include <filesystem>
#include <string>

struct AVFormatContext;
struct AVCodecContext;
struct AVPacket;
struct AVFrame;

class Mp3Decoder
{
public:
    Mp3Decoder();
    ~Mp3Decoder();

    Mp3Decoder(const Mp3Decoder&) = delete;
    Mp3Decoder& operator=(const Mp3Decoder&) = delete;

    bool open(const std::filesystem::path& path);
    void close();

    int nextFrame(AVFrame* frame);

    const AVCodecContext* codecContext() const;
    const std::string& lastError() const;

private:
    AVFormatContext* format_ = nullptr;
    AVCodecContext* codec_ = nullptr;
    AVPacket* packet_ = nullptr;
    int streamIndex_ = -1;
    bool flushed_ = false;
    std::string lastError_;
};
