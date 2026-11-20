#pragma once

#include <string>

namespace FFmpegInfo
{
    bool isLgplOnly();
    std::string versionText();
    std::string licenseText();
    std::string buildConfiguration();
}
