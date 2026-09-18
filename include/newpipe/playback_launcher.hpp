#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/models.hpp"
#include "newpipe/playback_helper.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/youtube_catalog_service.hpp"

namespace newpipe {

struct PlaybackLaunch {
    std::optional<PlaybackRequest> request;
    StreamItem history_item;
};

// Pressing A used to resolve the stream inline on the borealis main loop, so
// the UI sat frozen for a whole player-API round trip before the player
// appeared - and on a slow connection that looked like a crash.
//
// The resolve runs on the worker now. `on_unresolved` is called back on the UI
// thread when nothing playable came out of it, so callers can fall through to
// the detail screen the way they always have.
inline void launch_playback_async(
    const LifetimeToken& alive,
    std::shared_ptr<YouTubeCatalogService> service,
    const StreamItem& item,
    std::string log_tag,
    std::function<void(const StreamItem&)> on_unresolved) {
    run_async<PlaybackLaunch>(
        alive,
        [service, item]() {
            PlaybackLaunch launch;
            const auto detail = service->get_stream_detail(item.url);
            launch.request = build_playback_request(item, detail);
            launch.history_item = detail.has_value() ? detail->item : item;
            return launch;
        },
        [item, log_tag, on_unresolved](const PlaybackLaunch& launch) {
            if (!launch.request.has_value()) {
                if (on_unresolved) {
                    on_unresolved(item);
                }
                return;
            }

            std::string ignored_error;
            LibraryStore::instance().add_history(launch.history_item, &ignored_error);
            logf("%s: queue playback url=%s", log_tag.c_str(), launch.request->url.c_str());
            queue_playback(*launch.request);
            brls::Application::quit();
        });
}

}  // namespace newpipe
