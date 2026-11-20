#include "pch.h"
#include "Mp3Decoder.h"
#include "FFmpegUtil.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

Mp3Decoder::Mp3Decoder() = default;

Mp3Decoder::~Mp3Decoder()
{
    close();
}

void Mp3Decoder::close()
{
    if (packet_ != nullptr)
    {
        av_packet_free(&packet_);
    }
    if (codec_ != nullptr)
    {
        avcodec_free_context(&codec_);
    }
    if (format_ != nullptr)
    {
        avformat_close_input(&format_);
    }
    streamIndex_ = -1;
    flushed_ = false;
}

bool Mp3Decoder::open(const std::filesystem::path& path)
{
    close();
    lastError_.clear();

    const std::string utf8Path = FFmpegUtil::toUtf8(path);

    int result = avformat_open_input(&format_, utf8Path.c_str(), nullptr, nullptr);
    if (result < 0)
    {
        lastError_ = "The MP3 file cannot be opened: " + FFmpegUtil::errorString(result);
        close();
        return false;
    }

    result = avformat_find_stream_info(format_, nullptr);
    if (result < 0)
    {
        lastError_ = "The MP3 file is corrupted or unreadable: " + FFmpegUtil::errorString(result);
        close();
        return false;
    }

    const AVCodec* decoder = nullptr;
    streamIndex_ = av_find_best_stream(format_, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (streamIndex_ < 0 || decoder == nullptr)
    {
        lastError_ = "The file does not contain any audio stream.";
        close();
        return false;
    }

    codec_ = avcodec_alloc_context3(decoder);
    if (codec_ == nullptr)
    {
        lastError_ = "Out of memory while creating the MP3 decoder.";
        close();
        return false;
    }

    result = avcodec_parameters_to_context(codec_, format_->streams[streamIndex_]->codecpar);
    if (result >= 0)
    {
        codec_->pkt_timebase = format_->streams[streamIndex_]->time_base;
        result = avcodec_open2(codec_, decoder, nullptr);
    }
    if (result < 0)
    {
        lastError_ = "The MP3 decoder cannot be started: " + FFmpegUtil::errorString(result);
        close();
        return false;
    }

    packet_ = av_packet_alloc();
    if (packet_ == nullptr)
    {
        lastError_ = "Out of memory while reading the MP3 file.";
        close();
        return false;
    }

    return true;
}

int Mp3Decoder::nextFrame(AVFrame* frame)
{
    if (codec_ == nullptr)
    {
        return -1;
    }

    for (;;)
    {
        int result = avcodec_receive_frame(codec_, frame);
        if (result == 0)
        {
            return 1;
        }
        if (result == AVERROR_EOF)
        {
            return 0;
        }
        if (result != AVERROR(EAGAIN))
        {
            lastError_ = "The MP3 file is corrupted or unreadable: " + FFmpegUtil::errorString(result);
            return -1;
        }

        if (flushed_)
        {
            return 0;
        }

        result = av_read_frame(format_, packet_);
        if (result == AVERROR_EOF)
        {
            avcodec_send_packet(codec_, nullptr);
            flushed_ = true;
            continue;
        }
        if (result < 0)
        {
            lastError_ = "The MP3 file is corrupted or unreadable: " + FFmpegUtil::errorString(result);
            return -1;
        }

        if (packet_->stream_index == streamIndex_)
        {
            const int sendResult = avcodec_send_packet(codec_, packet_);
            if (sendResult < 0 && sendResult != AVERROR_INVALIDDATA && sendResult != AVERROR(EAGAIN))
            {
                av_packet_unref(packet_);
                lastError_ = "The MP3 file is corrupted or unreadable: " + FFmpegUtil::errorString(sendResult);
                return -1;
            }
        }
        av_packet_unref(packet_);
    }
}

const AVCodecContext* Mp3Decoder::codecContext() const
{
    return codec_;
}

const std::string& Mp3Decoder::lastError() const
{
    return lastError_;
}
