#include "pch.h"
#include "OpenGLShaderRenderer.h"

#include <GL/gl.h>
#include <winhttp.h>
#include <wincodec.h>
#include <wincrypt.h>
#include <wrl/client.h>

#include <algorithm>
#include <ctime>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

using GlUint = unsigned int;
using GlInt = int;
using GlSizei = int;
using GlEnum = unsigned int;
using GlBoolean = unsigned char;
using GlFloat = float;

using PFNGLCREATEPROGRAMPROC = GlUint (__stdcall*)(void);
using PFNGLDELETEPROGRAMPROC = void (__stdcall*)(GlUint);
using PFNGLCREATESHADERPROC = GlUint (__stdcall*)(GlEnum);
using PFNGLDELETESHADERPROC = void (__stdcall*)(GlUint);
using PFNGLSHADERSOURCEPROC = void (__stdcall*)(GlUint, GlSizei, char const* const*, int const*);
using PFNGLCOMPILESHADERPROC = void (__stdcall*)(GlUint);
using PFNGLGETSHADERIVPROC = void (__stdcall*)(GlUint, GlEnum, GlInt*);
using PFNGLGETSHADERINFOLOGPROC = void (__stdcall*)(GlUint, GlSizei, GlSizei*, char*);
using PFNGLATTACHSHADERPROC = void (__stdcall*)(GlUint, GlUint);
using PFNGLLINKPROGRAMPROC = void (__stdcall*)(GlUint);
using PFNGLGETPROGRAMIVPROC = void (__stdcall*)(GlUint, GlEnum, GlInt*);
using PFNGLGETPROGRAMINFOLOGPROC = void (__stdcall*)(GlUint, GlSizei, GlSizei*, char*);
using PFNGLUSEPROGRAMPROC = void (__stdcall*)(GlUint);
using PFNGLGENFRAMEBUFFERSPROC = void (__stdcall*)(GlSizei, GlUint*);
using PFNGLDELETEFRAMEBUFFERSPROC = void (__stdcall*)(GlSizei, GlUint const*);
using PFNGLBINDFRAMEBUFFERPROC = void (__stdcall*)(GlEnum, GlUint);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void (__stdcall*)(GlEnum, GlEnum, GlEnum, GlUint, GlInt);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC = GlEnum (__stdcall*)(GlEnum);
using PFNGLGENTEXTURESPROC = void (__stdcall*)(GlSizei, GlUint*);
using PFNGLDELETETEXTURESPROC = void (__stdcall*)(GlSizei, GlUint const*);
using PFNGLBINDTEXTUREPROC = void (__stdcall*)(GlEnum, GlUint);
using PFNGLTEXIMAGE2DPROC = void (__stdcall*)(GlEnum, GlInt, GlInt, GlSizei, GlSizei, GlInt, GlEnum, GlEnum, void const*);
using PFNGLTEXPARAMETERIPROC = void (__stdcall*)(GlEnum, GlEnum, GlInt);
using PFNGLGENERATEMIPMAPPROC = void (__stdcall*)(GlEnum);
using PFNGLGENVERTEXARRAYSPROC = void (__stdcall*)(GlSizei, GlUint*);
using PFNGLDELETEVERTEXARRAYSPROC = void (__stdcall*)(GlSizei, GlUint const*);
using PFNGLBINDVERTEXARRAYPROC = void (__stdcall*)(GlUint);
using PFNGLGENBUFFERSPROC = void (__stdcall*)(GlSizei, GlUint*);
using PFNGLDELETEBUFFERSPROC = void (__stdcall*)(GlSizei, GlUint const*);
using PFNGLBINDBUFFERPROC = void (__stdcall*)(GlEnum, GlUint);
using PFNGLBUFFERDATAPROC = void (__stdcall*)(GlEnum, std::size_t, void const*, GlEnum);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (__stdcall*)(GlUint);
using PFNGLVERTEXATTRIBPOINTERPROC = void (__stdcall*)(GlUint, GlInt, GlEnum, GlBoolean, GlSizei, void const*);
using PFNGLGETUNIFORMLOCATIONPROC = GlInt (__stdcall*)(GlUint, char const*);
using PFNGLUNIFORM1IPROC = void (__stdcall*)(GlInt, GlInt);
using PFNGLUNIFORM1FPROC = void (__stdcall*)(GlInt, GlFloat);
using PFNGLUNIFORM3FPROC = void (__stdcall*)(GlInt, GlFloat, GlFloat, GlFloat);
using PFNGLUNIFORM4FPROC = void (__stdcall*)(GlInt, GlFloat, GlFloat, GlFloat, GlFloat);
using PFNGLACTIVETEXTUREPROC = void (__stdcall*)(GlEnum);
using PFNGLDRAWARRAYSPROC = void (__stdcall*)(GlEnum, GlInt, GlSizei);
using PFNGLREADPIXELSPROC = void (__stdcall*)(GlInt, GlInt, GlSizei, GlSizei, GlEnum, GlEnum, void*);
using PFNWGLCREATECONTEXTATTRIBSARBPROC = HGLRC (__stdcall*)(HDC, HGLRC, int const*);

namespace
{
    constexpr GlEnum GLVC_FRAMEBUFFER = 0x8D40;
    constexpr GlEnum GLVC_RENDERBUFFER = 0x8D41;
    constexpr GlEnum GLVC_COLOR_ATTACHMENT0 = 0x8CE0;
    constexpr GlEnum GLVC_FRAMEBUFFER_COMPLETE = 0x8CD5;
    constexpr GlEnum GLVC_RGBA = 0x1908;
    constexpr GlEnum GLVC_RGBA8 = 0x8058;
    constexpr GlEnum GLVC_TEXTURE_2D = 0x0DE1;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP = 0x8513;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519;
    constexpr GlEnum GLVC_TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A;
    constexpr GlEnum GLVC_TEXTURE_WRAP_S = 0x2802;
    constexpr GlEnum GLVC_TEXTURE_WRAP_T = 0x2803;
    constexpr GlEnum GLVC_TEXTURE_WRAP_R = 0x8072;
    constexpr GlEnum GLVC_TEXTURE_MIN_FILTER = 0x2801;
    constexpr GlEnum GLVC_TEXTURE_MAG_FILTER = 0x2800;
    constexpr GlEnum GLVC_CLAMP_TO_EDGE = 0x812F;
    constexpr GlEnum GLVC_LINEAR = 0x2601;
    constexpr GlEnum GLVC_NEAREST = 0x2600;
    constexpr GlEnum GLVC_NEAREST_MIPMAP_NEAREST = 0x2700;
    constexpr GlEnum GLVC_LINEAR_MIPMAP_LINEAR = 0x2703;
    constexpr GlEnum GLVC_REPEAT = 0x2901;
    constexpr GlEnum GLVC_MIRRORED_REPEAT = 0x8370;
    constexpr GlEnum GLVC_SRGB8_ALPHA8 = 0x8C43;
    constexpr GlEnum GLVC_ARRAY_BUFFER = 0x8892;
    constexpr GlEnum GLVC_STATIC_DRAW = 0x88E4;
    constexpr GlEnum GLVC_FLOAT = 0x1406;
    constexpr GlEnum GLVC_TRIANGLES = 0x0004;
    constexpr GlEnum GLVC_UNSIGNED_BYTE = 0x1401;
    constexpr GlEnum GLVC_TEXTURE0 = 0x84C0;
    constexpr GlEnum GLVC_COLOR_BUFFER_BIT = 0x00004000;
    constexpr GlEnum GLVC_VERTEX_SHADER = 0x8B31;
    constexpr GlEnum GLVC_FRAGMENT_SHADER = 0x8B30;
    constexpr GlEnum GLVC_COMPILE_STATUS = 0x8B81;
    constexpr GlEnum GLVC_LINK_STATUS = 0x8B82;
    constexpr GlEnum GLVC_INFO_LOG_LENGTH = 0x8B84;
    constexpr std::size_t MaxTextureBytes = 64ull * 1024ull * 1024ull;
    constexpr std::size_t MaxTexturePixels = 64ull * 1024ull * 1024ull;

