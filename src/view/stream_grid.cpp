#include "view/stream_grid.hpp"

#include "newpipe/i18n.hpp"
#include "newpipe/log.hpp"
#include "view/stream_card.hpp"

namespace {
// Four cards fit the 1280x720 design space at a readable card size.
constexpr size_t kGridColumns = 4;
}  // namespace

StreamGrid::StreamGrid() {
    // Width and height stay at yoga's auto default (the XML says so explicitly
    // too); setWidth(View::AUTO) would not work here anyway, since AUTO is NaN
    // and borealis compares it with ==.
    this->setAxis(brls::Axis::COLUMN);
}

void StreamGrid::reset() {
    this->clearViews();
    this->items_.clear();
    this->trailingRow_ = nullptr;
    this->trailingRowCount_ = 0;
}

void StreamGrid::appendItems(const std::vector<newpipe::StreamItem>& items) {
    if (items.empty()) {
        return;
    }

    this->items_.reserve(this->items_.size() + items.size());
    for (const auto& item : items) {
        this->items_.push_back(item);
        this->addCard(this->items_.size() - 1);
    }

    newpipe::logf("grid: append=%zu total=%zu", items.size(), this->items_.size());
}

void StreamGrid::addCard(size_t index) {
    if (!this->trailingRow_ || this->trailingRowCount_ >= kGridColumns) {
        auto* row = new brls::Box(brls::Axis::ROW);
        row->setMarginBottom(10);
        this->addView(row);
        this->trailingRow_ = row;
        this->trailingRowCount_ = 0;
    }

    auto* card = new StreamCard();
    card->setData(this->items_[index]);
    card->registerClickAction([this, index](brls::View*) {
        if (this->onPlay_ && index < this->items_.size()) {
            this->onPlay_(this->items_[index]);
        }
        return true;
    });
    card->registerAction(
        newpipe::tr("common/info"),
        brls::ControllerButton::BUTTON_Y,
        [this, index](brls::View*) {
            if (this->onInfo_ && index < this->items_.size()) {
                this->onInfo_(this->items_[index]);
            }
            return true;
        });
    card->addGestureRecognizer(new brls::TapGestureRecognizer(card));

    this->trailingRow_->addView(card);
    this->trailingRowCount_++;
}
