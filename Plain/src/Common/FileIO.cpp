#include "FileIO.h"
#include <Windows.h>

bool checkLastChangeTime(const fs::path& path, fs::file_time_type* outLastChangeTime) {
    std::error_code exception;
    const fs::file_time_type lastWriteTimeSrc = std::filesystem::last_write_time(path, exception);
    if (exception.value() == 0) {
        *outLastChangeTime = lastWriteTimeSrc;
        return true;
    }
    std::cout << "Failed to to read file last change time\n";
    std::cout << exception.message() << std::endl;
    return false;
}

bool checkLastChangeTimeSlow(const fs::path& path, fs::file_time_type* outLastChangeTime, const int tries, const int tryWaitTimeMs) {
    std::error_code exception;
    //sometime quering last write time randomly fails, try a few times for more consistent success
    for (int i = 0; i < tries; i++) {
        const fs::file_time_type lastWriteTimeSrc = std::filesystem::last_write_time(path, exception);
        if (exception.value() == 0) {
            *outLastChangeTime = lastWriteTimeSrc;
            return true;
        }
        Sleep(tryWaitTimeMs);
    }
    std::cout << "Failed to to read file last change time\n";
    std::cout << exception.message() << std::endl;
    return false;
}

bool loadTextFile(const std::filesystem::path& absolutePath, std::vector<char>* outText) {

    std::ifstream file(absolutePath, std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "could not open file " << absolutePath << std::endl;
        return false;
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    outText->resize(fileSize);
    file.read(outText->data(), fileSize);

    file.close();

    return true;
}

bool loadBinaryFile(const std::filesystem::path& absolutePath, std::vector<uint32_t>* outData) {
    std::ifstream file(absolutePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Could not open file " << absolutePath << std::endl;
        return false;
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    outData->resize(fileSize / 4); //4 chars per size_t, TODO: think about systems with different data sizes
    file.read(reinterpret_cast<char*>(outData->data()), fileSize);
    file.close();

    return true;
}

void writeBinaryFile(const std::filesystem::path absolutePath, const std::vector<uint32_t>& data) {
    std::ofstream binaryFile;
    binaryFile.open(absolutePath, std::ios::out | std::ios::binary);
    if (!binaryFile.is_open()) {
        std::cout << "Failed to write binary file: " << absolutePath << "\n";
        return;
    }
    binaryFile.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint32_t));
    binaryFile.close();
}