#include "pch.h"
#include "FFmpegInfo.h"

#include <cstring>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace
{
    bool startsWithLgpl(const char* text)
    {
        return text != nullptr && std::strncmp(text, "LGPL", 4) == 0;
    }

    bool contains(const std::string& text, const char* token)
    {
        return text.find(token) != std::string::npos;
    }
}

namespace FFmpegInfo
{
    std::string buildConfiguration()
    {
        return std::string(avcodec_configuration());
    }

    std::string versionText()
    {
        return std::string(av_version_info());
    }

    std::string licenseText()
    {
        return std::string(avcodec_license());
    }

    bool isLgplOnly()
    {
        if (!startsWithLgpl(avutil_license()) || !startsWithLgpl(avcodec_license()) ||
            !startsWithLgpl(avformat_license()) || !startsWithLgpl(swscale_license()) ||
            !startsWithLgpl(swresample_license()))
        {
            return false;
        }

        const std::string configuration = buildConfiguration();
        return !contains(configuration, "--enable-gpl") && !contains(configuration, "--enable-nonfree") &&
            !contains(configuration, "--enable-libx264") && !contains(configuration, "--enable-libx265") &&
            !contains(configuration, "--enable-version3");
    }
}
