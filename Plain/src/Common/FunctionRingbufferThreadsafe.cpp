#include "pch.h"
#include "FunctionRingbufferThreadsafe.h"

FunctionRingbufferThreadsafe::FunctionRingbufferThreadsafe(const size_t bufferSize) {
	m_buffer.resize(bufferSize);
	m_frontIndex = 0;
	m_elementCount = 0;
}

void FunctionRingbufferThreadsafe::add(const std::function<void(int)> function) {
	std::unique_lock uniqueLock(m_mutex);

	if (m_elementCount == m_buffer.size()) {
		//buffer already full, need to wait
		m_fullCondition.wait(uniqueLock);
	}

	assert(m_elementCount != m_buffer.size());

	//insert element and update info
	const size_t insertionIndex = (m_frontIndex + m_elementCount) % m_buffer.size();
	m_buffer[insertionIndex] = function;
	m_elementCount++;

	//notify one thread waiting on an empty buffer
	m_emptyCondition.notify_one();
}

std::function<void(int)> FunctionRingbufferThreadsafe::popFront() {
	std::unique_lock uniqueLock(m_mutex);

	if (m_elementCount == 0) {
		//buffer is empty, need to wait
		m_emptyCondition.wait(uniqueLock);
	}

	assert(m_elementCount != 0);
	//pop element and update info
	std::function<void(int)> function = m_buffer[m_frontIndex];
	m_elementCount--;
	m_frontIndex = (m_frontIndex + 1) % m_buffer.size();

	//notify one thread waiting on a full buffer
	m_fullCondition.notify_one();

	return function;
}