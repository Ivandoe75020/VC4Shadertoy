#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ShaderInput
{
    int channel = 0;
    int id = -1;
    std::string type;
    std::string source;
    std::string filter = "linear";
    std::string wrap = "clamp";
    bool vflip = true;
    bool srgb = false;
};

struct ShaderPass
{
    std::string name;
    std::string type;
    std::string code;
    int outputId = -1;
    std::vector<ShaderInput> inputs;
};

struct ShadertoyProject
{
    std::filesystem::path sourcePath;
    std::string id;
    std::string name;
    std::string author;
    std::string description;
    std::vector<ShaderPass> passes;
    std::vector<std::string> warnings;
};
