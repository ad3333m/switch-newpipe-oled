#include "tab/home_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_launcher.hpp"
#include "newpipe/settings_store.hpp"
#include "view/tab_focus.hpp"

HomeTab::HomeTab() {
    this->inflateFromXMLRes("xml/tabs/home.xml");
    newpipe::log_line("home: construct");

    kiosks_ = service_->list_kiosks();
    newpipe::logf("home: kiosks=%zu", kiosks_.size());
    const newpipe::AppSettings settings = newpipe::SettingsStore::instance().settings();
    for (size_t i = 0; i < kiosks_.size(); i++) {
        if (kiosks_[i].id == settings.home_kiosk) {
            kioskIndex_ = i;
            break;
        }
    }

    if (grid) {
        grid->setOnPlay([this](const newpipe::StreamItem& item) { this->playStream(item); });
        grid->setOnInfo([this](const newpipe::StreamItem& item) { this->openStream(item); });
    }
    if (scrollFrame) {
        scrollFrame->setOnReachEnd([this]() { return this->requestMorePages(); });
    }

    if (statusLabel) {
        statusLabel->setText(newpipe::tr("home/preparing"));
    }
    brls::delay(700, [this]() {
        interactionReady_.store(true);
        newpipe::log_line("home: interaction ready");
    });
    scheduleLoadHome(250);
}

void HomeTab::onCreate() {
    // Tab actions are mirrored on the sidebar item so they stay usable while the
    // sidebar holds focus, and when the feed is empty and nothing here is focusable.
    this->registerTabAction(newpipe::tr("common/refresh"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        loadHome();
        return true;
    });
    this->registerTabAction(newpipe::tr("home/category_action"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        cycleKiosk();
        return true;
    });
}

void HomeTab::setBusy(bool busy) {
    loading_ = busy;
    if (spinner) {
        spinner->setVisibility(busy ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

void HomeTab::loadHome() {
    newpipe::logf("home: loadHome index=%zu", kioskIndex_);
    if (loading_) {
        return;
    }
    if (!initialLoadCompleted_) {
        initialLoadAttempts_++;
    }

    if (!service_->is_loaded()) {
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("common/service_init_failed", service_->error_message()));
        }
        return;
    }

    if (kiosks_.empty()) {
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("home/no_kiosk"));
        }
        return;
    }

    kioskIndex_ %= kiosks_.size();
    const std::string kiosk_id = kiosks_[kioskIndex_].id;
    setBusy(true);

    // Off the UI thread: this is a full YouTube round trip, and it used to hold
    // the main loop (and every animation on screen) for the whole of it.
    const auto service = service_;
    newpipe::run_async<FeedResult>(
        alive_,
        [service, kiosk_id]() {
            FeedResult result;
            result.feed = service->get_home_feed(kiosk_id);
            result.error = service->error_message();
            return result;
        },
        [this](const FeedResult& result) { this->applyFeed(result); });
}

void HomeTab::applyFeed(const FeedResult& result) {
    setBusy(false);

    if (!result.feed.has_value()) {
        if (statusLabel) {
            statusLabel->setText(
                result.error.empty() ? newpipe::tr("home/load_failed") : result.error);
        }
        if (!initialLoadCompleted_ && (!grid || grid->empty()) && initialLoadAttempts_ < 4) {
            if (statusLabel) {
                statusLabel->setText(newpipe::tr("home/preparing"));
            }
            newpipe::logf("home: auto retry attempt=%d", initialLoadAttempts_);
            scheduleLoadHome(350);
        }
        return;
    }

    initialLoadCompleted_ = true;
    continuation_ = result.feed->continuation;

    feedTitle_ = result.feed->kiosk.title;
    const std::string option_key = "settings/home_kiosk/options/" + result.feed->kiosk.id;
    const std::string translated = newpipe::tr(option_key);
    if (!translated.empty() && translated != option_key) {
        feedTitle_ = translated;
    }

    if (grid) {
        newpipe::release_grid_focus(this, grid);
        grid->reset();
        grid->appendItems(result.feed->items);
    }
    if (scrollFrame) {
        scrollFrame->setContentOffsetY(0, false);
        scrollFrame->resetPagingTrigger();
    }
    if (moreLabel) {
        moreLabel->setVisibility(brls::Visibility::GONE);
    }

    newpipe::logf(
        "home: feed=%s items=%zu",
        result.feed->kiosk.id.c_str(),
        result.feed->items.size());

    if (statusLabel) {
        statusLabel->setText(
            newpipe::tr("common/count_with_title", feedTitle_, grid ? grid->itemCount() : 0));
    }
}

bool HomeTab::requestMorePages() {
    if (loading_ || loadingMore_ || !continuation_.valid()) {
        return false;
    }

    loadingMore_ = true;
    if (moreLabel) {
        moreLabel->setText(newpipe::tr("common/loading_more"));
        moreLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    const newpipe::Continuation continuation = continuation_;
    const auto service = service_;
    newpipe::run_async<PageResult>(
        alive_,
        [service, continuation]() {
            PageResult result;
            result.page = service->get_more_items(continuation);
            result.error = service->error_message();
            return result;
        },
        [this](const PageResult& result) { this->applyPage(result); });
    return true;
}

void HomeTab::applyPage(const PageResult& result) {
    loadingMore_ = false;

    if (!result.page.has_value() || result.page->items.empty()) {
        // Nothing more to show: stop asking rather than retrying forever.
        newpipe::logf("home: no further pages error=%s", result.error.c_str());
        continuation_ = {};
        if (moreLabel) {
            moreLabel->setVisibility(brls::Visibility::GONE);
        }
        return;
    }

    continuation_ = result.page->continuation;
    if (grid) {
        grid->appendItems(result.page->items);
    }
    if (moreLabel) {
        moreLabel->setVisibility(brls::Visibility::GONE);
    }
    if (scrollFrame) {
        scrollFrame->resetPagingTrigger();
    }
    if (statusLabel) {
        statusLabel->setText(
            newpipe::tr("common/count_with_title", feedTitle_, grid ? grid->itemCount() : 0));
    }
}

void HomeTab::scheduleLoadHome(long delay_ms) {
    brls::delay(delay_ms, [this]() { loadHome(); });
}

void HomeTab::cycleKiosk() {
    if (!allowInitialInput()) {
        return;
    }
    if (kiosks_.empty()) {
        return;
    }

    kioskIndex_ = (kioskIndex_ + 1) % kiosks_.size();
    newpipe::logf("home: cycleKiosk newIndex=%zu id=%s", kioskIndex_, kiosks_[kioskIndex_].id.c_str());
    continuation_ = {};
    loadHome();
}

bool HomeTab::allowInitialInput() const {
    if (interactionReady_.load()) {
        return true;
    }

    newpipe::log_line("home: ignored startup input");
    return false;
}

void HomeTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("home: playStream url=%s", item.url.c_str());
    setBusy(true);
    newpipe::launch_playback_async(
        alive_,
        service_,
        item,
        "home",
        [this](const newpipe::StreamItem& unresolved) {
            setBusy(false);
            openStream(unresolved);
        });
}

void HomeTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("home: openStream url=%s", item.url.c_str());
    brls::Application::pushActivity(new StreamDetailActivity(item));
}
