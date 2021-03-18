#pragma once
#include "pch.h"
#include <functional>
#include <mutex>

class FunctionRingbufferThreadsafe {
public:
	//bufferSize is the maximum number of elements the ringbuffer can hold at any time
	FunctionRingbufferThreadsafe(const size_t bufferSize);

	//appends function to end of list, waits if buffer is full
	void add(const std::function<void()> function);

	//removes and returns first element, waits if buffer is empty
	std::function<void()> popFront();

private:
	std::vector<std::function<void()>> m_buffer;
	int m_frontIndex;	//index of first element in m_buffer
	int m_elementCount;	//number of elements currently in m_buffer

	std::mutex m_mutex;
	std::condition_variable m_emptyCondition;
	std::condition_variable m_fullCondition;
};