    std::wstring toWide(std::string const& text)
    {
        if (text.empty())
        {
            return {};
        }
        const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), nullptr, 0);
        if (length <= 0)
        {
            return {};
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), length) != length)
        {
            return {};
        }
        return result;
    }

    bool decodeDataUrl(std::string const& source, std::vector<std::uint8_t>& bytes, std::string& error)
    {
        const std::size_t comma = source.find(',');
        if (comma == std::string::npos || source.substr(0, comma).find(";base64") == std::string::npos)
        {
            error = "Embedded texture data must use a base64 data URL.";
            return false;
        }
        const std::string encoded = source.substr(comma + 1);
        DWORD decodedSize = 0;
        if (!::CryptStringToBinaryA(encoded.c_str(), static_cast<DWORD>(encoded.size()),
            CRYPT_STRING_BASE64, nullptr, &decodedSize, nullptr, nullptr) ||
            decodedSize == 0 || decodedSize > MaxTextureBytes)
        {
            error = "The embedded texture data is invalid or too large.";
            return false;
        }
        bytes.resize(decodedSize);
        if (!::CryptStringToBinaryA(encoded.c_str(), static_cast<DWORD>(encoded.size()),
            CRYPT_STRING_BASE64, bytes.data(), &decodedSize, nullptr, nullptr))
        {
            error = "The embedded texture data could not be decoded.";
            return false;
        }
        bytes.resize(decodedSize);
        return true;
    }

    bool downloadHttps(std::wstring const& url, std::vector<std::uint8_t>& bytes, std::string& error)
    {
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwSchemeLength = static_cast<DWORD>(-1);
        parts.dwHostNameLength = static_cast<DWORD>(-1);
        parts.dwUrlPathLength = static_cast<DWORD>(-1);
        parts.dwExtraInfoLength = static_cast<DWORD>(-1);
        if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        {
            error = "Referenced textures must use HTTPS URLs.";
            return false;
        }

        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring target(parts.lpszUrlPath, parts.dwUrlPathLength);
        if (parts.dwExtraInfoLength > 0)
        {
            target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
        }

        HINTERNET session = ::WinHttpOpen(L"VideoConverterForShadertoy/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (session == nullptr)
        {
            error = "Unable to initialize the HTTPS texture downloader.";
            return false;
        }
        ::WinHttpSetTimeouts(session, 10000, 10000, 15000, 15000);

        HINTERNET connection = ::WinHttpConnect(session, host.c_str(), parts.nPort, 0);
        HINTERNET request = connection == nullptr ? nullptr :
            ::WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        bool success = request != nullptr &&
            ::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            ::WinHttpReceiveResponse(request, nullptr);
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (success)
        {
            success = ::WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) && status == 200;
        }
        if (!success)
        {
            error = "The referenced texture could not be downloaded over HTTPS.";
        }
        else
        {
            DWORD available = 0;
            for (;;)
            {
                if (!::WinHttpQueryDataAvailable(request, &available))
                {
                    error = "The referenced texture download was interrupted.";
                    success = false;
                    break;
                }
                if (available == 0)
                {
                    break;
                }
                if (bytes.size() + available > MaxTextureBytes)
                {
                    error = "The referenced texture exceeds the 64 MB size limit.";
                    success = false;
                    break;
                }
                const std::size_t oldSize = bytes.size();
                bytes.resize(oldSize + available);
                DWORD received = 0;
                if (!::WinHttpReadData(request, bytes.data() + oldSize, available, &received) || received == 0)
                {
                    error = "The referenced texture download was interrupted.";
                    success = false;
                    break;
                }
                bytes.resize(oldSize + received);
            }
            if (success && bytes.empty())
            {
                error = "The referenced texture is empty.";
                success = false;
            }
        }

        if (request != nullptr)
        {
            ::WinHttpCloseHandle(request);
        }
        if (connection != nullptr)
        {
            ::WinHttpCloseHandle(connection);
        }
        ::WinHttpCloseHandle(session);
        return success;
    }

    bool readTextureBytes(
        ShaderInput const& input,
        ShadertoyProject const& project,
        std::vector<std::uint8_t>& bytes,
        std::string& error)
    {
        if (input.source.starts_with("data:"))
        {
            return decodeDataUrl(input.source, bytes, error);
        }

        std::string source = input.source;
        if (source.starts_with("http://") || source.starts_with("https://"))
        {
            const std::wstring url = toWide(source);
            if (url.empty())
            {
                error = "The texture URL is not valid UTF-8.";
                return false;
            }
            return downloadHttps(url, bytes, error);
        }

        if (source.starts_with("/media/") || source.starts_with("media/"))
        {
            const std::string urlText = "https://www.shadertoy.com/" +
                (source.starts_with("/") ? source.substr(1) : source);
            const std::wstring url = toWide(urlText);
            if (url.empty())
            {
                error = "The referenced texture URL is not valid UTF-8.";
                return false;
            }
            return downloadHttps(url, bytes, error);
        }

        const std::wstring wideSource = toWide(source);
        if (wideSource.empty())
        {
            error = "The referenced texture path is not valid UTF-8.";
            return false;
        }
        const std::filesystem::path sourcePath{ wideSource };
        const std::filesystem::path resolvedPath = sourcePath.is_absolute()
            ? sourcePath
            : project.sourcePath.parent_path() / sourcePath;
        std::error_code ec;
        const std::uintmax_t size = std::filesystem::file_size(resolvedPath, ec);
        if (ec || size == 0 || size > MaxTextureBytes)
        {
            error = "The referenced texture file cannot be opened, is empty, or exceeds the 64 MB size limit.";
            return false;
        }
        std::ifstream stream(resolvedPath, std::ios::binary);
        if (!stream)
        {
            error = "The referenced texture file cannot be opened.";
            return false;
        }
        bytes.resize(static_cast<std::size_t>(size));
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!stream || static_cast<std::size_t>(stream.gcount()) != bytes.size())
        {
            error = "The referenced texture file could not be read completely.";
            return false;
        }
        return true;
    }

    bool decodeTexture(
        std::vector<std::uint8_t> const& encoded,
        std::vector<std::uint8_t>& pixels,
        int& width,
        int& height,
        std::string& error)
    {
        const HRESULT apartmentResult = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool uninitialize = SUCCEEDED(apartmentResult);
        if (FAILED(apartmentResult) && apartmentResult != RPC_E_CHANGED_MODE)
        {
            error = "Unable to initialize the Windows imaging component.";
            return false;
        }

        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        HRESULT result = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        Microsoft::WRL::ComPtr<IWICStream> stream;
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
        Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
        if (SUCCEEDED(result))
        {
            result = factory->CreateStream(&stream);
        }
        if (SUCCEEDED(result))
        {
            result = stream->InitializeFromMemory(
                const_cast<BYTE*>(encoded.data()), static_cast<DWORD>(encoded.size()));
        }
        if (SUCCEEDED(result))
        {
            result = factory->CreateDecoderFromStream(stream.Get(), nullptr,
                WICDecodeMetadataCacheOnLoad, &decoder);
        }
        if (SUCCEEDED(result))
        {
            result = decoder->GetFrame(0, &frame);
        }
        UINT decodedWidth = 0;
        UINT decodedHeight = 0;
        if (SUCCEEDED(result))
        {
            result = frame->GetSize(&decodedWidth, &decodedHeight);
        }
        if (SUCCEEDED(result) && (decodedWidth == 0 || decodedHeight == 0 ||
            static_cast<std::uint64_t>(decodedWidth) * decodedHeight > MaxTexturePixels))
        {
            result = E_INVALIDARG;
        }
        if (SUCCEEDED(result))
        {
            result = factory->CreateFormatConverter(&converter);
        }
        if (SUCCEEDED(result))
        {
            result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        }
        if (SUCCEEDED(result))
        {
            width = static_cast<int>(decodedWidth);
            height = static_cast<int>(decodedHeight);
            const std::size_t stride = static_cast<std::size_t>(width) * 4;
            pixels.resize(stride * static_cast<std::size_t>(height));
            result = converter->CopyPixels(nullptr, static_cast<UINT>(stride),
                static_cast<UINT>(pixels.size()), pixels.data());
        }
        if (uninitialize)
        {
            ::CoUninitialize();
        }
        if (FAILED(result))
        {
            error = "The texture image format is invalid or unsupported.";
            return false;
        }
        return true;
    }

    template<typename Function>
    Function getProcAddress(char const* name)
    {
        FARPROC address = wglGetProcAddress(name);
        if (address == nullptr)
        {
            address = ::GetProcAddress(::GetModuleHandleW(L"opengl32.dll"), name);
        }
        return reinterpret_cast<Function>(address);
    }

    std::string shaderInfoLog(PFNGLGETSHADERINFOLOGPROC getLog, GlUint shader)
    {
        GlInt length = 0;
        PFNGLGETSHADERIVPROC getStatus = getProcAddress<PFNGLGETSHADERIVPROC>("glGetShaderiv");
        getStatus(shader, GLVC_INFO_LOG_LENGTH, &length);
        if (length <= 1)
        {
            return "Unknown GLSL shader error";
        }
        std::vector<char> log(static_cast<std::size_t>(length), '\0');
        getLog(shader, length, nullptr, log.data());
        return std::string(log.data());
    }

    std::string programInfoLog(PFNGLGETPROGRAMINFOLOGPROC getLog, GlUint program)
    {
        GlInt length = 0;
        PFNGLGETPROGRAMIVPROC getStatus = getProcAddress<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
        getStatus(program, GLVC_INFO_LOG_LENGTH, &length);
        if (length <= 1)
        {
            return "Unknown GLSL link error";
        }
        std::vector<char> log(static_cast<std::size_t>(length), '\0');
        getLog(program, length, nullptr, log.data());
        return std::string(log.data());
    }

    LRESULT CALLBACK hiddenWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_DESTROY)
        {
            ::PostQuitMessage(0);
        }
        return ::DefWindowProcW(window, message, wParam, lParam);
    }

    std::string adaptShadertoyFragment(ShaderPass const& pass, std::string const& commonCode)
    {
        std::string source = commonCode;
        if (!source.empty())
        {
            source.push_back('\n');
        }
        source += pass.code;

        std::string shaderCode;
        std::string extensions;
        int conditionalDepth = 0;
        std::size_t lineStart = 0;

        while (lineStart < source.size())
        {
            const std::size_t lineEnd = source.find('\n', lineStart);
            const std::size_t lineLength = (lineEnd == std::string::npos ? source.size() : lineEnd) - lineStart;
            std::string_view line{ source.data() + lineStart, lineLength };
            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            const std::size_t first = line.find_first_not_of(" \t");
            const std::string_view trimmed = first == std::string_view::npos ? std::string_view{} : line.substr(first);

            if (!trimmed.starts_with("#version"))
            {
                if (trimmed.starts_with("#extension") && conditionalDepth == 0)
                {
                    extensions.append(line);
                    extensions.push_back('\n');
                }
                else if (!trimmed.starts_with("precision "))
                {
                    shaderCode.append(line);
                    shaderCode.push_back('\n');
                }
            }

            if (trimmed.starts_with("#if ") || trimmed.starts_with("#ifdef") || trimmed.starts_with("#ifndef"))
            {
                ++conditionalDepth;
            }
            else if (trimmed.starts_with("#endif") && conditionalDepth > 0)
            {
                --conditionalDepth;
            }

            if (lineEnd == std::string::npos)
            {
                break;
            }
            lineStart = lineEnd + 1;
        }

        std::string declarations =
            "#version 330 core\n" +
            extensions +
            "out vec4 shadertoyOutput;\n"
            "uniform vec3 iResolution;\n"
            "uniform float iTime;\n"
            "uniform float iTimeDelta;\n"
            "uniform float iFrameRate;\n"
            "uniform int iFrame;\n"
            "uniform vec4 iMouse;\n"
            "uniform vec4 iDate;\n"
            "uniform vec3 iChannelResolution[4];\n";
        for (int channel = 0; channel < 4; ++channel)
        {
            const auto input = std::find_if(pass.inputs.begin(), pass.inputs.end(), [&](ShaderInput const& item) {
                return item.channel == channel;
            });
            declarations += "uniform ";
            declarations += input != pass.inputs.end() && input->type == "cubemap" ? "samplerCube " : "sampler2D ";
            declarations += "iChannel" + std::to_string(channel) + ";\n";
        }
        declarations += "#line 1\n";
        declarations += shaderCode;
        declarations +=
            "\nvoid main()\n"
            "{\n"
            "    mainImage(shadertoyOutput, gl_FragCoord.xy);\n"
            "}\n";
        return declarations;
    }
}

