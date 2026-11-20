#include "pch.h"
#include "ExportPipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    constexpr int PreviewWidth = 320;
    constexpr int PreviewHeight = 180;

    std::vector<std::uint8_t> downsamplePreview(
        std::vector<std::uint8_t> const& source,
        int width,
        int height)
    {
        std::vector<std::uint8_t> preview(static_cast<std::size_t>(PreviewWidth) * PreviewHeight * 4);
        for (int y = 0; y < PreviewHeight; ++y)
        {
            const int sourceY = y * height / PreviewHeight;
            for (int x = 0; x < PreviewWidth; ++x)
            {
                const int sourceX = x * width / PreviewWidth;
                const std::size_t sourceOffset =
                    (static_cast<std::size_t>(sourceY) * width + sourceX) * 4;
                const std::size_t destinationOffset =
                    (static_cast<std::size_t>(y) * PreviewWidth + x) * 4;
                std::copy_n(source.data() + sourceOffset, 4, preview.data() + destinationOffset);
            }
        }
        return preview;
    }
}

ExportPipeline::ExportPipeline(IShaderRenderer& renderer, IVideoEncoder& encoder, IAudioSource& audio)
    : renderer_(renderer), encoder_(encoder), audio_(audio)
{
}

ExportTask::ExportTask(
    std::shared_future<ExportStatus> result,
    std::shared_ptr<std::atomic<bool>> cancelRequested)
    : result_(std::move(result)), cancelRequested_(std::move(cancelRequested))
{
}

void ExportTask::cancel() const
{
    cancelRequested_->store(true);
}

bool ExportTask::isReady() const
{
    return result_.valid() && result_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

ExportStatus ExportTask::wait() const
{
    return result_.get();
}

ExportTask ExportPipeline::startAsync(
    ShadertoyProject project,
    ExportSettings settings,
    ProgressCallback onProgress)
{
    auto cancelRequested = std::make_shared<std::atomic<bool>>(false);
    auto result = std::async(std::launch::async,
        [this, project = std::move(project), settings = std::move(settings),
            onProgress = std::move(onProgress), cancelRequested]() {
            return run(project, settings, onProgress, *cancelRequested);
        }).share();
    return ExportTask{ std::move(result), std::move(cancelRequested) };
}

ExportStatus ExportPipeline::run(
    const ShadertoyProject& project,
    const ExportSettings& settings,
    const ProgressCallback& onProgress,
    const std::atomic<bool>& cancelRequested)
{
    const int width = settings.width();
    const int height = settings.height();
    const int fps = settings.fps();
    const bool hasAudio = !settings.audioPath.empty();
    if (cancelRequested.load())
    {
        return ExportStatus::Cancelled;
    }

    double duration = settings.durationSeconds();
    if (hasAudio)
    {
        if (!audio_.open(settings.audioPath))
        {
            return ExportStatus::Failed;
        }
        duration = audio_.durationSeconds();
    }

    if (!std::isfinite(duration) || duration <= 0.0 ||
        duration * static_cast<double>(fps) > static_cast<double>(std::numeric_limits<int>::max()))
    {
        return ExportStatus::Failed;
    }
    const int totalFrames = static_cast<int>(std::llround(duration * static_cast<double>(fps)));
    if (totalFrames <= 0)
    {
        return ExportStatus::Failed;
    }

    if (!renderer_.initialize(project, width, height, fps))
    {
        return ExportStatus::Failed;
    }
    if (cancelRequested.load())
    {
        return ExportStatus::Cancelled;
    }

    EncoderConfig config;
    config.outputPath = settings.outputPath;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.audioPath = hasAudio ? settings.audioPath : std::filesystem::path{};
    config.durationSeconds = duration;

    if (!encoder_.open(config))
    {
        return ExportStatus::Failed;
    }

    std::vector<std::uint8_t> pixels;
    const std::size_t stride = static_cast<std::size_t>(width) * 4;

    for (int i = 0; i < totalFrames; ++i)
    {
        if (cancelRequested.load())
        {
            encoder_.abort();
            return ExportStatus::Cancelled;
        }

        const double timeSeconds = static_cast<double>(i) / static_cast<double>(fps);

        if (!renderer_.renderFrame(timeSeconds, i, pixels) || !encoder_.writeVideoFrame(pixels.data(), stride))
        {
            encoder_.abort();
            return ExportStatus::Failed;
        }

        if (onProgress && (i % std::max(1, fps / 5) == 0 || i + 1 == totalFrames))
        {
            onProgress(ExportProgress{ i + 1, totalFrames, downsamplePreview(pixels, width, height) });
        }
        else if (onProgress)
        {
            onProgress(ExportProgress{ i + 1, totalFrames, {} });
        }
    }

    if (cancelRequested.load())
    {
        encoder_.abort();
        return ExportStatus::Cancelled;
    }
    if (!encoder_.finalize())
    {
        encoder_.abort();
        return ExportStatus::Failed;
    }
    return ExportStatus::Completed;
}
