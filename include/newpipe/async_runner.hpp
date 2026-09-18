#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include <borealis.hpp>

namespace newpipe {

// Keeps the UI responsive while the catalog service is on the network.
//
// Every feed/search/detail call used to run inline on the borealis main loop,
// so the sidebar, the scrolling and the spinner all froze for as long as
// YouTube took to answer. AsyncRunner owns a single worker thread that all of
// that work is funnelled through; results come back to the main thread via
// brls::sync().
//
// One worker (not one thread per request) is deliberate: a tab's
// YouTubeCatalogService keeps mutable caches, so serialising every call to it
// on the same thread is what makes touching it from the UI unnecessary and
// race-free. Nothing outside the worker is allowed to call the service.
class AsyncRunner {
public:
    static AsyncRunner& instance();

    ~AsyncRunner();

    void start();
    void stop();

    // Runs `work` on the worker thread.
    void post(std::function<void()> work);

private:
    void worker();

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::queue<std::function<void()>> queue_;
};

// A token a view hands to the work it starts. The view owns it, so when the
// view dies the token dies with it and the queued result is dropped instead of
// writing into freed memory.
using LifetimeToken = std::shared_ptr<bool>;

inline LifetimeToken make_lifetime_token() {
    return std::make_shared<bool>(true);
}

// Runs `work` off the UI thread, then `on_done(result)` back on it - but only
// if `alive` is still held by its owner.
template <typename Result>
void run_async(
    const LifetimeToken& alive,
    std::function<Result()> work,
    std::function<void(Result)> on_done) {
    std::weak_ptr<bool> weak = alive;
    AsyncRunner::instance().post(
        [weak, work = std::move(work), on_done = std::move(on_done)]() mutable {
            if (weak.expired()) {
                return;
            }

            auto result = std::make_shared<Result>(work());
            brls::sync([weak, result, on_done]() {
                if (weak.expired()) {
                    return;
                }
                on_done(*result);
            });
        });
}

}  // namespace newpipe
