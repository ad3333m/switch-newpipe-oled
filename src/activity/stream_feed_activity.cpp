#include "activity/stream_feed_activity.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_launcher.hpp"

StreamFeedActivity::StreamFeedActivity(std::string title, std::vector<newpipe::StreamItem> items)
    : title_(std::move(title))
    , items_(std::move(items)) {
}

void StreamFeedActivity::onContentAvailable() {
    brls::delay(500, [this]() { interactionReady_.store(true); });
    this->registerAction(newpipe::tr("hints/back"), brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    if (this->statusLabel) {
        this->statusLabel->setText(this->title_);
    }
    if (this->subtitleLabel) {
        this->subtitleLabel->setText(newpipe::tr("feed/subtitle"));
    }

    if (this->grid) {
        this->grid->setOnPlay([this](const newpipe::StreamItem& item) { this->playStream(item); });
        this->grid->setOnInfo([this](const newpipe::StreamItem& item) { this->openStream(item); });
        this->grid->appendItems(this->items_);
    }
}

bool StreamFeedActivity::allowInitialInput() const {
    return interactionReady_.load();
}

void StreamFeedActivity::playStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    newpipe::launch_playback_async(
        alive_,
        this->service_,
        item,
        "feed_activity",
        [this](const newpipe::StreamItem& unresolved) { this->openStream(unresolved); });
}

void StreamFeedActivity::openStream(const newpipe::StreamItem& item) {
    if (!allowInitialInput()) {
        return;
    }
    brls::Application::pushActivity(new StreamDetailActivity(item));
}
