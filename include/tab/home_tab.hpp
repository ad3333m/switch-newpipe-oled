#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/models.hpp"
#include "newpipe/settings_store.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/paging_scrolling_frame.hpp"
#include "view/stream_grid.hpp"

class HomeTab : public AttachedView {
public:
    HomeTab();

    void onCreate() override;

    static brls::View* create() { return new HomeTab(); }

private:
    struct FeedResult {
        std::optional<newpipe::HomeFeed> feed;
        std::string error;
    };

    struct PageResult {
        std::optional<newpipe::FeedPage> page;
        std::string error;
    };

    void loadHome();
    void applyFeed(const FeedResult& result);
    void scheduleLoadHome(long delay_ms);
    bool requestMorePages();
    void applyPage(const PageResult& result);
    void setBusy(bool busy);
    void cycleKiosk();
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    BRLS_BIND(brls::Label, statusLabel, "home/status");
    BRLS_BIND(brls::Label, moreLabel, "home/more");
    BRLS_BIND(brls::ProgressSpinner, spinner, "home/spinner");
    BRLS_BIND(PagingScrollingFrame, scrollFrame, "home/scroll");
    BRLS_BIND(StreamGrid, grid, "home/grid");

    // Shared, not owned outright: borealis deletes activities and tabs from
    // inside Application::exit(), and work already handed to the worker may
    // still be using the service at that point. The shared_ptr the queued work
    // holds keeps it alive until that request finishes.
    std::shared_ptr<newpipe::YouTubeCatalogService> service_ =
        std::make_shared<newpipe::YouTubeCatalogService>();
    std::vector<newpipe::Kiosk> kiosks_;
    newpipe::Continuation continuation_;
    std::string feedTitle_;
    size_t kioskIndex_ = 0;
    bool initialLoadCompleted_ = false;
    int initialLoadAttempts_ = 0;
    bool loading_ = false;
    bool loadingMore_ = false;
    std::atomic<bool> interactionReady_{false};
    newpipe::LifetimeToken alive_ = newpipe::make_lifetime_token();
};
