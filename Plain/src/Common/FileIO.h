#pragma once
#include "pch.h"

namespace fs = std::filesystem;

bool checkLastChangeTime(const fs::path& path, fs::file_time_type* outLastChangeTime);

//slower, more reliable version of checkLastChangeTime
//checkLastChangeTime sometimes fails, due to other processes accessing the file at the time
//slow version tries multiple times if failing, sleeping tryWaitTimeMs between tries
bool checkLastChangeTimeSlow(const fs::path& path, fs::file_time_type* outLastChangeTime, const int tries, const int tryWaitTimeMs);

bool loadTextFile(const std::filesystem::path& absolutePath, std::vector<char>* outText);
bool loadBinaryFile(const std::filesystem::path& absolutePath, std::vector<uint32_t>* outData);
void writeBinaryFile(const std::filesystem::path absolutePath, const std::vector<uint32_t>& data);