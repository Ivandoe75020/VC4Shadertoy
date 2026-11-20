#pragma once

#include "ExportSettings.h"
#include "IAudioSource.h"
#include "IShaderRenderer.h"
#include "IVideoEncoder.h"
#include "ShadertoyProject.h"

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <vector>

enum class ExportStatus
{
    Completed,
    Cancelled,
    Failed
};

struct ExportProgress
{
    int frameIndex = 0;
    int totalFrames = 0;
    std::vector<std::uint8_t> previewPixels;
};

using ProgressCallback = std::function<void(const ExportProgress&)>;

class ExportTask
{
public:
    void cancel() const;
    bool isReady() const;
    ExportStatus wait() const;

private:
    friend class ExportPipeline;

    ExportTask(std::shared_future<ExportStatus> result, std::shared_ptr<std::atomic<bool>> cancelRequested);

    std::shared_future<ExportStatus> result_;
    std::shared_ptr<std::atomic<bool>> cancelRequested_;
};

class ExportPipeline
{
public:
    ExportPipeline(IShaderRenderer& renderer, IVideoEncoder& encoder, IAudioSource& audio);

    ExportTask startAsync(
        ShadertoyProject project,
        ExportSettings settings,
        ProgressCallback onProgress);

    ExportStatus run(
        const ShadertoyProject& project,
        const ExportSettings& settings,
        const ProgressCallback& onProgress,
        const std::atomic<bool>& cancelRequested);

private:
    IShaderRenderer& renderer_;
    IVideoEncoder& encoder_;
    IAudioSource& audio_;
};
