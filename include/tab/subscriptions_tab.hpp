#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/auth_store.hpp"
#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/paging_scrolling_frame.hpp"
#include "view/stream_grid.hpp"

class SubscriptionsTab : public AttachedView {
public:
    SubscriptionsTab();

    void onCreate() override;

    static brls::View* create() { return new SubscriptionsTab(); }

private:
    struct FeedResult {
        bool auth_loaded = false;
        bool has_session = false;
        std::optional<newpipe::HomeFeed> feed;
        newpipe::AuthSession session;
        std::string auth_error;
        std::string error;
    };

    struct PageResult {
        std::optional<newpipe::FeedPage> page;
        std::string error;
    };

    struct SessionOpResult {
        bool ok = false;
        std::string error;
    };

    void refresh();
    void applyFeed(const FeedResult& result);
    bool requestMorePages();
    void applyPage(const PageResult& result);
    void clearGrid();
    void setBusy(bool busy);
    void showSignedOutState();
    void openSessionDialog();
    void handleManualCookieInput(const std::string& text);
    // Session files are touched by the same worker the feeds run on, so an
    // import never lands halfway through a fetch that is reading it.
    void runSessionOp(
        std::function<SessionOpResult()> op,
        std::string success_key,
        std::string failure_key);
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    BRLS_BIND(brls::Label, statusLabel, "subscriptions/status");
    BRLS_BIND(brls::Label, bodyLabel, "subscriptions/body");
    BRLS_BIND(brls::Label, moreLabel, "subscriptions/more");
    BRLS_BIND(brls::ProgressSpinner, spinner, "subscriptions/spinner");
    BRLS_BIND(PagingScrollingFrame, scrollFrame, "subscriptions/scroll");
    BRLS_BIND(StreamGrid, grid, "subscriptions/grid");

    // Shared, not owned outright: borealis deletes activities and tabs from
    // inside Application::exit(), and work already handed to the worker may
    // still be using the service at that point. The shared_ptr the queued work
    // holds keeps it alive until that request finishes.
    std::shared_ptr<newpipe::YouTubeCatalogService> service_ =
        std::make_shared<newpipe::YouTubeCatalogService>();
    newpipe::Continuation continuation_;
    newpipe::AuthSession session_;
    std::string feedTitle_;
    bool hasSession_ = false;
    bool loading_ = false;
    bool loadingMore_ = false;
    std::atomic<bool> interactionReady_{false};
    newpipe::LifetimeToken alive_ = newpipe::make_lifetime_token();
};