OpenGLShaderRenderer::~OpenGLShaderRenderer()
{
    destroy();
}

bool OpenGLShaderRenderer::initialize(const ShadertoyProject& project, int width, int height, int fps)
{
    destroy();
    lastError_.clear();
    commonCode_.clear();
    width_ = width;
    height_ = height;
    fps_ = fps;

    if (width <= 0 || height <= 0 || fps <= 0)
    {
        lastError_ = "Invalid offscreen render dimensions or frame rate.";
        return false;
    }

    std::time_t now = std::time(nullptr);
    std::tm localDate{};
    if (localtime_s(&localDate, &now) != 0)
    {
        lastError_ = "Unable to determine the shader render date.";
        return false;
    }
    dateUniform_ = {
        static_cast<float>(localDate.tm_year + 1900),
        static_cast<float>(localDate.tm_mon + 1),
        static_cast<float>(localDate.tm_mday),
        static_cast<float>(localDate.tm_hour * 3600 + localDate.tm_min * 60 + localDate.tm_sec)
    };

    if (!createWindowAndContext())
    {
        return false;
    }

    vertexSource_ =
        "#version 330 core\n"
        "layout(location = 0) in vec2 position;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "}\n";

    bool hasImagePass = false;
    for (ShaderPass const& shaderPass : project.passes)
    {
        if (shaderPass.type == "common")
        {
            commonCode_ = shaderPass.code;
            continue;
        }
        if (shaderPass.type != "image" && shaderPass.type != "buffer")
        {
            lastError_ = "The pass '" + shaderPass.name + "' cannot be rendered by the OpenGL renderer.";
            return false;
        }

        PassResources pass;
        pass.pass = shaderPass;
        pass.isBuffer = shaderPass.type == "buffer";
        hasImagePass = hasImagePass || shaderPass.type == "image";
        passes_.push_back(std::move(pass));
    }

    if (!hasImagePass)
    {
        lastError_ = "The shader has no Image pass.";
        return false;
    }

    for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
    {
        PassResources& pass = passes_[passIndex];
        for (ShaderInput const& input : pass.pass.inputs)
        {
            if (input.type == "buffer")
            {
                const auto bufferPass = std::find_if(passes_.begin(), passes_.end(), [&](PassResources const& candidate) {
                    return candidate.isBuffer && candidate.pass.outputId == input.id;
                });
                if (bufferPass == passes_.end())
                {
                    lastError_ = "The input buffer of pass '" + pass.pass.name + "' could not be found.";
                    return false;
                }
            }
            else if (input.type != "texture" && input.type != "cubemap")
            {
                lastError_ = "Input type '" + input.type + "' on pass '" + pass.pass.name +
                    "' is not supported by the shader renderer.";
                return false;
            }
        }

        if (!loadPassInputs(pass, project) || !compileAndLinkProgram(pass) || !createPassTargets(pass))
        {
            return false;
        }
    }

    bufferWriteIndex_ = 0;
    return true;
}

