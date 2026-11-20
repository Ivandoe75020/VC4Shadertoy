#pragma once

#include <filesystem>
#include <string>

namespace FFmpegUtil
{
    std::string errorString(int code);
    std::string toUtf8(const std::filesystem::path& path);
}
