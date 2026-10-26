#include "activity/main_activity.hpp"

#include "newpipe/i18n.hpp"
#include "newpipe/settings_store.hpp"

namespace {

size_t startup_tab_index(const std::string& tab_id) {
    // Tab order: home(0) shorts(1) search(2) subscriptions(3) library(4) settings(5)
    if (tab_id == "shorts") {
        return 1;
    }
    if (tab_id == "search") {
        return 2;
    }
    if (tab_id == "subscriptions") {
        return 3;
    }
    if (tab_id == "library") {
        return 4;
    }
    if (tab_id == "settings") {
        return 5;
    }
    return 0;
}

}  // namespace

void MainActivity::onContentAvailable() {
    this->registerAction(newpipe::tr("common/info"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        auto* dialog = new brls::Dialog(newpipe::tr("app/info_body", APP_VERSION));
        dialog->addButton(newpipe::tr("hints/ok"), [dialog]() { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
        return true;
    });

    if (this->tabsFrame) {
        const size_t index = startup_tab_index(newpipe::SettingsStore::instance().settings().startup_tab);
        this->tabsFrame->setDefaultTabIndex(index);
        this->tabsFrame->focusTab(static_cast<int>(index));
    }
}
