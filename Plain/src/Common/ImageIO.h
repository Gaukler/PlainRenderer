#pragma once
#include "pch.h"
#include "ImageDescription.h"

//if isFullPath is false, the resource directory is prepended
bool loadImage(const std::filesystem::path& path, const bool isFullPath,
	ImageDescription* outDescription, std::vector<uint8_t>* outData);

//only a limited number of formats and configurations is supported
//currently supporting BC1, BC3, BC5 and if using DX10 header R16_float 
bool loadDDSFile(const std::filesystem::path& filename, ImageDescription* outDescription, std::vector<uint8_t>* outData);

//not robust and tested enough to use as a general purpose DDS exporter, use only for project
//always uses DX10 header for format encoding, which does not seem to be supported widely
void writeDDSFile(const std::filesystem::path& pathAbsolute, const ImageDescription& imageDescription, 
	const std::vector<uint8_t>& data);