bool OpenGLShaderRenderer::createWindowAndContext()
{
    HINSTANCE instance = ::GetModuleHandleW(nullptr);
    wchar_t const* className = L"VideoConverterOffscreenGL";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = hiddenWindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    ::RegisterClassW(&windowClass);

    HWND window = ::CreateWindowExW(
        0, className, L"Offscreen Shader",
        WS_POPUP, 0, 0, width_, height_,
        nullptr, nullptr, instance, nullptr);
    if (window == nullptr)
    {
        lastError_ = "Unable to create the offscreen render window.";
        return false;
    }
    windowHandle_ = window;

    HDC deviceContext = ::GetDC(window);
    if (deviceContext == nullptr)
    {
        lastError_ = "Unable to acquire the offscreen device context.";
        return false;
    }
    deviceContext_ = deviceContext;

    PIXELFORMATDESCRIPTOR descriptor{};
    descriptor.nSize = sizeof(descriptor);
    descriptor.nVersion = 1;
    descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    descriptor.iPixelType = PFD_TYPE_RGBA;
    descriptor.cColorBits = 32;
    descriptor.cDepthBits = 24;
    const int pixelFormat = ::ChoosePixelFormat(deviceContext, &descriptor);
    if (pixelFormat == 0 || !::SetPixelFormat(deviceContext, pixelFormat, &descriptor))
    {
        lastError_ = "Unable to configure the OpenGL pixel format.";
        return false;
    }

    HGLRC renderContext = ::wglCreateContext(deviceContext);
    if (renderContext == nullptr)
    {
        lastError_ = "Unable to create the OpenGL render context.";
        return false;
    }
    if (!::wglMakeCurrent(deviceContext, renderContext))
    {
        ::wglDeleteContext(renderContext);
        lastError_ = "Unable to activate the OpenGL render context.";
        return false;
    }

    PFNWGLCREATECONTEXTATTRIBSARBPROC createContextAttributes =
        getProcAddress<PFNWGLCREATECONTEXTATTRIBSARBPROC>("wglCreateContextAttribsARB");
    if (createContextAttributes != nullptr)
    {
        int attributes[] = {
            0x2091, 3,
            0x2092, 3,
            0x2094, 0x00000001,
            0
        };
        HGLRC modernContext = createContextAttributes(deviceContext, nullptr, attributes);
        if (modernContext != nullptr)
        {
            ::wglMakeCurrent(nullptr, nullptr);
            ::wglDeleteContext(renderContext);
            if (::wglMakeCurrent(deviceContext, modernContext))
            {
                renderContext = modernContext;
            }
            else
            {
                ::wglDeleteContext(modernContext);
                lastError_ = "Unable to activate the OpenGL 3.3 context.";
                return false;
            }
        }
    }

    renderContext_ = renderContext;
    ::glViewport(0, 0, width_, height_);
    return true;
}

