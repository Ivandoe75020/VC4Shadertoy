#pragma once

#include "ShadertoyProject.h"

#include <cstdint>
#include <vector>

class IShaderRenderer
{
public:
    virtual ~IShaderRenderer() = default;
    virtual bool initialize(const ShadertoyProject& project, int width, int height, int fps) = 0;
    virtual bool renderFrame(double timeSeconds, int frameIndex, std::vector<std::uint8_t>& rgbaPixels) = 0;
};
