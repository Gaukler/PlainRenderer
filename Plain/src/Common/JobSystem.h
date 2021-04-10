#pragma once
#include "pch.h"

#include <thread>
#include <functional>
#include <mutex>

namespace JobSystem {

    //jobs increment counter when starting
    //threads can wait on a counter reaching zero
    //this allows to wait until all jobs associated with a counter being completed
    struct Counter {
        int counter = 0;    //could use atomic, but std::condition_variable requires mutex anyways
        std::mutex mutex;
        std::condition_variable zeroCondition;
    };

    void initJobSystem();
    //executes job
    //increments counter before starting and decrements counter after finishing
    void addJob(const std::function<void(int workerIndex)> job, Counter* counter);

    void waitOnCounter(Counter& counter);
    int getWorkerCount();
}