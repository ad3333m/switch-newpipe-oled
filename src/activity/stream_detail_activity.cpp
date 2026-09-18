#include "activity/comment_feed_activity.hpp"
#include "activity/stream_detail_activity.hpp"

#include "activity/stream_feed_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_launcher.hpp"
#include "view/fade.hpp"

StreamDetailActivity::StreamDetailActivity(newpipe::StreamItem item)
    : item_(std::move(item)) {
}

void StreamDetailActivity::onContentAvailable() {
    this->registerAction(newpipe::tr("hints/back"), brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });
    this->registerAction(newpipe::tr("detail/play_action"), brls::BUTTON_A, [this](brls::View*) {
        this->playStream();
        return true;
    });
    this->registerAction(newpipe::tr("detail/channel_action"), brls::BUTTON_X, [this](brls::View*) {
        this->openChannelFeed();
        return true;
    });
    this->registerAction(newpipe::tr("detail/related_action"), brls::BUTTON_Y, [this](brls::View*) {
        this->openRelatedFeed();
        return true;
    });
    this->registerAction(newpipe::tr("detail/more_action"), brls::BUTTON_LB, [this](brls::View*) {
        this->openExtrasMenu();
        return true;
    });
    this->registerAction(newpipe::tr("detail/favorite_action"), brls::BUTTON_RB, [this](brls::View*) {
        this->toggleFavorite();
        return true;
    });

    // Fill in what the list already knew before the network answers, so the
    // screen is never blank while the player API is being asked.
    this->applyDetail(std::nullopt);
    this->loadDetail();
}

void StreamDetailActivity::setBusy(bool busy) {
    this->busy_ = busy;
    if (this->spinner) {
        if (busy) {
            newpipe::fade_in(this->spinner);
        } else {
            newpipe::fade_out_gone(this->spinner);
        }
    }
}

void StreamDetailActivity::loadDetail() {
    setBusy(true);

    const std::string url = this->item_.url;
    const auto service = service_;
    newpipe::run_async<std::optional<newpipe::StreamDetail>>(
        alive_,
        [service, url]() { return service->get_stream_detail(url); },
        [this](const std::optional<newpipe::StreamDetail>& detail) {
            setBusy(false);
            this->applyDetail(detail);
        });
}

void StreamDetailActivity::applyDetail(const std::optional<newpipe::StreamDetail>& detail) {
    if (detail.has_value()) {
        this->item_ = detail->item;
    }

    if (this->titleLabel) {
        this->titleLabel->setText(
            this->item_.title.empty() ? newpipe::tr("detail/default_title") : this->item_.title);
    }

    std::string meta;
    const auto append_meta = [&meta](const std::string& value) {
        if (value.empty()) {
            return;
        }
        if (!meta.empty()) {
            meta += " • ";
        }
        meta += value;
    };
    append_meta(this->item_.channel_name);
    append_meta(this->item_.view_count_text);
    append_meta(this->item_.published_text);
    append_meta(this->item_.duration_text);
    if (this->metaLabel) {
        this->metaLabel->setText(meta);
    }

    std::string body;
    if (detail.has_value() && !detail->description.empty()) {
        body = detail->description;
    }
    if (body.empty()) {
        body = newpipe::tr("detail/no_description");
    }
    if (!this->item_.url.empty()) {
        body += "\n\n" + this->item_.url;
    }

    if (this->bodyLabel) {
        this->bodyLabel->setText(body);
    }
    if (this->statusLabel) {
        std::string status = newpipe::tr("detail/status_play");
        const bool has_channel = !this->item_.channel_id.empty() || !this->item_.channel_url.empty();
        const bool has_related = detail.has_value() && !detail->related_items.empty();
        const bool has_playlist = this->item_.url.find("list=") != std::string::npos;
        status += has_channel ? newpipe::tr("detail/status_channel") : "";
        status += has_related ? newpipe::tr("detail/status_related") : "";
        status += has_playlist ? newpipe::tr("detail/status_more") : newpipe::tr("detail/status_comments");
        status += newpipe::tr("detail/status_favorite");
        this->statusLabel->setText(status);

        if (this->getContentView()) {
            this->getContentView()->setActionAvailable(brls::BUTTON_X, has_channel);
            this->getContentView()->setActionAvailable(brls::BUTTON_Y, has_related);
            this->getContentView()->setActionAvailable(brls::BUTTON_LB, true);
        }
    }

    this->updateFavoriteAction();
}

void StreamDetailActivity::updateFavoriteAction() {
    if (this->getContentView()) {
        const bool favorite = newpipe::LibraryStore::instance().is_favorite(this->item_.url);
        this->getContentView()->updateActionHint(
            brls::BUTTON_RB,
            favorite ? newpipe::tr("detail/unfavorite_action")
                     : newpipe::tr("detail/favorite_action"));
    }
}

