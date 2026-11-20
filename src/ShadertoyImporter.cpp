#include "pch.h"
#include "ShadertoyImporter.h"
#include "Json.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <system_error>

namespace
{
    constexpr std::uintmax_t MaxFileSize = 64ull * 1024ull * 1024ull;
    constexpr int ChannelCount = 4;
    constexpr std::size_t MaxBufferPasses = 4;

    std::string lowerCase(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string stringField(const JsonValue* object, std::string_view key, const std::string& fallback = std::string())
    {
        if (object == nullptr)
        {
            return fallback;
        }
        const JsonValue* value = object->find(key);
        if (value == nullptr || !value->isString())
        {
            return fallback;
        }
        return value->text;
    }

    bool intField(const JsonValue* object, std::string_view key, int& result)
    {
        if (object == nullptr)
        {
            return false;
        }
        const JsonValue* value = object->find(key);
        if (value == nullptr)
        {
            return false;
        }
        if (value->isNumber())
        {
            result = static_cast<int>(value->number);
            return true;
        }
        if (value->isString() && !value->text.empty())
        {
            int parsed = 0;
            const char* begin = value->text.data();
            const char* end = begin + value->text.size();
            const auto conversion = std::from_chars(begin, end, parsed);
            if (conversion.ec == std::errc{} && conversion.ptr == end)
            {
                result = parsed;
                return true;
            }
        }
        return false;
    }

    bool boolField(const JsonValue* object, std::string_view key, bool fallback)
    {
        if (object == nullptr)
        {
            return fallback;
        }
        const JsonValue* value = object->find(key);
        if (value == nullptr)
        {
            return fallback;
        }
        if (value->isBoolean())
        {
            return value->boolean;
        }
        if (value->isString())
        {
            const std::string text = lowerCase(value->text);
            if (text == "true")
            {
                return true;
            }
            if (text == "false")
            {
                return false;
            }
        }
        return fallback;
    }

    std::string defaultPassName(const std::string& type, std::size_t bufferIndex)
    {
        if (type == "common")
        {
            return "Common";
        }
        if (type == "buffer")
        {
            return std::string("Buffer ") + static_cast<char>('A' + std::min<std::size_t>(bufferIndex, 25));
        }
        if (type == "sound")
        {
            return "Sound";
        }
        return "Image";
    }

    bool isSupportedInputType(const std::string& type)
    {
        return type == "texture" || type == "cubemap" || type == "volume" || type == "buffer" || type == "keyboard";
    }

    std::string describeUnsupportedInput(const std::string& type)
    {
        if (type == "webcam")
        {
            return "a webcam";
        }
        if (type == "mic")
        {
            return "a microphone";
        }
        if (type == "music" || type == "musicstream")
        {
            return "a music or sound input";
        }
        if (type == "video")
        {
            return "a video";
        }
        return "an input of type '" + type + "'";
    }

    std::string channelName(int channel)
    {
        return "iChannel" + std::to_string(channel);
    }

    bool parseInput(const JsonValue& json, const std::string& passName, ShaderInput& input, std::string& error)
    {
        if (!json.isObject())
        {
            error = "The pass '" + passName + "' contains an invalid input.";
            return false;
        }

        int channel = 0;
        if (!intField(&json, "channel", channel) || channel < 0 || channel >= ChannelCount)
        {
            error = "The pass '" + passName + "' has an input with an invalid channel number (expected 0 to 3).";
            return false;
        }

        input.channel = channel;
        input.type = lowerCase(stringField(&json, "ctype"));
        input.source = stringField(&json, "src");
        intField(&json, "id", input.id);

        const JsonValue* sampler = json.find("sampler");
        if (sampler != nullptr && sampler->isObject())
        {
            const std::string filter = lowerCase(stringField(sampler, "filter"));
            const std::string wrap = lowerCase(stringField(sampler, "wrap"));
            if (!filter.empty())
            {
                input.filter = filter;
            }
            if (!wrap.empty())
            {
                input.wrap = wrap;
            }
            input.vflip = boolField(sampler, "vflip", true);
            input.srgb = boolField(sampler, "srgb", false);
        }

        if (!isSupportedInputType(input.type))
        {
            error = "The pass '" + passName + "' uses " + channelName(channel) + " with " + describeUnsupportedInput(input.type) +
                ". This input is not supported: only textures, cubemaps, volumes, buffers and keyboard inputs can be exported.";
            return false;
        }

        if ((input.type == "texture" || input.type == "cubemap" || input.type == "volume") && input.source.empty())
        {
            error = "The input " + channelName(channel) + " of the pass '" + passName + "' has no texture file (missing input).";
            return false;
        }

        return true;
    }

    bool parsePass(const JsonValue& json, std::size_t bufferIndex, ShaderPass& pass, bool& skipped, ShadertoyProject& project, std::string& error)
    {
        skipped = false;
        if (!json.isObject())
        {
            error = "The file contains an invalid render pass.";
            return false;
        }

        pass.type = lowerCase(stringField(&json, "type"));
        pass.name = stringField(&json, "name");
        if (pass.type.empty())
        {
            error = "A render pass has no type.";
            return false;
        }
        if (pass.name.empty())
        {
            pass.name = defaultPassName(pass.type, bufferIndex);
        }

        if (pass.type == "sound")
        {
            project.warnings.push_back("The sound pass of this shader is ignored. Load an MP3 file to add music to the video.");
            skipped = true;
            return true;
        }
        if (pass.type == "cubemap")
        {
            error = "Cubemap passes ('" + pass.name + "') are not supported.";
            return false;
        }
        if (pass.type != "image" && pass.type != "buffer" && pass.type != "common")
        {
            error = "The pass '" + pass.name + "' has an unknown type '" + pass.type + "'.";
            return false;
        }

        pass.code = stringField(&json, "code");
        if (pass.code.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            error = "The pass '" + pass.name + "' has no shader code.";
            return false;
        }

        const JsonValue* outputs = json.find("outputs");
        if (outputs != nullptr && outputs->isArray() && !outputs->items.empty())
        {
            intField(&outputs->items.front(), "id", pass.outputId);
        }

        const JsonValue* inputs = json.find("inputs");
        if (inputs != nullptr && inputs->isArray())
        {
            for (const JsonValue& inputJson : inputs->items)
            {
                ShaderInput input;
                if (!parseInput(inputJson, pass.name, input, error))
                {
                    return false;
                }
                pass.inputs.push_back(std::move(input));
            }
        }

        return true;
    }
}

bool parseShadertoyJson(const std::string& text, ShadertoyProject& project, std::string& error)
{
    project = ShadertoyProject{};
    error.clear();

    JsonValue root;
    std::string parseError;
    if (!parseJson(text, root, parseError))
    {
        error = "The file is not valid JSON: " + parseError + ".";
        return false;
    }

    const JsonValue* shader = &root;
    if (root.isArray())
    {
        if (root.items.empty())
        {
            error = "The file does not contain any shader.";
            return false;
        }
        if (root.items.size() > 1)
        {
            project.warnings.push_back("The file contains " + std::to_string(root.items.size()) + " shaders. Only the first one is loaded.");
        }
        shader = &root.items.front();
    }

    if (shader->isObject())
    {
        const JsonValue* wrapped = shader->find("Shader");
        if (wrapped != nullptr && wrapped->isObject())
        {
            shader = wrapped;
        }
        else
        {
            const JsonValue* reported = shader->find("Error");
            if (reported != nullptr && reported->isString())
            {
                error = "Shadertoy reported an error instead of a shader: " + reported->text;
                return false;
            }
        }
    }

    if (!shader->isObject())
    {
        error = "This file is not a Shadertoy shader export.";
        return false;
    }

    const JsonValue* renderpass = shader->find("renderpass");
    if (renderpass == nullptr || !renderpass->isArray() || renderpass->items.empty())
    {
        error = "This file is not a Shadertoy shader export: no render pass was found.";
        return false;
    }

    const JsonValue* info = shader->find("info");
    if (info != nullptr && !info->isObject())
    {
        info = nullptr;
    }
    project.id = stringField(info, "id");
    project.name = stringField(info, "name", "Untitled shader");
    project.author = stringField(info, "username", "Unknown author");
    project.description = stringField(info, "description");
    if (project.name.empty())
    {
        project.name = "Untitled shader";
    }

    std::size_t bufferCount = 0;
    std::size_t imageCount = 0;
    std::size_t commonCount = 0;

    for (const JsonValue& passJson : renderpass->items)
    {
        ShaderPass pass;
        bool skipped = false;
        if (!parsePass(passJson, bufferCount, pass, skipped, project, error))
        {
            return false;
        }
        if (skipped)
        {
            continue;
        }

        if (pass.type == "buffer")
        {
            ++bufferCount;
        }
        else if (pass.type == "image")
        {
            ++imageCount;
        }
        else if (pass.type == "common")
        {
            ++commonCount;
        }
        project.passes.push_back(std::move(pass));
    }

    if (imageCount == 0)
    {
        error = "The shader has no Image pass.";
        return false;
    }
    if (imageCount > 1 || commonCount > 1)
    {
        error = "The shader has more than one Image pass or Common pass.";
        return false;
    }
    if (bufferCount > MaxBufferPasses)
    {
        error = "The shader has " + std::to_string(bufferCount) + " buffer passes. Only Buffer A to Buffer D are supported.";
        return false;
    }

    for (const ShaderPass& pass : project.passes)
    {
        for (const ShaderInput& input : pass.inputs)
        {
            if (input.type != "buffer")
            {
                continue;
            }
            const bool exists = std::any_of(project.passes.begin(), project.passes.end(), [&](const ShaderPass& other) {
                return other.type == "buffer" && other.outputId == input.id && input.id >= 0;
            });
            if (!exists)
            {
                error = "The input " + channelName(input.channel) + " of the pass '" + pass.name + "' uses a buffer that does not exist in this shader (missing input).";
                return false;
            }
        }
    }

    return true;
}

bool importShadertoyJson(const std::filesystem::path& path, ShadertoyProject& project, std::string& error)
{
    project = ShadertoyProject{};
    error.clear();

    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec)
    {
        error = "The file cannot be opened.";
        return false;
    }
    if (size == 0)
    {
        error = "The file is empty.";
        return false;
    }
    if (size > MaxFileSize)
    {
        error = "The file is too large to be a Shadertoy export.";
        return false;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        error = "The file cannot be opened.";
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad())
    {
        error = "The file cannot be read.";
        return false;
    }

    if (!parseShadertoyJson(text, project, error))
    {
        return false;
    }

    project.sourcePath = path;
    return true;
}
