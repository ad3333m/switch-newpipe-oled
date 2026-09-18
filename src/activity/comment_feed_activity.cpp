#include "activity/comment_feed_activity.hpp"

#include "newpipe/i18n.hpp"

CommentFeedActivity::CommentFeedActivity(std::string title, std::vector<newpipe::CommentItem> items)
    : title_(std::move(title))
    , items_(std::move(items)) {
}

void CommentFeedActivity::onContentAvailable() {
    this->registerAction(newpipe::tr("hints/back"), brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    if (this->titleLabel) {
        this->titleLabel->setText(this->title_.empty() ? newpipe::tr("comments/title") : this->title_);
    }
    if (this->subtitleLabel) {
        this->subtitleLabel->setText(newpipe::tr("comments/subtitle"));
    }

    this->buildList();
}

void CommentFeedActivity::buildList() {
    if (!this->listBox) {
        return;
    }

    // Each comment gets its own card instead of running together as one wall of
    // text, and the colours come from the theme so they track the OLED palette.
    brls::Theme theme = brls::Application::getTheme();

    this->listBox->clearViews();
    for (const auto& item : this->items_) {
        auto* block = new brls::Box(brls::Axis::COLUMN);
        block->setMarginBottom(10);
        block->setPadding(14, 16, 14, 16);
        block->setCornerRadius(10);
        block->setBackgroundColor(theme["color/newpipe_card"]);

        auto* author = new brls::Label();
        author->setFontSize(16);
        author->setSingleLine(true);
        author->setMarginBottom(6);
        author->setTextColor(theme["color/newpipe_text_dim"]);
        std::string author_text = item.author_name;
        if (item.is_verified) {
            author_text += " · " + newpipe::tr("comments/verified");
        }
        if (!item.published_text.empty()) {
            if (!author_text.empty()) {
                author_text += " · ";
            }
            author_text += item.published_text;
        }
        author->setText(author_text.empty() ? newpipe::tr("comments/unknown_author") : author_text);
        block->addView(author);

        auto* body = new brls::Label();
        body->setFontSize(17);
        body->setLineHeight(1.5f);
        body->setTextColor(theme["color/newpipe_text"]);
        body->setText(item.body);
        block->addView(body);

        std::string meta_text;
        if (!item.like_count_text.empty()) {
            meta_text += newpipe::tr("comments/likes", item.like_count_text);
        }
        if (!item.reply_count_text.empty()) {
            if (!meta_text.empty()) {
                meta_text += " · ";
            }
            meta_text += newpipe::tr("comments/replies", item.reply_count_text);
        }
        if (!meta_text.empty()) {
            auto* meta = new brls::Label();
            meta->setFontSize(13);
            meta->setSingleLine(true);
            meta->setMarginTop(6);
            meta->setTextColor(theme["color/newpipe_text_faint"]);
            meta->setText(meta_text);
            block->addView(meta);
        }

        this->listBox->addView(block);
    }
}
