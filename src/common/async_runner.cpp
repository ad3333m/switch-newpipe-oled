#include "newpipe/async_runner.hpp"

#include <chrono>

#include "newpipe/log.hpp"

namespace newpipe {

AsyncRunner& AsyncRunner::instance() {
    static AsyncRunner runner;
    return runner;
}

AsyncRunner::~AsyncRunner() {
    stop();
}

void AsyncRunner::start() {
    if (running_) {
        return;
    }

    running_ = true;
    log_line("async: start worker");
    thread_ = std::thread([this]() { worker(); });
}

void AsyncRunner::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<std::function<void()>> drained;
    queue_.swap(drained);
    log_line("async: stop worker");
}

void AsyncRunner::post(std::function<void()> work) {
    if (!work) {
        return;
    }

    // Without a worker the caller would silently never hear back, which is far
    // worse than a brief stall, so run it here instead.
    if (!running_) {
        log_line("async: worker not running, executing inline");
        work();
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(work));
}

void AsyncRunner::worker() {
    log_line("async: worker entered");

    while (running_) {
        std::function<void()> work;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!queue_.empty()) {
                work = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (!work) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        try {
            work();
        } catch (const std::exception& ex) {
            logf("async: task threw: %s", ex.what());
        } catch (...) {
            log_line("async: task threw unknown exception");
        }
    }

    log_line("async: worker exit");
}

}  // namespace newpipe
