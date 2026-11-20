#include "pch.h"
#include "FFmpegEncoder.h"
#include "FFmpegUtil.h"
#include "Mp3Decoder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <system_error>
#include <utility>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace
{
    constexpr int AudioBitRate = 192000;
    constexpr int AudioChannels = 2;
    constexpr double AudioLeadSeconds = 1.0;
    constexpr double VideoBitsPerPixel = 0.2;
    constexpr std::int64_t MinVideoBitRate = 2000000;
    constexpr std::int64_t MaxVideoBitRate = 50000000;

    AVPixelFormat choosePixelFormat(const AVCodecContext* context, const AVCodec* codec)
    {
        const void* configs = nullptr;
        int count = 0;
        const int result = avcodec_get_supported_config(context, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, &count);
        if (result < 0 || configs == nullptr || count <= 0)
        {
            return AV_PIX_FMT_NV12;
        }

        const AVPixelFormat* formats = static_cast<const AVPixelFormat*>(configs);
        for (int i = 0; i < count; ++i)
        {
            if (formats[i] == AV_PIX_FMT_NV12)
            {
                return AV_PIX_FMT_NV12;
            }
        }
        for (int i = 0; i < count; ++i)
        {
            if (formats[i] == AV_PIX_FMT_YUV420P)
            {
                return AV_PIX_FMT_YUV420P;
            }
        }
        return formats[0];
    }
}

struct FFmpegEncoder::Impl
{
    std::string videoCodecName;
    std::string lastError;
    EncoderConfig config;

    AVFormatContext* format = nullptr;
    AVPacket* packet = nullptr;
    bool ioOpened = false;
    bool opened = false;
    bool finished = false;

    AVCodecContext* videoContext = nullptr;
    AVStream* videoStream = nullptr;
    AVFrame* videoFrame = nullptr;
    SwsContext* scaler = nullptr;
    std::int64_t nextVideoPts = 0;

    bool hasAudio = false;
    Mp3Decoder decoder;
    AVCodecContext* audioContext = nullptr;
    AVStream* audioStream = nullptr;
    SwrContext* resampler = nullptr;
    AVAudioFifo* fifo = nullptr;
    AVFrame* decodedFrame = nullptr;
    bool decoderFinished = false;
    std::int64_t audioSamplesWritten = 0;
    std::int64_t audioSampleLimit = 0;
    int audioRate = 0;

    bool fail(const std::string& message, int code = 0)
    {
        lastError = message;
        if (code < 0)
        {
            lastError += ": " + FFmpegUtil::errorString(code);
        }
        return false;
    }

    void release()
    {
        if (scaler != nullptr)
        {
            sws_freeContext(scaler);
            scaler = nullptr;
        }
        if (videoFrame != nullptr)
        {
            av_frame_free(&videoFrame);
        }
        if (decodedFrame != nullptr)
        {
            av_frame_free(&decodedFrame);
        }
        if (fifo != nullptr)
        {
            av_audio_fifo_free(fifo);
            fifo = nullptr;
        }
        if (resampler != nullptr)
        {
            swr_free(&resampler);
        }
        if (videoContext != nullptr)
        {
            avcodec_free_context(&videoContext);
        }
        if (audioContext != nullptr)
        {
            avcodec_free_context(&audioContext);
        }
        if (packet != nullptr)
        {
            av_packet_free(&packet);
        }
        if (format != nullptr)
        {
            if (ioOpened)
            {
                avio_closep(&format->pb);
                ioOpened = false;
            }
            avformat_free_context(format);
            format = nullptr;
        }
        decoder.close();
        videoStream = nullptr;
        audioStream = nullptr;
        opened = false;
    }

    bool writePackets(AVCodecContext* context, AVStream* stream)
    {
        for (;;)
        {
            const int result = avcodec_receive_packet(context, packet);
            if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            {
                return true;
            }
            if (result < 0)
            {
                return fail("Encoding failed", result);
            }

            av_packet_rescale_ts(packet, context->time_base, stream->time_base);
            packet->stream_index = stream->index;

            const int writeResult = av_interleaved_write_frame(format, packet);
            if (writeResult < 0)
            {
                return fail("Writing the MP4 file failed", writeResult);
            }
        }
    }

    bool encodeFrame(AVCodecContext* context, AVStream* stream, AVFrame* frame)
    {
        const int result = avcodec_send_frame(context, frame);
        if (result < 0 && result != AVERROR_EOF)
        {
            return fail("Encoding failed", result);
        }
        return writePackets(context, stream);
    }

