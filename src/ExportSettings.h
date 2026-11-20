#pragma once

#include <filesystem>
#include <optional>

enum class Resolution
{
    P720,
    P1080
};

enum class FrameRate
{
    Fps30,
    Fps60
};

enum class ClipDuration
{
    Seconds60,
    Seconds90,
    Seconds120
};

struct ExportSettings
{
    Resolution resolution = Resolution::P720;
    FrameRate frameRate = FrameRate::Fps30;
    ClipDuration clipDuration = ClipDuration::Seconds60;
    std::filesystem::path audioPath;
    std::optional<double> audioDurationSeconds;
    std::filesystem::path outputPath;

    int width() const
    {
        return resolution == Resolution::P1080 ? 1920 : 1280;
    }

    int height() const
    {
        return resolution == Resolution::P1080 ? 1080 : 720;
    }

    int fps() const
    {
        return frameRate == FrameRate::Fps60 ? 60 : 30;
    }

    double durationSeconds() const
    {
        if (!audioPath.empty())
        {
            return audioDurationSeconds.value_or(0.0);
        }
        switch (clipDuration)
        {
        case ClipDuration::Seconds90:
            return 90.0;
        case ClipDuration::Seconds120:
            return 120.0;
        default:
            return 60.0;
        }
    }
};
