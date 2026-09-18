#include "tab/subscriptions_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/auth_store.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_launcher.hpp"
#include "view/tab_focus.hpp"

SubscriptionsTab::SubscriptionsTab() {
    this->inflateFromXMLRes("xml/tabs/subscriptions.xml");
    newpipe::log_line("subscriptions: construct");

    if (grid) {
        grid->setOnPlay([this](const newpipe::StreamItem& item) { this->playStream(item); });
        grid->setOnInfo([this](const newpipe::StreamItem& item) { this->openStream(item); });
    }
    if (scrollFrame) {
        scrollFrame->setOnReachEnd([this]() { return this->requestMorePages(); });
    }

    brls::delay(700, [this]() { interactionReady_.store(true); });

    this->refresh();
}

void SubscriptionsTab::onCreate() {
    // Mirrored on the sidebar item: while signed out this tab has no focusable
    // child, so a content-only action could never be triggered.
    this->registerTabAction(newpipe::tr("common/refresh"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->refresh();
        return true;
    });
    this->registerTabAction(newpipe::tr("subscriptions/session_action"), brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
        this->openSessionDialog();
        return true;
    });
}

bool SubscriptionsTab::allowInitialInput() const {
    return interactionReady_.load();
}

void SubscriptionsTab::setBusy(bool busy) {
    loading_ = busy;
    if (spinner) {
        spinner->setVisibility(busy ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
    }
}

void SubscriptionsTab::clearGrid() {
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
        moreLabel->setVisibility(brls::Visibility::GONE);
    }
}

void SubscriptionsTab::refresh() {
    newpipe::log_line("subscriptions: refresh");
    if (loading_) {
        return;
    }

    setBusy(true);

    // Session load, feed fetch and session read all happen on the worker, so
    // the tab only ever renders a snapshot that is internally consistent.
    const auto service = service_;
    newpipe::run_async<FeedResult>(
        alive_,
        [service]() {
            FeedResult result;
            result.auth_loaded = service->load_auth_session(&result.auth_error);
            if (!result.auth_loaded) {
                return result;
            }

            result.has_session = service->has_auth_session();
            if (!result.has_session) {
                return result;
            }

            result.feed = service->get_subscriptions_feed();
            result.error = service->error_message();
            result.session = service->auth_session();
            return result;
        },
        [this](const FeedResult& result) { this->applyFeed(result); });
}

void SubscriptionsTab::applyFeed(const FeedResult& result) {
    setBusy(false);
    session_ = result.session;
    hasSession_ = result.has_session;

    if (!result.auth_loaded) {
        if (statusLabel) {
            statusLabel->setText(
                result.auth_error.empty() ? newpipe::tr("subscriptions/session_load_failed")
                                          : result.auth_error);
        }
        if (bodyLabel) {
            bodyLabel->setText(newpipe::tr("subscriptions/session_load_failed_body"));
        }
        clearGrid();
        return;
    }

    if (!result.has_session) {
        showSignedOutState();
        return;
    }

    if (!result.feed.has_value()) {
        if (statusLabel) {
            statusLabel->setText(
                result.error.empty() ? newpipe::tr("subscriptions/feed_load_failed") : result.error);
        }
        if (bodyLabel) {
            std::string body = newpipe::tr("subscriptions/feed_load_failed_body");
            if (!result.session.source_path.empty()) {
                body += "\n" + newpipe::tr("subscriptions/current_source", result.session.source_path);
            }
            bodyLabel->setText(body);
        }
        clearGrid();
        return;
    }

    clearGrid();
    feedTitle_ = result.feed->kiosk.title;
    continuation_ = result.feed->continuation;
    if (grid) {
        grid->appendItems(result.feed->items);
    }
    if (scrollFrame) {
        scrollFrame->resetPagingTrigger();
    }

    if (statusLabel) {
        statusLabel->setText(
            newpipe::tr("common/count_with_title", feedTitle_, grid ? grid->itemCount() : 0));
    }
    if (bodyLabel) {
        std::string session_name;
        if (!result.session.display_name.empty()) {
            session_name = result.session.display_name;
        } else if (!result.session.source_path.empty()) {
            session_name = result.session.source_path;
        } else {
            session_name = newpipe::tr("settings/session/saved");
        }
        std::string body = newpipe::tr("subscriptions/session_prefix", session_name);
        body += "\n" + newpipe::tr("subscriptions/controls");
        bodyLabel->setText(body);
    }
}

