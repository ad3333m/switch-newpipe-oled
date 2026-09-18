#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/stream_grid.hpp"

class StreamFeedActivity : public brls::Activity {
public:
    StreamFeedActivity(std::string title, std::vector<newpipe::StreamItem> items);

    CONTENT_FROM_XML_RES("activity/stream_feed.xml");

    void onContentAvailable() override;

private:
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    std::string title_;
    std::vector<newpipe::StreamItem> items_;
    // Shared, not owned outright: borealis deletes activities and tabs from
    // inside Application::exit(), and work already handed to the worker may
    // still be using the service at that point. The shared_ptr the queued work
    // holds keeps it alive until that request finishes.
    std::shared_ptr<newpipe::YouTubeCatalogService> service_ =
        std::make_shared<newpipe::YouTubeCatalogService>();
    std::atomic<bool> interactionReady_{false};
    newpipe::LifetimeToken alive_ = newpipe::make_lifetime_token();

    BRLS_BIND(brls::Label, statusLabel, "feed/status");
    BRLS_BIND(brls::Label, subtitleLabel, "feed/subtitle");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "feed/scroll");
    BRLS_BIND(StreamGrid, grid, "feed/grid");
};
