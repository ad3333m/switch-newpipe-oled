#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <borealis.hpp>

#include "newpipe/async_runner.hpp"
#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/paging_scrolling_frame.hpp"
#include "view/stream_grid.hpp"

class LibraryTab : public AttachedView {
public:
    LibraryTab();

    void onCreate() override;

    static brls::View* create() { return new LibraryTab(); }

private:
    // Favourites and history are read straight off the SD card, so this tab has
    // nothing to page through and nothing to wait on the network for.
    void refresh();
    void toggleSection();
    void clearCurrentSection();
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    BRLS_BIND(brls::Label, statusLabel, "library/status");
    BRLS_BIND(brls::Label, bodyLabel, "library/body");
    BRLS_BIND(brls::ProgressSpinner, spinner, "library/spinner");
    BRLS_BIND(PagingScrollingFrame, scrollFrame, "library/scroll");
    BRLS_BIND(StreamGrid, grid, "library/grid");

    // Shared, not owned outright: borealis deletes activities and tabs from
    // inside Application::exit(), and work already handed to the worker may
    // still be using the service at that point. The shared_ptr the queued work
    // holds keeps it alive until that request finishes.
    std::shared_ptr<newpipe::YouTubeCatalogService> service_ =
        std::make_shared<newpipe::YouTubeCatalogService>();
    bool showingFavorites_ = false;
    std::atomic<bool> interactionReady_{false};
    newpipe::LifetimeToken alive_ = newpipe::make_lifetime_token();
};
