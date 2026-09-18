#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <borealis.hpp>

#include "newpipe/models.hpp"

// The card grid every feed screen is built from.
//
// It used to be four near-identical copies of the same "clear everything, then
// rebuild every row from scratch" loop, one per tab. Rebuilding is what made
// paging impossible: it throws away the views the user is looking at and drops
// the scroll position back to the top. This one appends instead, so a page that
// arrives while the user is halfway down the feed simply extends it.
class StreamGrid : public brls::Box {
public:
    StreamGrid();

    // Drops every card. Call newpipe::release_grid_focus() first if the focus
    // could be inside the grid.
    void reset();

    // Adds cards for `items` after the ones already shown, filling the trailing
    // row before starting a new one so pages that do not divide evenly by the
    // column count still tile without gaps.
    void appendItems(const std::vector<newpipe::StreamItem>& items);

    size_t itemCount() const { return this->items_.size(); }
    bool empty() const { return this->items_.empty(); }

    void setOnPlay(std::function<void(const newpipe::StreamItem&)> callback) {
        this->onPlay_ = std::move(callback);
    }

    void setOnInfo(std::function<void(const newpipe::StreamItem&)> callback) {
        this->onInfo_ = std::move(callback);
    }

    static brls::View* create() { return new StreamGrid(); }

private:
    void addCard(size_t index);

    std::vector<newpipe::StreamItem> items_;
    brls::Box* trailingRow_ = nullptr;
    size_t trailingRowCount_ = 0;
    std::function<void(const newpipe::StreamItem&)> onPlay_;
    std::function<void(const newpipe::StreamItem&)> onInfo_;
};
