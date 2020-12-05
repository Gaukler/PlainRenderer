#pragma once
#include "pch.h"

namespace fs = std::filesystem;

bool checkLastChangeTime(const fs::path path, fs::file_time_type* outLastChangeTime);

bool loadTextFile(const std::filesystem::path& absolutePath, std::vector<char>* outText);
bool loadBinaryFile(const std::filesystem::path& absolutePath, std::vector<uint32_t>* outData);
void writeBinaryFile(const std::filesystem::path absolutePath, const std::vector<uint32_t>& data);