#pragma once

#include "IShaderRenderer.h"

#include <array>
#include <string>
#include <vector>

class OpenGLShaderRenderer final : public IShaderRenderer
{
public:
    OpenGLShaderRenderer() = default;
    ~OpenGLShaderRenderer() override;

    bool initialize(const ShadertoyProject& project, int width, int height, int fps) override;
    bool renderFrame(double timeSeconds, int frameIndex, std::vector<std::uint8_t>& rgbaPixels) override;

    std::string const& lastError() const;

private:
    struct PassResources
    {
        ShaderPass pass;
        std::uint32_t program = 0;
        std::array<std::uint32_t, 2> framebuffers{};
        std::array<std::uint32_t, 2> textures{};
        std::array<std::uint32_t, 4> inputTextures{};
        std::array<int, 4> channelLocations{ -1, -1, -1, -1 };
        std::array<int, 4> channelResolutionLocations{ -1, -1, -1, -1 };
        std::array<bool, 4> cubeChannels{};
        std::array<int, 4> inputWidths{};
        std::array<int, 4> inputHeights{};
        int resolutionLocation = -1;
        int timeLocation = -1;
        int timeDeltaLocation = -1;
        int frameRateLocation = -1;
        int frameLocation = -1;
        int mouseLocation = -1;
        int dateLocation = -1;
        bool isBuffer = false;
    };

    bool createWindowAndContext();
    bool createPassTargets(PassResources& pass);
    bool loadPassInputs(PassResources& pass, const ShadertoyProject& project);
    bool compileAndLinkProgram(PassResources& pass);
    void destroy();

    void* windowHandle_ = nullptr;
    void* deviceContext_ = nullptr;
    void* renderContext_ = nullptr;
    std::uint32_t vertexArray_ = 0;
    std::uint32_t vertexBuffer_ = 0;
    std::vector<PassResources> passes_;
    int bufferWriteIndex_ = 0;
    int fps_ = 30;
    std::array<float, 4> dateUniform_{};
    std::string commonCode_;
    int width_ = 0;
    int height_ = 0;
    std::string vertexSource_;
    std::string lastError_;
};