bool SubscriptionsTab::requestMorePages() {
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

void SubscriptionsTab::applyPage(const PageResult& result) {
    loadingMore_ = false;

    if (!result.page.has_value() || result.page->items.empty()) {
        // Nothing more to show: stop asking rather than retrying forever.
        newpipe::logf("subscriptions: no further pages error=%s", result.error.c_str());
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

void SubscriptionsTab::showSignedOutState() {
    clearGrid();
    if (statusLabel) {
        statusLabel->setText(newpipe::tr("subscriptions/signed_out_title"));
    }
    if (bodyLabel) {
        bodyLabel->setText(
            newpipe::tr("subscriptions/signed_out_body", newpipe::default_auth_import_path()));
    }
}

void SubscriptionsTab::runSessionOp(
    std::function<SessionOpResult()> op,
    std::string success_key,
    std::string failure_key) {
    setBusy(true);
    newpipe::run_async<SessionOpResult>(
        alive_,
        std::move(op),
        [this, success_key, failure_key](const SessionOpResult& result) {
            setBusy(false);
            if (result.ok) {
                brls::Application::notify(newpipe::tr(success_key));
            } else {
                brls::Application::notify(
                    result.error.empty() ? newpipe::tr(failure_key) : result.error);
            }
            this->refresh();
        });
}

void SubscriptionsTab::openSessionDialog() {
    if (loading_) {
        return;
    }

    std::string body;
    if (hasSession_) {
        if (!session_.source_path.empty()) {
            body = newpipe::tr("subscriptions/session_dialog/saved", session_.source_path);
        } else if (!session_.source_label.empty()) {
            body = newpipe::tr("subscriptions/session_dialog/saved", session_.source_label);
        } else {
            body = newpipe::tr("subscriptions/session_dialog/saved", "manual");
        }
    } else {
        body = newpipe::tr("subscriptions/session_dialog/signed_out", newpipe::default_auth_import_path());
    }

    auto* dialog = new brls::Dialog(body);
    dialog->addButton(newpipe::tr("subscriptions/session_dialog/load_file"), [this, dialog]() {
        dialog->close();
        const auto service = service_;
        this->runSessionOp(
            [service]() {
                SessionOpResult result;
                result.ok = service->import_auth_session_from_file({}, &result.error);
                return result;
            },
            "subscriptions/session_dialog/load_file_done",
            "subscriptions/session_dialog/load_file_failed");
    });
    dialog->addButton(newpipe::tr("subscriptions/session_dialog/input_cookie"), [this, dialog]() {
        dialog->close();
        brls::Application::getImeManager()->openForText(
            [this](const std::string& text) { this->handleManualCookieInput(text); },
            newpipe::tr("subscriptions/session_dialog/ime_title"),
            newpipe::tr("subscriptions/session_dialog/ime_subtitle"),
            4096,
            "");
    });
    if (hasSession_) {
        dialog->addButton(newpipe::tr("subscriptions/session_dialog/logout"), [this, dialog]() {
            dialog->close();
            const auto service = service_;
            this->runSessionOp(
                [service]() {
                    SessionOpResult result;
                    result.ok = service->clear_auth_session(&result.error);
                    return result;
                },
                "subscriptions/session_dialog/logout_done",
                "subscriptions/session_dialog/logout_failed");
        });
    }
    dialog->addButton(newpipe::tr("common/close"), [dialog]() { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void SubscriptionsTab::handleManualCookieInput(const std::string& text) {
    if (text.empty()) {
        brls::Application::notify(newpipe::tr("subscriptions/session_dialog/empty_input"));
        return;
    }

    const auto service = service_;
    this->runSessionOp(
        [service, text]() {
            SessionOpResult result;
            result.ok = service->update_auth_session_from_cookie(text, "manual input", &result.error);
            return result;
        },
        "subscriptions/session_dialog/save_cookie_done",
        "subscriptions/session_dialog/save_cookie_failed");
}

void SubscriptionsTab::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::logf("subscriptions: playStream url=%s", item.url.c_str());
    setBusy(true);
    newpipe::launch_playback_async(
        alive_,
        service_,
        item,
        "subscriptions",
        [this](const newpipe::StreamItem& unresolved) {
            setBusy(false);
            openStream(unresolved);
        });
}

void SubscriptionsTab::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    brls::Application::pushActivity(new StreamDetailActivity(item));
}