bool OpenGLShaderRenderer::compileAndLinkProgram(PassResources& pass)
{
    PFNGLCREATEPROGRAMPROC createProgram = getProcAddress<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
    PFNGLCREATESHADERPROC createShader = getProcAddress<PFNGLCREATESHADERPROC>("glCreateShader");
    PFNGLSHADERSOURCEPROC shaderSource = getProcAddress<PFNGLSHADERSOURCEPROC>("glShaderSource");
    PFNGLCOMPILESHADERPROC compileShader = getProcAddress<PFNGLCOMPILESHADERPROC>("glCompileShader");
    PFNGLGETSHADERIVPROC getShaderStatus = getProcAddress<PFNGLGETSHADERIVPROC>("glGetShaderiv");
    PFNGLGETSHADERINFOLOGPROC getShaderLog = getProcAddress<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
    PFNGLATTACHSHADERPROC attachShader = getProcAddress<PFNGLATTACHSHADERPROC>("glAttachShader");
    PFNGLLINKPROGRAMPROC linkProgram = getProcAddress<PFNGLLINKPROGRAMPROC>("glLinkProgram");
    PFNGLGETPROGRAMIVPROC getProgramStatus = getProcAddress<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
    PFNGLGETPROGRAMINFOLOGPROC getProgramLog = getProcAddress<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
    if (createProgram == nullptr || createShader == nullptr || shaderSource == nullptr ||
        compileShader == nullptr || getShaderStatus == nullptr || getShaderLog == nullptr ||
        attachShader == nullptr || linkProgram == nullptr || getProgramStatus == nullptr ||
        getProgramLog == nullptr)
    {
        lastError_ = "Required OpenGL shader functions are unavailable.";
        return false;
    }

    GlUint vertexShader = createShader(GLVC_VERTEX_SHADER);
    char const* vertexSource = vertexSource_.c_str();
    shaderSource(vertexShader, 1, &vertexSource, nullptr);
    compileShader(vertexShader);
    GlInt vertexStatus = 0;
    getShaderStatus(vertexShader, GLVC_COMPILE_STATUS, &vertexStatus);
    if (vertexStatus == 0)
    {
        lastError_ = "Vertex shader compilation failed for pass '" + pass.pass.name + "': " +
            shaderInfoLog(getShaderLog, vertexShader);
        PFNGLDELETESHADERPROC deleteShader = getProcAddress<PFNGLDELETESHADERPROC>("glDeleteShader");
        if (deleteShader != nullptr)
        {
            deleteShader(vertexShader);
        }
        return false;
    }

    GlUint fragmentShader = createShader(GLVC_FRAGMENT_SHADER);
    const std::string fragmentSource = adaptShadertoyFragment(pass.pass, commonCode_);
    char const* fragmentSourceData = fragmentSource.c_str();
    shaderSource(fragmentShader, 1, &fragmentSourceData, nullptr);
    compileShader(fragmentShader);
    GlInt fragmentStatus = 0;
    getShaderStatus(fragmentShader, GLVC_COMPILE_STATUS, &fragmentStatus);
    if (fragmentStatus == 0)
    {
        lastError_ = "Fragment shader compilation failed for pass '" + pass.pass.name + "': " +
            shaderInfoLog(getShaderLog, fragmentShader);
        PFNGLDELETESHADERPROC deleteShader = getProcAddress<PFNGLDELETESHADERPROC>("glDeleteShader");
        if (deleteShader != nullptr)
        {
            deleteShader(vertexShader);
            deleteShader(fragmentShader);
        }
        return false;
    }

    pass.program = createProgram();
    attachShader(pass.program, vertexShader);
    attachShader(pass.program, fragmentShader);
    linkProgram(pass.program);
    GlInt linkStatus = 0;
    getProgramStatus(pass.program, GLVC_LINK_STATUS, &linkStatus);
    PFNGLDELETESHADERPROC deleteShader = getProcAddress<PFNGLDELETESHADERPROC>("glDeleteShader");
    if (deleteShader != nullptr)
    {
        deleteShader(vertexShader);
        deleteShader(fragmentShader);
    }
    if (linkStatus == 0)
    {
        lastError_ = "Shader link failed for pass '" + pass.pass.name + "': " + programInfoLog(getProgramLog, pass.program);
        return false;
    }

    PFNGLGETUNIFORMLOCATIONPROC getUniformLocation =
        getProcAddress<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    if (getUniformLocation != nullptr)
    {
        pass.resolutionLocation = getUniformLocation(pass.program, "iResolution");
        pass.timeLocation = getUniformLocation(pass.program, "iTime");
        pass.timeDeltaLocation = getUniformLocation(pass.program, "iTimeDelta");
        pass.frameRateLocation = getUniformLocation(pass.program, "iFrameRate");
        pass.frameLocation = getUniformLocation(pass.program, "iFrame");
        pass.mouseLocation = getUniformLocation(pass.program, "iMouse");
        pass.dateLocation = getUniformLocation(pass.program, "iDate");
        for (int channel = 0; channel < 4; ++channel)
        {
            const std::string name = "iChannel" + std::to_string(channel);
            pass.channelLocations[static_cast<std::size_t>(channel)] =
                getUniformLocation(pass.program, name.c_str());
            const std::string resolutionName = "iChannelResolution[" + std::to_string(channel) + "]";
            pass.channelResolutionLocations[static_cast<std::size_t>(channel)] =
                getUniformLocation(pass.program, resolutionName.c_str());
        }
    }

    if (vertexArray_ != 0)
    {
        return true;
    }

    PFNGLGENVERTEXARRAYSPROC genVertexArrays = getProcAddress<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
    PFNGLBINDVERTEXARRAYPROC bindVertexArray = getProcAddress<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    PFNGLGENBUFFERSPROC genBuffers = getProcAddress<PFNGLGENBUFFERSPROC>("glGenBuffers");
    PFNGLBINDBUFFERPROC bindBuffer = getProcAddress<PFNGLBINDBUFFERPROC>("glBindBuffer");
    PFNGLBUFFERDATAPROC bufferData = getProcAddress<PFNGLBUFFERDATAPROC>("glBufferData");
    PFNGLENABLEVERTEXATTRIBARRAYPROC enableAttribute =
        getProcAddress<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray");
    PFNGLVERTEXATTRIBPOINTERPROC vertexAttribute =
        getProcAddress<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer");
    if (genVertexArrays == nullptr || bindVertexArray == nullptr || genBuffers == nullptr ||
        bindBuffer == nullptr || bufferData == nullptr || enableAttribute == nullptr || vertexAttribute == nullptr)
    {
        lastError_ = "Required OpenGL vertex array functions are unavailable.";
        return false;
    }

    genVertexArrays(1, &vertexArray_);
    bindVertexArray(vertexArray_);
    genBuffers(1, &vertexBuffer_);
    bindBuffer(GLVC_ARRAY_BUFFER, vertexBuffer_);
    float vertices[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f
    };
    bufferData(GLVC_ARRAY_BUFFER, sizeof(vertices), vertices, GLVC_STATIC_DRAW);
    enableAttribute(0);
    vertexAttribute(0, 2, GLVC_FLOAT, GL_FALSE, 0, nullptr);
    bindVertexArray(0);

    return true;
}

