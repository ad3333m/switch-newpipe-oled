#include "tab/library_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_launcher.hpp"
#include "view/fade.hpp"
#include "view/tab_focus.hpp"

LibraryTab::LibraryTab() {
    this->inflateFromXMLRes("xml/tabs/library.xml");
    brls::delay(700, [this]() { interactionReady_.store(true); });

    if (grid) {
        grid->setOnPlay([this](const newpipe::StreamItem& item) { this->playStream(item); });
        grid->setOnInfo([this](const newpipe::StreamItem& item) { this->openStream(item); });
    }

    // Kept on the content only: clearing is destructive and needs no sidebar
    // shortcut, an empty section has nothing to clear anyway.
    this->registerAction(newpipe::tr("library/clear_action"), brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
        this->clearCurrentSection();
        return true;
    });

    this->refresh();
}

void LibraryTab::onCreate() {
    // Mirrored on the sidebar item: an empty history/favorites list has no
    // focusable child, so a content-only action could never be triggered.
    this->registerTabAction(newpipe::tr("common/refresh"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->refresh();
        return true;
    });
    this->registerTabAction(newpipe::tr("library/section_action"), brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
        this->toggleSection();
        return true;
    });
}

bool LibraryTab::allowInitialInput() const {
    return interactionReady_.load();
}

void LibraryTab::refresh() {
    std::string error;
    newpipe::LibraryStore::instance().load(&error);
    const auto items = this->showingFavorites_
        ? newpipe::LibraryStore::instance().favorite_items()
        : newpipe::LibraryStore::instance().history_items();

    if (grid) {
        newpipe::release_grid_focus(this, grid);
        grid->reset();
        grid->appendItems(items);
    }
    if (scrollFrame) {
        scrollFrame->setContentOffsetY(0, false);
    }

    if (statusLabel) {
        statusLabel->setText(
            newpipe::tr(
                "common/count_with_title",
                this->showingFavorites_ ? newpipe::tr("library/favorites")
                                        : newpipe::tr("library/history"),
                items.size()));
    }
    if (bodyLabel) {
        if (items.empty()) {
            bodyLabel->setText(
                this->showingFavorites_
                    ? newpipe::tr("library/favorites_empty")
                    : newpipe::tr("library/history_empty"));
        } else {
            bodyLabel->setText(newpipe::tr("library/controls"));
        }
    }
    if (spinner) {
        newpipe::fade_out_gone(spinner);
    }
}

void LibraryTab::toggleSection() {
    this->showingFavorites_ = !this->showingFavorites_;
    this->refresh();
}

void LibraryTab::clearCurrentSection() {
    std::string error;
    const bool ok = this->showingFavorites_ ? newpipe::LibraryStore::instance().clear_favorites(&error)
                                            : newpipe::LibraryStore::instance().clear_history(&error);
    if (!ok) {
        brls::Application::notify(error.empty() ? newpipe::tr("library/clear_failed") : error);
        return;
    }

    brls::Application::notify(this->showingFavorites_ ? newpipe::tr("library/favorites_cleared")
                                                      : newpipe::tr("library/history_cleared"));
    this->refresh();
}

void LibraryTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    if (spinner) {
        newpipe::fade_in(spinner);
    }
    newpipe::launch_playback_async(
        alive_,
        service_,
        item,
        "library",
        [this](const newpipe::StreamItem& unresolved) {
            if (spinner) {
                newpipe::fade_out_gone(spinner);
            }
            openStream(unresolved);
        });
}

void LibraryTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    brls::Application::pushActivity(new StreamDetailActivity(item));
}