    bool openVideo()
    {
        const AVCodec* codec = avcodec_find_encoder_by_name(videoCodecName.c_str());
        if (codec == nullptr)
        {
            return fail("The H.264 encoder is not available. Media Foundation may be missing on this system");
        }

        videoStream = avformat_new_stream(format, nullptr);
        videoContext = avcodec_alloc_context3(codec);
        if (videoStream == nullptr || videoContext == nullptr)
        {
            return fail("Out of memory while creating the video encoder");
        }

        const AVPixelFormat pixelFormat = choosePixelFormat(videoContext, codec);

        const std::int64_t bitRate = std::clamp(
            static_cast<std::int64_t>(static_cast<double>(config.width) * config.height * config.fps * VideoBitsPerPixel),
            MinVideoBitRate,
            MaxVideoBitRate);

        videoContext->codec_type = AVMEDIA_TYPE_VIDEO;
        videoContext->width = config.width;
        videoContext->height = config.height;
        videoContext->pix_fmt = pixelFormat;
        videoContext->time_base = AVRational{ 1, config.fps };
        videoContext->framerate = AVRational{ config.fps, 1 };
        videoContext->gop_size = config.fps * 2;
        videoContext->max_b_frames = 0;
        videoContext->bit_rate = bitRate;
        videoContext->color_range = AVCOL_RANGE_MPEG;
        videoContext->colorspace = AVCOL_SPC_BT709;
        videoContext->color_primaries = AVCOL_PRI_BT709;
        videoContext->color_trc = AVCOL_TRC_BT709;
        if ((format->oformat->flags & AVFMT_GLOBALHEADER) != 0)
        {
            videoContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int result = avcodec_open2(videoContext, codec, nullptr);
        if (result < 0)
        {
            return fail("The video encoder cannot be started", result);
        }

        result = avcodec_parameters_from_context(videoStream->codecpar, videoContext);
        if (result < 0)
        {
            return fail("The video stream cannot be configured", result);
        }
        videoStream->time_base = videoContext->time_base;
        videoStream->avg_frame_rate = videoContext->framerate;

        videoFrame = av_frame_alloc();
        if (videoFrame == nullptr)
        {
            return fail("Out of memory while creating the video frame");
        }
        videoFrame->format = pixelFormat;
        videoFrame->width = config.width;
        videoFrame->height = config.height;
        result = av_frame_get_buffer(videoFrame, 0);
        if (result < 0)
        {
            return fail("Out of memory while allocating the video frame", result);
        }

        scaler = sws_getContext(
            config.width, config.height, AV_PIX_FMT_RGBA,
            config.width, config.height, pixelFormat,
            SWS_BILINEAR | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);
        if (scaler == nullptr)
        {
            return fail("The RGB to YUV converter cannot be created");
        }

        const int* coefficients = sws_getCoefficients(SWS_CS_ITU709);
        sws_setColorspaceDetails(scaler, coefficients, 1, coefficients, 0, 0, 1 << 16, 1 << 16);

        return true;
    }

    bool openAudio()
    {
        if (!decoder.open(config.audioPath))
        {
            return fail(decoder.lastError());
        }

        const AVCodecContext* input = decoder.codecContext();
        audioRate = input->sample_rate;
        if (audioRate <= 0)
        {
            return fail("The MP3 file has an invalid sample rate");
        }

        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (codec == nullptr)
        {
            return fail("The AAC encoder is not available");
        }

        audioStream = avformat_new_stream(format, nullptr);
        audioContext = avcodec_alloc_context3(codec);
        if (audioStream == nullptr || audioContext == nullptr)
        {
            return fail("Out of memory while creating the audio encoder");
        }

        audioContext->codec_type = AVMEDIA_TYPE_AUDIO;
        audioContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        audioContext->sample_rate = audioRate;
        audioContext->bit_rate = AudioBitRate;
        audioContext->time_base = AVRational{ 1, audioRate };
        av_channel_layout_default(&audioContext->ch_layout, AudioChannels);
        if ((format->oformat->flags & AVFMT_GLOBALHEADER) != 0)
        {
            audioContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        int result = avcodec_open2(audioContext, codec, nullptr);
        if (result < 0)
        {
            return fail("The AAC encoder cannot be started", result);
        }

        result = avcodec_parameters_from_context(audioStream->codecpar, audioContext);
        if (result < 0)
        {
            return fail("The audio stream cannot be configured", result);
        }
        audioStream->time_base = audioContext->time_base;

        result = swr_alloc_set_opts2(
            &resampler,
            &audioContext->ch_layout, audioContext->sample_fmt, audioRate,
            &input->ch_layout, input->sample_fmt, input->sample_rate,
            0, nullptr);
        if (result < 0)
        {
            return fail("The audio resampler cannot be created", result);
        }
        result = swr_init(resampler);
        if (result < 0)
        {
            return fail("The audio resampler cannot be started", result);
        }

        fifo = av_audio_fifo_alloc(audioContext->sample_fmt, AudioChannels, 1);
        decodedFrame = av_frame_alloc();
        if (fifo == nullptr || decodedFrame == nullptr)
        {
            return fail("Out of memory while preparing the audio");
        }

        audioSampleLimit = static_cast<std::int64_t>(std::llround(config.durationSeconds * audioRate));
        hasAudio = true;
        return true;
    }

    bool pushToFifo(const std::uint8_t* const* input, int inputSamples)
    {
        const int capacity = swr_get_out_samples(resampler, inputSamples);
        if (capacity <= 0)
        {
            return true;
        }

        std::uint8_t** buffer = nullptr;
        int lineSize = 0;
        int result = av_samples_alloc_array_and_samples(&buffer, &lineSize, AudioChannels, capacity, audioContext->sample_fmt, 0);
        if (result < 0)
        {
            return fail("Out of memory while converting the audio", result);
        }

        const int converted = swr_convert(resampler, buffer, capacity, input, inputSamples);
        bool ok = true;
        if (converted < 0)
        {
            ok = fail("Audio conversion failed", converted);
        }
        else if (converted > 0)
        {
            result = av_audio_fifo_write(fifo, reinterpret_cast<void**>(buffer), converted);
            if (result < converted)
            {
                ok = fail("Out of memory while buffering the audio");
            }
        }

        av_freep(&buffer[0]);
        av_freep(&buffer);
        return ok;
    }

    bool decodeMore()
    {
        const int status = decoder.nextFrame(decodedFrame);
        if (status < 0)
        {
            return fail(decoder.lastError());
        }
        if (status == 0)
        {
            decoderFinished = true;
            return pushToFifo(nullptr, 0);
        }

        const bool ok = pushToFifo(decodedFrame->extended_data, decodedFrame->nb_samples);
        av_frame_unref(decodedFrame);
        return ok;
    }

    bool pumpAudio(double untilSeconds, bool flush)
    {
        if (!hasAudio)
        {
            return true;
        }

        const int frameSize = audioContext->frame_size > 0 ? audioContext->frame_size : 1024;

        for (;;)
        {
            if (audioSamplesWritten >= audioSampleLimit)
            {
                return true;
            }
            if (!flush && static_cast<double>(audioSamplesWritten) / audioRate >= untilSeconds)
            {
                return true;
            }

            while (!decoderFinished && av_audio_fifo_size(fifo) < frameSize)
            {
                if (!decodeMore())
                {
                    return false;
                }
            }

            const int available = av_audio_fifo_size(fifo);
            if (available <= 0)
            {
                return true;
            }

            const int take = static_cast<int>(std::min<std::int64_t>(
                std::min(frameSize, available),
                audioSampleLimit - audioSamplesWritten));

            AVFrame* frame = av_frame_alloc();
            if (frame == nullptr)
            {
                return fail("Out of memory while encoding the audio");
            }
            frame->nb_samples = take;
            frame->format = audioContext->sample_fmt;
            frame->sample_rate = audioRate;
            av_channel_layout_copy(&frame->ch_layout, &audioContext->ch_layout);

            int result = av_frame_get_buffer(frame, 0);
            if (result >= 0)
            {
                result = av_audio_fifo_read(fifo, reinterpret_cast<void**>(frame->data), take);
                if (result < take)
                {
                    result = AVERROR(EIO);
                }
                else
                {
                    result = 0;
                }
            }
            if (result < 0)
            {
                av_frame_free(&frame);
                return fail("Reading the buffered audio failed", result);
            }

            frame->pts = audioSamplesWritten;
            const bool ok = encodeFrame(audioContext, audioStream, frame);
            av_frame_free(&frame);
            if (!ok)
            {
                return false;
            }
            audioSamplesWritten += take;
        }
    }
};

FFmpegEncoder::FFmpegEncoder()
    : FFmpegEncoder("h264_mf")
{
}

FFmpegEncoder::FFmpegEncoder(std::string videoCodecName)
    : impl_(std::make_unique<Impl>())
{
    impl_->videoCodecName = std::move(videoCodecName);
    av_log_set_level(AV_LOG_ERROR);
}

FFmpegEncoder::~FFmpegEncoder()
{
    if (impl_ && impl_->opened && !impl_->finished)
    {
        abort();
    }
}

bool FFmpegEncoder::open(const EncoderConfig& config)
{
    Impl& s = *impl_;
    s.release();
    s.lastError.clear();
    s.finished = false;
    s.hasAudio = false;
    s.decoderFinished = false;
    s.audioSamplesWritten = 0;
    s.audioSampleLimit = 0;
    s.nextVideoPts = 0;
    s.config = config;

    if (config.width <= 0 || config.height <= 0 || (config.width % 2) != 0 || (config.height % 2) != 0)
    {
        return s.fail("The video size must be positive and even");
    }
    if (config.fps <= 0)
    {
        return s.fail("The frame rate must be positive");
    }
    if (config.outputPath.empty())
    {
        return s.fail("No output file was chosen");
    }

    const std::string outputPath = FFmpegUtil::toUtf8(config.outputPath);

    int result = avformat_alloc_output_context2(&s.format, nullptr, "mp4", outputPath.c_str());
    if (result < 0 || s.format == nullptr)
    {
        s.release();
        return s.fail("The MP4 muxer cannot be created", result);
    }

    s.packet = av_packet_alloc();
    if (s.packet == nullptr)
    {
        s.release();
        return s.fail("Out of memory while creating the encoder");
    }

    if (!s.openVideo() || (!config.audioPath.empty() && !s.openAudio()))
    {
        const std::string message = s.lastError;
        s.release();
        s.lastError = message;
        return false;
    }

    result = avio_open(&s.format->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
    if (result < 0)
    {
        s.release();
        return s.fail("The output file cannot be created", result);
    }
    s.ioOpened = true;

    AVDictionary* options = nullptr;
    av_dict_set(&options, "movflags", "faststart", 0);
    result = avformat_write_header(s.format, &options);
    av_dict_free(&options);
    if (result < 0)
    {
        s.release();
        std::error_code ignored;
        std::filesystem::remove(config.outputPath, ignored);
        return s.fail("The MP4 header cannot be written", result);
    }

    s.opened = true;
    return true;
}

bool FFmpegEncoder::writeVideoFrame(const std::uint8_t* rgbaPixels, std::size_t strideBytes)
{
    Impl& s = *impl_;
    if (!s.opened || s.finished)
    {
        return s.fail("The encoder is not open");
    }

    int result = av_frame_make_writable(s.videoFrame);
    if (result < 0)
    {
        return s.fail("The video frame cannot be reused", result);
    }

    const std::uint8_t* sourceData[4] = { rgbaPixels, nullptr, nullptr, nullptr };
    const int sourceStride[4] = { static_cast<int>(strideBytes), 0, 0, 0 };
    const int rows = sws_scale(s.scaler, sourceData, sourceStride, 0, s.config.height, s.videoFrame->data, s.videoFrame->linesize);
    if (rows != s.config.height)
    {
        return s.fail("The RGB to YUV conversion failed");
    }

    s.videoFrame->pts = s.nextVideoPts;
    if (!s.encodeFrame(s.videoContext, s.videoStream, s.videoFrame))
    {
        return false;
    }

    const double videoSeconds = static_cast<double>(s.nextVideoPts) / static_cast<double>(s.config.fps);
    ++s.nextVideoPts;

    return s.pumpAudio(videoSeconds + AudioLeadSeconds, false);
}

bool FFmpegEncoder::finalize()
{
    Impl& s = *impl_;
    if (!s.opened || s.finished)
    {
        return s.fail("The encoder is not open");
    }

    bool ok = s.pumpAudio(0.0, true);
    if (ok && s.hasAudio)
    {
        ok = s.encodeFrame(s.audioContext, s.audioStream, nullptr);
    }
    if (ok)
    {
        ok = s.encodeFrame(s.videoContext, s.videoStream, nullptr);
    }
    if (ok)
    {
        const int result = av_write_trailer(s.format);
        if (result < 0)
        {
            ok = s.fail("The MP4 file cannot be finalized", result);
        }
    }

    const std::string message = s.lastError;
    const std::filesystem::path output = s.config.outputPath;
    s.finished = true;
    s.release();
    s.lastError = message;

    if (!ok)
    {
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
    }
    return ok;
}

void FFmpegEncoder::abort()
{
    Impl& s = *impl_;
    const std::filesystem::path output = s.config.outputPath;
    const bool hadOutput = s.format != nullptr;
    s.finished = true;
    s.release();

    if (hadOutput && !output.empty())
    {
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
    }
}

const std::string& FFmpegEncoder::lastError() const
{
    return impl_->lastError;
}
