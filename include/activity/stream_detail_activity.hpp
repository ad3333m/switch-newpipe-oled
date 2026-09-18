#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"

class StreamDetailActivity : public brls::Activity {
public:
    explicit StreamDetailActivity(newpipe::StreamItem item);

    CONTENT_FROM_XML_RES("activity/stream_detail.xml");

    void onContentAvailable() override;

private:
    struct FeedResult {
        std::optional<newpipe::HomeFeed> feed;
        std::string error;
    };

    void loadDetail();
    void applyDetail(const std::optional<newpipe::StreamDetail>& detail);
    void updateFavoriteAction();
    void setBusy(bool busy);
    void playStream();
    // Every "open something else" action is a network round trip, so they all
    // go through the worker and push their activity from the callback.
    void openFeed(
        std::function<FeedResult()> fetch,
        std::string failure_key,
        std::function<std::string(const newpipe::HomeFeed&)> title_for);
    void openChannelFeed();
    void openRelatedFeed();
    void openExtrasMenu();
    void openPlaylistFeed();
    void openComments();
    void toggleFavorite();

    newpipe::StreamItem item_;
    // Shared, not owned outright: borealis deletes activities and tabs from
    // inside Application::exit(), and work already handed to the worker may
    // still be using the service at that point. The shared_ptr the queued work
    // holds keeps it alive until that request finishes.
    std::shared_ptr<newpipe::YouTubeCatalogService> service_ =
        std::make_shared<newpipe::YouTubeCatalogService>();
    bool busy_ = false;
    newpipe::LifetimeToken alive_ = newpipe::make_lifetime_token();

    BRLS_BIND(brls::Label, titleLabel, "detail/title");
    BRLS_BIND(brls::Label, metaLabel, "detail/meta");
    BRLS_BIND(brls::Label, statusLabel, "detail/status");
    BRLS_BIND(brls::Label, bodyLabel, "detail/body");
    BRLS_BIND(brls::ProgressSpinner, spinner, "detail/spinner");
};
