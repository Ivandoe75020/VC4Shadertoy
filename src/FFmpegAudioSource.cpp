#include "pch.h"
#include "FFmpegAudioSource.h"
#include "Mp3Decoder.h"

#include <cstdint>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

bool FFmpegAudioSource::open(const std::filesystem::path& path)
{
    durationSeconds_ = 0.0;
    sampleRate_ = 0;
    lastError_.clear();

    Mp3Decoder decoder;
    if (!decoder.open(path))
    {
        lastError_ = decoder.lastError();
        return false;
    }

    const int rate = decoder.codecContext()->sample_rate;
    if (rate <= 0)
    {
        lastError_ = "The MP3 file has an invalid sample rate.";
        return false;
    }

    AVFrame* frame = av_frame_alloc();
    if (frame == nullptr)
    {
        lastError_ = "Out of memory while reading the MP3 file.";
        return false;
    }

    std::int64_t totalSamples = 0;
    for (;;)
    {
        const int status = decoder.nextFrame(frame);
        if (status < 0)
        {
            lastError_ = decoder.lastError();
            av_frame_free(&frame);
            return false;
        }
        if (status == 0)
        {
            break;
        }
        totalSamples += frame->nb_samples;
        av_frame_unref(frame);
    }
    av_frame_free(&frame);

    if (totalSamples <= 0)
    {
        lastError_ = "The MP3 file does not contain any playable audio.";
        return false;
    }

    sampleRate_ = rate;
    durationSeconds_ = static_cast<double>(totalSamples) / static_cast<double>(rate);
    return true;
}

double FFmpegAudioSource::durationSeconds() const
{
    return durationSeconds_;
}

int FFmpegAudioSource::sampleRate() const
{
    return sampleRate_;
}

const std::string& FFmpegAudioSource::lastError() const
{
    return lastError_;
}
