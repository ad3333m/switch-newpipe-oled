#include "tab/search_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_launcher.hpp"
#include "view/fade.hpp"
#include "view/tab_focus.hpp"

SearchTab::SearchTab() {
    this->inflateFromXMLRes("xml/tabs/search.xml");
    newpipe::log_line("search: construct");

    if (grid) {
        grid->setOnPlay([this](const newpipe::StreamItem& item) { this->playStream(item); });
        grid->setOnInfo([this](const newpipe::StreamItem& item) { this->openStream(item); });
    }
    if (scrollFrame) {
        scrollFrame->setOnReachEnd([this]() { return this->requestMorePages(); });
    }

    if (statusLabel) {
        statusLabel->setText(newpipe::tr("search/prompt"));
    }
    brls::delay(700, [this]() {
        interactionReady_.store(true);
        newpipe::log_line("search: interaction ready");
    });
}

void SearchTab::onCreate() {
    // Registered on the sidebar item as well: until a query returns results this
    // tab has no focusable child, so focus can never leave the sidebar and a
    // content-only action would be unreachable.
    this->registerTabAction(newpipe::tr("search/action"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        brls::Application::getImeManager()->openForText(
            [this](const std::string& text) { doSearch(text); },
            newpipe::tr("search/ime_title"),
            newpipe::tr("search/ime_subtitle"),
            80,
            lastQuery_);
        return true;
    });
}

void SearchTab::setBusy(bool busy) {
    loading_ = busy;
    if (spinner) {
        if (busy) {
            newpipe::fade_in(spinner);
        } else {
            newpipe::fade_out_gone(spinner);
        }
    }
}

void SearchTab::clearResults() {
    continuation_ = {};
    if (grid) {
        newpipe::release_grid_focus(this, grid);
        grid->reset();
    }
    if (scrollFrame) {
        scrollFrame->setContentOffsetY(0, false);
        scrollFrame->resetPagingTrigger();
    }
    if (moreLabel) {
        newpipe::fade_out_gone(moreLabel);
    }
}

void SearchTab::doSearch(const std::string& query) {
    lastQuery_ = query;
    newpipe::logf("search: doSearch query=%s", query.c_str());

    if (!service_->is_loaded()) {
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("common/service_init_failed", service_->error_message()));
        }
        return;
    }

    clearResults();

    if (query.empty()) {
        if (statusLabel) {
            statusLabel->setText(newpipe::tr("search/prompt"));
        }
        return;
    }

    setBusy(true);
    if (statusLabel) {
        statusLabel->setText(newpipe::tr("search/searching", query));
    }

    const auto service = service_;
    newpipe::run_async<SearchResult>(
        alive_,
        [service, query]() {
            SearchResult result;
            result.results = service->search(query);
            result.error = service->error_message();
            return result;
        },
        [this](const SearchResult& result) { this->applyResults(result); });
}

void SearchTab::applyResults(const SearchResult& result) {
    setBusy(false);
    continuation_ = result.results.continuation;

    if (grid) {
        grid->appendItems(result.results.items);
    }
    if (scrollFrame) {
        scrollFrame->resetPagingTrigger();
    }
    newpipe::logf("search: results=%zu", result.results.items.size());

    if (!statusLabel) {
        return;
    }
    if (result.results.items.empty()) {
        statusLabel->setText(
            result.error.empty() ? newpipe::tr("search/no_results", result.results.query)
                                 : result.error);
        return;
    }
    statusLabel->setText(
        newpipe::tr("search/results_count", result.results.query, grid ? grid->itemCount() : 0));
}

bool SearchTab::requestMorePages() {
    if (loading_ || loadingMore_ || !continuation_.valid()) {
        return false;
    }

    loadingMore_ = true;
    if (moreLabel) {
        moreLabel->setText(newpipe::tr("common/loading_more"));
        newpipe::fade_in(moreLabel);
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

void SearchTab::applyPage(const PageResult& result) {
    loadingMore_ = false;

    if (!result.page.has_value() || result.page->items.empty()) {
        // Nothing more to show: stop asking rather than retrying forever.
        newpipe::logf("search: no further pages error=%s", result.error.c_str());
        continuation_ = {};
        if (moreLabel) {
            newpipe::fade_out_gone(moreLabel);
        }
        return;
    }

    continuation_ = result.page->continuation;
    if (grid) {
        grid->appendItems(result.page->items);
    }
    if (moreLabel) {
        newpipe::fade_out_gone(moreLabel);
    }
    if (scrollFrame) {
        scrollFrame->resetPagingTrigger();
    }
    if (statusLabel) {
        statusLabel->setText(
            newpipe::tr("search/results_count", lastQuery_, grid ? grid->itemCount() : 0));
    }
}

bool SearchTab::allowInitialInput() const {
    if (interactionReady_.load()) {
        return true;
    }

    newpipe::log_line("search: ignored startup input");
    return false;
}

void SearchTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("search: playStream url=%s", item.url.c_str());
    setBusy(true);
    newpipe::launch_playback_async(
        alive_,
        service_,
        item,
        "search",
        [this](const newpipe::StreamItem& unresolved) {
            setBusy(false);
            openStream(unresolved);
        });
}

void SearchTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("search: openStream url=%s", item.url.c_str());
    brls::Application::pushActivity(new StreamDetailActivity(item));
}
