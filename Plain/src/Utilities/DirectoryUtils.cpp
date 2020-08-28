#include "pch.h"
#include "DirectoryUtils.h"

namespace fs = std::filesystem;

fs::path DirectoryUtils::m_workingDirectory;
fs::path DirectoryUtils::m_resourceDirectory;

void DirectoryUtils::init() {
    m_workingDirectory = fs::current_path();
    m_resourceDirectory = searchResourceDirectory(m_workingDirectory);
}

fs::path DirectoryUtils::getWorkingDirectory(){
	return m_workingDirectory;
}

fs::path DirectoryUtils::getResourceDirectory(){
	return m_resourceDirectory;
}

fs::path DirectoryUtils::searchResourceDirectory(const fs::path& workingDirectory) {

	fs::path current = workingDirectory;

	//parent_path of C: will return C: -> loop terminates
	while (current != current.parent_path()) {
		fs::directory_iterator it(current);

		for (auto& entry : it) {
			if (!entry.is_directory()) continue;
			//end of path is always "", second to last is folder
			if (*(--entry.path().end()) == "resources") {
				return entry;
			}
		}
		current = (current.parent_path());
	}

	throw std::runtime_error("couldn't find resource directory");
}