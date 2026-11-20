#pragma once

#include "ShadertoyProject.h"

#include <filesystem>
#include <string>

bool parseShadertoyJson(const std::string& text, ShadertoyProject& project, std::string& error);
bool importShadertoyJson(const std::filesystem::path& path, ShadertoyProject& project, std::string& error);