bool OpenGLShaderRenderer::createPassTargets(PassResources& pass)
{
    PFNGLGENTEXTURESPROC genTextures = getProcAddress<PFNGLGENTEXTURESPROC>("glGenTextures");
    PFNGLBINDTEXTUREPROC bindTexture = getProcAddress<PFNGLBINDTEXTUREPROC>("glBindTexture");
    PFNGLTEXIMAGE2DPROC texImage = getProcAddress<PFNGLTEXIMAGE2DPROC>("glTexImage2D");
    PFNGLTEXPARAMETERIPROC texParameter = getProcAddress<PFNGLTEXPARAMETERIPROC>("glTexParameteri");
    PFNGLGENFRAMEBUFFERSPROC genFramebuffers = getProcAddress<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    PFNGLBINDFRAMEBUFFERPROC bindFramebuffer = getProcAddress<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture =
        getProcAddress<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    PFNGLCHECKFRAMEBUFFERSTATUSPROC framebufferStatus =
        getProcAddress<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus");
    if (genTextures == nullptr || bindTexture == nullptr || texImage == nullptr || texParameter == nullptr ||
        genFramebuffers == nullptr || bindFramebuffer == nullptr || framebufferTexture == nullptr ||
        framebufferStatus == nullptr)
    {
        lastError_ = "Required OpenGL framebuffer functions are unavailable.";
        return false;
    }

    const std::size_t targetCount = pass.isBuffer ? 2 : 1;
    for (std::size_t target = 0; target < targetCount; ++target)
    {
        genTextures(1, &pass.textures[target]);
        bindTexture(GLVC_TEXTURE_2D, pass.textures[target]);
        texImage(GLVC_TEXTURE_2D, 0, GLVC_RGBA8, width_, height_, 0, GLVC_RGBA, GLVC_UNSIGNED_BYTE, nullptr);
        texParameter(GLVC_TEXTURE_2D, GLVC_TEXTURE_MIN_FILTER, GLVC_LINEAR);
        texParameter(GLVC_TEXTURE_2D, GLVC_TEXTURE_MAG_FILTER, GLVC_LINEAR);
        texParameter(GLVC_TEXTURE_2D, GLVC_TEXTURE_WRAP_S, GLVC_CLAMP_TO_EDGE);
        texParameter(GLVC_TEXTURE_2D, GLVC_TEXTURE_WRAP_T, GLVC_CLAMP_TO_EDGE);

        genFramebuffers(1, &pass.framebuffers[target]);
        bindFramebuffer(GLVC_FRAMEBUFFER, pass.framebuffers[target]);
        framebufferTexture(GLVC_FRAMEBUFFER, GLVC_COLOR_ATTACHMENT0, GLVC_TEXTURE_2D, pass.textures[target], 0);
        if (framebufferStatus(GLVC_FRAMEBUFFER) != GLVC_FRAMEBUFFER_COMPLETE)
        {
            lastError_ = "Unable to create an offscreen framebuffer for pass '" + pass.pass.name + "'.";
            return false;
        }

        if (pass.isBuffer)
        {
            ::glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            ::glClear(GLVC_COLOR_BUFFER_BIT);
        }
    }

    return true;
}

