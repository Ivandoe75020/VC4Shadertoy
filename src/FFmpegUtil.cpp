#include "pch.h"
#include "FFmpegUtil.h"

extern "C"
{
#include <libavutil/error.h>
}

namespace FFmpegUtil
{
    std::string errorString(int code)
    {
        char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
        av_strerror(code, buffer, sizeof(buffer));
        return std::string(buffer);
    }

    std::string toUtf8(const std::filesystem::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }
}
