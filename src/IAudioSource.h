#pragma once

#include <filesystem>

class IAudioSource
{
public:
    virtual ~IAudioSource() = default;
    virtual bool open(const std::filesystem::path& path) = 0;
    virtual double durationSeconds() const = 0;
};
