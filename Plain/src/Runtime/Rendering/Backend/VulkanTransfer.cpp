#include "pch.h"
#include "VulkanTransfer.h"

Data::Data(const void* ptr, const size_t size) : ptr(ptr), size(size) {};
Data::Data() : ptr(nullptr), size(0) {};