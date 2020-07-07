#pragma once
#include "pch.h"

namespace fs = std::filesystem;

class DirectoryUtils {
public:
	static DirectoryUtils& getReference();
	fs::path getWorkingDirectory() const;
	fs::path getResourceDirectory() const;

private:
	DirectoryUtils();

	//loops trough up from working directory and checks for "resource" folder
	//if none is found returns root of the path (C: or similar)
	fs::path searchResourceDirectory(const fs::path& workingDirectory);

	static fs::path m_workingDirectory;
	static fs::path m_resourceDirectory;
};