bool OpenGLShaderRenderer::loadPassInputs(PassResources& pass, const ShadertoyProject& project)
{
    PFNGLGENTEXTURESPROC genTextures = getProcAddress<PFNGLGENTEXTURESPROC>("glGenTextures");
    PFNGLBINDTEXTUREPROC bindTexture = getProcAddress<PFNGLBINDTEXTUREPROC>("glBindTexture");
    PFNGLTEXIMAGE2DPROC texImage = getProcAddress<PFNGLTEXIMAGE2DPROC>("glTexImage2D");
    PFNGLTEXPARAMETERIPROC texParameter = getProcAddress<PFNGLTEXPARAMETERIPROC>("glTexParameteri");
    PFNGLGENERATEMIPMAPPROC generateMipmaps = getProcAddress<PFNGLGENERATEMIPMAPPROC>("glGenerateMipmap");
    if (genTextures == nullptr || bindTexture == nullptr || texImage == nullptr || texParameter == nullptr)
    {
        lastError_ = "Required OpenGL texture functions are unavailable.";
        return false;
    }

    constexpr std::array<GlEnum, 6> CubeFaces{
        GLVC_TEXTURE_CUBE_MAP_POSITIVE_X,
        GLVC_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GLVC_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GLVC_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GLVC_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GLVC_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    constexpr std::array<std::array<int, 2>, 6> CubeCells{
        std::array<int, 2>{ 2, 1 },
        std::array<int, 2>{ 0, 1 },
        std::array<int, 2>{ 1, 0 },
        std::array<int, 2>{ 1, 2 },
        std::array<int, 2>{ 1, 1 },
        std::array<int, 2>{ 3, 1 }
    };

    for (ShaderInput const& input : pass.pass.inputs)
    {
        if (input.type == "buffer")
        {
            continue;
        }

        std::vector<std::uint8_t> encoded;
        std::string error;
        if (!readTextureBytes(input, project, encoded, error))
        {
            lastError_ = "Unable to load iChannel" + std::to_string(input.channel) +
                " of pass '" + pass.pass.name + "': " + error;
            return false;
        }

        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        if (!decodeTexture(encoded, pixels, width, height, error))
        {
            lastError_ = "Unable to decode iChannel" + std::to_string(input.channel) +
                " of pass '" + pass.pass.name + "': " + error;
            return false;
        }

        const std::size_t channel = static_cast<std::size_t>(input.channel);
        const bool cube = input.type == "cubemap";
        const GlEnum target = cube ? GLVC_TEXTURE_CUBE_MAP : GLVC_TEXTURE_2D;
        if (cube && (width % 4 != 0 || height % 3 != 0 || width / 4 != height / 3))
        {
            lastError_ = "iChannel" + std::to_string(input.channel) + " of pass '" + pass.pass.name +
                "' must be a 4-by-3 cubemap cross image.";
            return false;
        }

        genTextures(1, &pass.inputTextures[channel]);
        bindTexture(target, pass.inputTextures[channel]);
        const GlInt internalFormat = input.srgb ? static_cast<GlInt>(GLVC_SRGB8_ALPHA8) :
            static_cast<GlInt>(GLVC_RGBA8);
        if (cube)
        {
            const int faceSize = width / 4;
            std::vector<std::uint8_t> facePixels(
                static_cast<std::size_t>(faceSize) * static_cast<std::size_t>(faceSize) * 4);
            for (std::size_t face = 0; face < CubeFaces.size(); ++face)
            {
                for (int row = 0; row < faceSize; ++row)
                {
                    const int sourceRow = input.vflip
                        ? height - 1 - (CubeCells[face][1] * faceSize + row)
                        : CubeCells[face][1] * faceSize + row;
                    const std::size_t sourceOffset =
                        (static_cast<std::size_t>(sourceRow) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(CubeCells[face][0] * faceSize)) * 4;
                    const std::size_t destinationOffset = static_cast<std::size_t>(row) *
                        static_cast<std::size_t>(faceSize) * 4;
                    std::memcpy(facePixels.data() + destinationOffset, pixels.data() + sourceOffset,
                        static_cast<std::size_t>(faceSize) * 4);
                }
                texImage(CubeFaces[face], 0, internalFormat, faceSize, faceSize, 0,
                    GLVC_RGBA, GLVC_UNSIGNED_BYTE, facePixels.data());
            }
        }
        else
        {
            if (input.vflip)
            {
                const std::size_t rowSize = static_cast<std::size_t>(width) * 4;
                std::vector<std::uint8_t> flipped(pixels.size());
                for (int row = 0; row < height; ++row)
                {
                    std::memcpy(flipped.data() + static_cast<std::size_t>(row) * rowSize,
                        pixels.data() + static_cast<std::size_t>(height - 1 - row) * rowSize, rowSize);
                }
                pixels.swap(flipped);
            }
            texImage(target, 0, internalFormat, width, height, 0, GLVC_RGBA, GLVC_UNSIGNED_BYTE, pixels.data());
        }

        const bool nearest = input.filter == "nearest" || input.filter == "mipmapnearest";
        const bool mipmapped = input.filter == "mipmap" || input.filter == "mipmapnearest";
        const GlInt magFilter = nearest ? static_cast<GlInt>(GLVC_NEAREST) : static_cast<GlInt>(GLVC_LINEAR);
        const GlInt minFilter = mipmapped
            ? static_cast<GlInt>(nearest ? GLVC_NEAREST_MIPMAP_NEAREST : GLVC_LINEAR_MIPMAP_LINEAR)
            : magFilter;
        GlInt wrap = static_cast<GlInt>(GLVC_CLAMP_TO_EDGE);
        if (input.wrap == "repeat")
        {
            wrap = static_cast<GlInt>(GLVC_REPEAT);
        }
        else if (input.wrap == "mirror")
        {
            wrap = static_cast<GlInt>(GLVC_MIRRORED_REPEAT);
        }
        if (mipmapped && generateMipmaps != nullptr)
        {
            generateMipmaps(target);
        }
        texParameter(target, GLVC_TEXTURE_MIN_FILTER, minFilter);
        texParameter(target, GLVC_TEXTURE_MAG_FILTER, magFilter);
        texParameter(target, GLVC_TEXTURE_WRAP_S, wrap);
        texParameter(target, GLVC_TEXTURE_WRAP_T, wrap);
        if (cube)
        {
            texParameter(target, GLVC_TEXTURE_WRAP_R, static_cast<GlInt>(GLVC_CLAMP_TO_EDGE));
        }
        pass.cubeChannels[channel] = cube;
        pass.inputWidths[channel] = cube ? width / 4 : width;
        pass.inputHeights[channel] = cube ? height / 3 : height;
    }

    return true;
}

bool OpenGLShaderRenderer::renderFrame(
    double timeSeconds,
    int frameIndex,
    std::vector<std::uint8_t>& rgbaPixels)
{
    lastError_.clear();
    if (renderContext_ == nullptr || passes_.empty())
    {
        lastError_ = "The offscreen OpenGL renderer is not initialized.";
        return false;
    }

    HDC deviceContext = reinterpret_cast<HDC>(deviceContext_);
    HGLRC renderContext = reinterpret_cast<HGLRC>(renderContext_);
    if (!::wglMakeCurrent(deviceContext, renderContext))
    {
        lastError_ = "Unable to activate the offscreen OpenGL context.";
        return false;
    }

    PFNGLBINDFRAMEBUFFERPROC bindFramebuffer = getProcAddress<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    PFNGLUSEPROGRAMPROC useProgram = getProcAddress<PFNGLUSEPROGRAMPROC>("glUseProgram");
    PFNGLUNIFORM3FPROC uniformResolution = getProcAddress<PFNGLUNIFORM3FPROC>("glUniform3f");
    PFNGLUNIFORM4FPROC uniform4f = getProcAddress<PFNGLUNIFORM4FPROC>("glUniform4f");
    PFNGLUNIFORM1FPROC uniformTime = getProcAddress<PFNGLUNIFORM1FPROC>("glUniform1f");
    PFNGLUNIFORM1IPROC uniform1i = getProcAddress<PFNGLUNIFORM1IPROC>("glUniform1i");
    PFNGLACTIVETEXTUREPROC activeTexture = getProcAddress<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    PFNGLBINDTEXTUREPROC bindTexture = getProcAddress<PFNGLBINDTEXTUREPROC>("glBindTexture");
    PFNGLBINDVERTEXARRAYPROC bindVertexArray = getProcAddress<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    PFNGLDRAWARRAYSPROC drawArrays = getProcAddress<PFNGLDRAWARRAYSPROC>("glDrawArrays");
    PFNGLREADPIXELSPROC readPixels = getProcAddress<PFNGLREADPIXELSPROC>("glReadPixels");
    if (bindFramebuffer == nullptr || useProgram == nullptr ||
        uniformResolution == nullptr || uniform4f == nullptr || uniformTime == nullptr || uniform1i == nullptr ||
        activeTexture == nullptr || bindTexture == nullptr || bindVertexArray == nullptr ||
        drawArrays == nullptr || readPixels == nullptr)
    {
        lastError_ = "Required OpenGL rendering functions are unavailable.";
        return false;
    }

    const int previousBufferIndex = 1 - bufferWriteIndex_;
    std::size_t imagePassIndex = passes_.size();
    for (std::size_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
    {
        PassResources& pass = passes_[passIndex];
        if (!pass.isBuffer)
        {
            imagePassIndex = passIndex;
        }

        const std::size_t targetIndex = pass.isBuffer ? static_cast<std::size_t>(bufferWriteIndex_) : 0;
        bindFramebuffer(GLVC_FRAMEBUFFER, pass.framebuffers[targetIndex]);
        ::glViewport(0, 0, width_, height_);
        useProgram(pass.program);
        if (pass.resolutionLocation >= 0)
        {
            uniformResolution(pass.resolutionLocation, static_cast<GlFloat>(width_), static_cast<GlFloat>(height_), 1.0f);
        }
        if (pass.timeLocation >= 0)
        {
            uniformTime(pass.timeLocation, static_cast<GlFloat>(timeSeconds));
        }
        if (pass.timeDeltaLocation >= 0)
        {
            uniformTime(pass.timeDeltaLocation, 1.0f / static_cast<GlFloat>(fps_));
        }
        if (pass.frameRateLocation >= 0)
        {
            uniformTime(pass.frameRateLocation, static_cast<GlFloat>(fps_));
        }
        if (pass.frameLocation >= 0)
        {
            uniform1i(pass.frameLocation, frameIndex);
        }
        if (pass.mouseLocation >= 0)
        {
            uniform4f(pass.mouseLocation, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        if (pass.dateLocation >= 0)
        {
            uniform4f(pass.dateLocation, dateUniform_[0], dateUniform_[1], dateUniform_[2], dateUniform_[3]);
        }

        for (int channel = 0; channel < 4; ++channel)
        {
            const std::size_t channelIndex = static_cast<std::size_t>(channel);
            activeTexture(GLVC_TEXTURE0 + static_cast<GlEnum>(channel));
            GlEnum textureTarget = pass.cubeChannels[channelIndex] ? GLVC_TEXTURE_CUBE_MAP : GLVC_TEXTURE_2D;
            bindTexture(textureTarget, 0);
            GlFloat channelWidth = static_cast<GlFloat>(pass.inputWidths[channelIndex]);
            GlFloat channelHeight = static_cast<GlFloat>(pass.inputHeights[channelIndex]);

            const auto input = std::find_if(pass.pass.inputs.begin(), pass.pass.inputs.end(), [&](ShaderInput const& item) {
                return item.channel == channel;
            });
            if (input != pass.pass.inputs.end())
            {
                if (input->type == "buffer")
                {
                    const auto sourcePass = std::find_if(passes_.begin(), passes_.end(), [&](PassResources const& candidate) {
                        return candidate.isBuffer && candidate.pass.outputId == input->id;
                    });
                    if (sourcePass == passes_.end())
                    {
                        lastError_ = "The input buffer of pass '" + pass.pass.name + "' could not be found.";
                        return false;
                    }

                    const std::size_t sourcePassIndex = static_cast<std::size_t>(std::distance(passes_.begin(), sourcePass));
                    const int sourceTextureIndex =
                        sourcePassIndex < passIndex ? bufferWriteIndex_ : previousBufferIndex;
                    textureTarget = GLVC_TEXTURE_2D;
                    bindTexture(textureTarget, sourcePass->textures[static_cast<std::size_t>(sourceTextureIndex)]);
                    channelWidth = static_cast<GlFloat>(width_);
                    channelHeight = static_cast<GlFloat>(height_);
                }
                else
                {
                    textureTarget = pass.cubeChannels[channelIndex] ? GLVC_TEXTURE_CUBE_MAP : GLVC_TEXTURE_2D;
                    bindTexture(textureTarget, pass.inputTextures[channelIndex]);
                }
            }
            if (pass.channelLocations[channelIndex] >= 0)
            {
                uniform1i(pass.channelLocations[channelIndex], channel);
            }
            if (pass.channelResolutionLocations[channelIndex] >= 0)
            {
                uniformResolution(pass.channelResolutionLocations[channelIndex], channelWidth, channelHeight,
                    input == pass.pass.inputs.end() ? 0.0f : 1.0f);
            }
        }

        bindVertexArray(vertexArray_);
        drawArrays(GLVC_TRIANGLES, 0, 3);
        bindVertexArray(0);
    }

    if (imagePassIndex == passes_.size())
    {
        lastError_ = "The shader has no Image pass.";
        return false;
    }
    bindFramebuffer(GLVC_FRAMEBUFFER, passes_[imagePassIndex].framebuffers[0]);

    const std::size_t rowSize = static_cast<std::size_t>(width_) * 4;
    rgbaPixels.resize(rowSize * static_cast<std::size_t>(height_));
    std::vector<std::uint8_t> bottomUp(rgbaPixels.size());
    readPixels(0, 0, width_, height_, GLVC_RGBA, GLVC_UNSIGNED_BYTE, bottomUp.data());

    for (int row = 0; row < height_; ++row)
    {
        std::uint8_t* destination = rgbaPixels.data() + static_cast<std::size_t>(row) * rowSize;
        std::uint8_t const* source =
            bottomUp.data() + static_cast<std::size_t>(height_ - 1 - row) * rowSize;
        std::memcpy(destination, source, rowSize);
    }
    bufferWriteIndex_ = previousBufferIndex;
    return true;
}

void OpenGLShaderRenderer::destroy()
{
    if (renderContext_ != nullptr)
    {
        ::wglMakeCurrent(reinterpret_cast<HDC>(deviceContext_), reinterpret_cast<HGLRC>(renderContext_));
    }

    if (renderContext_ != nullptr)
    {
        PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers =
            getProcAddress<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
        PFNGLDELETETEXTURESPROC deleteTextures =
            getProcAddress<PFNGLDELETETEXTURESPROC>("glDeleteTextures");
        PFNGLDELETEPROGRAMPROC deleteProgram =
            getProcAddress<PFNGLDELETEPROGRAMPROC>("glDeleteProgram");
        for (PassResources const& pass : passes_)
        {
            if (deleteFramebuffers != nullptr)
            {
                for (std::uint32_t framebuffer : pass.framebuffers)
                {
                    if (framebuffer != 0)
                    {
                        deleteFramebuffers(1, &framebuffer);
                    }
                }
            }
            if (deleteTextures != nullptr)
            {
                for (std::uint32_t texture : pass.textures)
                {
                    if (texture != 0)
                    {
                        deleteTextures(1, &texture);
                    }
                }
                for (std::uint32_t texture : pass.inputTextures)
                {
                    if (texture != 0)
                    {
                        deleteTextures(1, &texture);
                    }
                }
            }
            if (deleteProgram != nullptr && pass.program != 0)
            {
                deleteProgram(pass.program);
            }
        }
        if (vertexArray_ != 0)
        {
            PFNGLDELETEVERTEXARRAYSPROC deleteVertexArrays =
                getProcAddress<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
            if (deleteVertexArrays != nullptr)
            {
                deleteVertexArrays(1, &vertexArray_);
            }
        }
        if (vertexBuffer_ != 0)
        {
            PFNGLDELETEBUFFERSPROC deleteBuffers =
                getProcAddress<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
            if (deleteBuffers != nullptr)
            {
                deleteBuffers(1, &vertexBuffer_);
            }
        }
        ::wglMakeCurrent(nullptr, nullptr);
        ::wglDeleteContext(reinterpret_cast<HGLRC>(renderContext_));
    }
    if (deviceContext_ != nullptr)
    {
        ::ReleaseDC(reinterpret_cast<HWND>(windowHandle_), reinterpret_cast<HDC>(deviceContext_));
    }
    if (windowHandle_ != nullptr)
    {
        ::DestroyWindow(reinterpret_cast<HWND>(windowHandle_));
    }

    windowHandle_ = nullptr;
    deviceContext_ = nullptr;
    renderContext_ = nullptr;
    passes_.clear();
    vertexArray_ = 0;
    vertexBuffer_ = 0;
    bufferWriteIndex_ = 0;
    commonCode_.clear();
    width_ = 0;
    height_ = 0;
}

std::string const& OpenGLShaderRenderer::lastError() const
{
    return lastError_;
}
