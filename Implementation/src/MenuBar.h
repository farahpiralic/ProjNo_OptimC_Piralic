//  OptimCompare - Project #16 (F. Piralić, 20106)
//  MenuBar.h
#pragma once
#include <gui/MenuBar.h>
#include "Constants.h"

class MenuBar : public gui::MenuBar
{
private:
    gui::SubMenu _subApp;
    gui::SubMenu _subRun;
    gui::SubMenu _subHelp;

protected:
    void populateAppMenu()
    {
        auto& items = _subApp.getItems();
        items[0].initAsActionItem(tr("settings"), 10); // translated in natGUI
        items[1].initAsSeparator();
        items[2].initAsQuitAppActionItem(tr("Quit"), "q"); // translated in natGUI
    }

    void populateRunMenu()
    {
        auto& items = _subRun.getItems();
        items[0].initAsActionItem(tr("run"), cActionRun, "r");
        items[1].initAsActionItem(tr("runStudy"), cActionStudy, "k");
        items[2].initAsSeparator();
        items[3].initAsActionItem(tr("resetView"), cActionResetView, "0");
    }

    void populateHelpMenu()
    {
        auto& items = _subHelp.getItems();
        items[0].initAsActionItem(tr("about"), cActionAbout);
    }

public:
    MenuBar()
    : gui::MenuBar(3)
    , _subApp(cMenuApp, tr("App"), 3)
    , _subRun(cMenuRun, tr("menuRun"), 4)
    , _subHelp(cMenuHelp, tr("menuHelp"), 1)
    {
        populateAppMenu();
        populateRunMenu();
        populateHelpMenu();
        _menus[0] = &_subApp;
        _menus[1] = &_subRun;
        _menus[2] = &_subHelp;
    }
};
