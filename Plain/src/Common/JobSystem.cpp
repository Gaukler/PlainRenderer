#include "pch.h"
#include "JobSystem.h"

#include <thread>
#include <functional>

#include "FunctionRingbufferThreadsafe.h"

namespace JobSystem {

    unsigned int g_threadCount;

    const size_t jobBufferSize = 64;
    FunctionRingbufferThreadsafe jobRingbuffer(jobBufferSize);

    //---- private function declarations ----

    //counter must not be nullptr
    void incrementCounter(Counter* counter);

    //counter must not be nullptr
    //notifies counters zeroCondition if counter reaches zero
    void decrementCounter(Counter* counter);


    //---- function implementations ----

    void initJobSystem() {

        g_threadCount = std::thread::hardware_concurrency();
        std::cout << "JobSystem thread count: " << g_threadCount << "\n\n";

        for (unsigned int workerIndex = 0; workerIndex < g_threadCount; workerIndex++) {
            std::thread worker([workerIndex]() {
                //function executed by worker
                while (true) {
                    std::function<void(int)> job = jobRingbuffer.popFront();
                    job(workerIndex);
                }
            });
            //let thread continue after worker goes out of scope
            worker.detach();
        }
    }

    void addJob(const std::function<void(int workerIndex)> job, Counter* counter) {
        if (counter != nullptr) {
            incrementCounter(counter);

            //add lambda that first executes job, then decrements counter
            jobRingbuffer.add([job, counter](int workerIndex) {
                job(workerIndex);
                decrementCounter(counter);
            });
        }
        else {
            jobRingbuffer.add(job);
        }
    }

    void waitOnCounter(Counter& counter) {
        std::unique_lock uniqueLock(counter.mutex);
        if (counter.counter == 0) {
            //counter already zero, do nothing
        }
        else {
            //counter not zero, wait
            counter.zeroCondition.wait(uniqueLock);
        }
    }

    int getWorkerCount() {
        return g_threadCount;
    }

    void incrementCounter(Counter* counter) {
        assert(counter != nullptr);
        std::unique_lock uniqueLock(counter->mutex);
        counter->counter++;
    }

    void decrementCounter(Counter* counter) {
        assert(counter != nullptr);
        std::unique_lock uniqueLock(counter->mutex);
        counter->counter--;
        if (counter->counter == 0) {
            counter->zeroCondition.notify_all();
        }
    }
}