#pragma once
#include "pch.h"

namespace fs = std::filesystem;

class DirectoryUtils {
public:
    static void init();

    static fs::path getWorkingDirectory();
    static fs::path getResourceDirectory();

private:
    //loops trough up from working directory and checks for "resource" folder
	//if none is found returns root of the path (C: or similar)
	static fs::path searchResourceDirectory(const fs::path& workingDirectory);

	static fs::path m_workingDirectory;
	static fs::path m_resourceDirectory;
};