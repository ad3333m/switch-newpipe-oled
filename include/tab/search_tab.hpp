#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/paging_scrolling_frame.hpp"
#include "view/stream_grid.hpp"

class SearchTab : public AttachedView {
public:
    SearchTab();

    void onCreate() override;

    static brls::View* create() { return new SearchTab(); }

private:
    struct SearchResult {
        newpipe::SearchResults results;
        std::string error;
    };

    struct PageResult {
        std::optional<newpipe::FeedPage> page;
        std::string error;
    };

    void doSearch(const std::string& query);
    void applyResults(const SearchResult& result);
    bool requestMorePages();
    void applyPage(const PageResult& result);
    void clearResults();
    void setBusy(bool busy);
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    BRLS_BIND(brls::Label, statusLabel, "search/status");
    BRLS_BIND(brls::Label, moreLabel, "search/more");
    BRLS_BIND(brls::ProgressSpinner, spinner, "search/spinner");
    BRLS_BIND(PagingScrollingFrame, scrollFrame, "search/scroll");
    BRLS_BIND(StreamGrid, grid, "search/grid");

    // Shared, not owned outright: borealis deletes activities and tabs from
    // inside Application::exit(), and work already handed to the worker may
    // still be using the service at that point. The shared_ptr the queued work
    // holds keeps it alive until that request finishes.
    std::shared_ptr<newpipe::YouTubeCatalogService> service_ =
        std::make_shared<newpipe::YouTubeCatalogService>();
    newpipe::Continuation continuation_;
    std::string lastQuery_;
    bool loading_ = false;
    bool loadingMore_ = false;
    std::atomic<bool> interactionReady_{false};
    newpipe::LifetimeToken alive_ = newpipe::make_lifetime_token();
};