void StreamDetailActivity::playStream() {
    if (this->busy_) {
        return;
    }
    setBusy(true);
    newpipe::launch_playback_async(
        alive_,
        this->service_,
        this->item_,
        "detail",
        [this](const newpipe::StreamItem&) {
            setBusy(false);
            brls::Application::notify(newpipe::tr("detail/playback_url_failed"));
        });
}

void StreamDetailActivity::openFeed(
    std::function<FeedResult()> fetch,
    std::string failure_key,
    std::function<std::string(const newpipe::HomeFeed&)> title_for) {
    if (this->busy_) {
        return;
    }
    setBusy(true);

    newpipe::run_async<FeedResult>(
        alive_,
        std::move(fetch),
        [this, failure_key, title_for](const FeedResult& result) {
            setBusy(false);
            if (!result.feed.has_value()) {
                brls::Application::notify(
                    result.error.empty() ? newpipe::tr(failure_key) : result.error);
                return;
            }

            brls::Application::pushActivity(
                new StreamFeedActivity(title_for(*result.feed), result.feed->items));
        });
}

void StreamDetailActivity::openChannelFeed() {
    const auto service = service_;
    const newpipe::StreamItem item = this->item_;
    this->openFeed(
        [service, item]() {
            FeedResult result;
            result.feed = service->get_channel_feed(item);
            result.error = service->error_message();
            return result;
        },
        "detail/channel_load_failed",
        [](const newpipe::HomeFeed& feed) { return feed.kiosk.title; });
}

void StreamDetailActivity::openRelatedFeed() {
    const auto service = service_;
    const newpipe::StreamItem item = this->item_;
    this->openFeed(
        [service, item]() {
            FeedResult result;
            result.feed = service->get_related_feed(item);
            result.error = service->error_message();
            return result;
        },
        "detail/related_load_failed",
        [](const newpipe::HomeFeed& feed) { return feed.kiosk.title; });
}

void StreamDetailActivity::openExtrasMenu() {
    auto* dialog = new brls::Dialog(newpipe::tr("detail/extras_title"));

    if (this->item_.url.find("list=") != std::string::npos) {
        dialog->addButton(newpipe::tr("detail/playlist_action"), [this, dialog]() {
            dialog->close();
            this->openPlaylistFeed();
        });
    }

    dialog->addButton(newpipe::tr("detail/comments_action"), [this, dialog]() {
        dialog->close();
        this->openComments();
    });
    dialog->addButton(newpipe::tr("common/close"), [dialog]() { dialog->close(); });
    dialog->setCancelable(true);
    dialog->open();
}

void StreamDetailActivity::openPlaylistFeed() {
    const auto service = service_;
    const newpipe::StreamItem item = this->item_;
    this->openFeed(
        [service, item]() {
            FeedResult result;
            result.feed = service->get_playlist_feed(item);
            result.error = service->error_message();
            return result;
        },
        "detail/playlist_load_failed",
        [](const newpipe::HomeFeed& feed) {
            std::string title = feed.kiosk.title;
            if (title.empty() || title == "재생목록" || title == "Playlist") {
                title = newpipe::tr("detail/playlist_action");
            }
            return title;
        });
}

void StreamDetailActivity::openComments() {
    if (this->busy_) {
        return;
    }
    setBusy(true);

    struct CommentsResult {
        std::optional<newpipe::CommentPage> page;
        std::string error;
    };

    const auto service = service_;
    const newpipe::StreamItem item = this->item_;
    newpipe::run_async<CommentsResult>(
        alive_,
        [service, item]() {
            CommentsResult result;
            result.page = service->get_comments(item);
            result.error = service->error_message();
            return result;
        },
        [this](const CommentsResult& result) {
            setBusy(false);
            if (!result.page.has_value()) {
                brls::Application::notify(
                    result.error.empty() ? newpipe::tr("detail/comments_load_failed")
                                         : result.error);
                return;
            }

            std::string title = result.page->title;
            if (title.empty() || title == "댓글" || title == "Comments") {
                title = newpipe::tr("comments/title");
            }

            brls::Application::pushActivity(new CommentFeedActivity(title, result.page->items));
        });
}

void StreamDetailActivity::toggleFavorite() {
    bool favorite = false;
    std::string error;
    if (!newpipe::LibraryStore::instance().toggle_favorite(this->item_, &favorite, &error)) {
        brls::Application::notify(error.empty() ? newpipe::tr("detail/favorite_save_failed") : error);
        return;
    }

    this->updateFavoriteAction();
    brls::Application::notify(
        favorite ? newpipe::tr("detail/favorite_added")
                 : newpipe::tr("detail/favorite_removed"));
}
