//  OptimCompare - Project #16 (F. Piralić, 20106)
//  ToolBar.h
#pragma once
#include <gui/ToolBar.h>
#include <gui/Image.h>
#include "Constants.h"

class ToolBar : public gui::ToolBar
{
    gui::Image _imgSettings;
    gui::Image _imgRun;

public:
    ToolBar()
    : gui::ToolBar("mainTB", 2)
    , _imgSettings(":settings")
    , _imgRun(":start")
    {
        addItem(tr("settings"), &_imgSettings, tr("settingsTT"), cMenuApp, 0, 0, 10);
        addItem(tr("run"), &_imgRun, tr("runTT"), cMenuRun, 0, 0, cActionRun);
        // (no toolbar item for the kappa study - the panel button and the
        //  Run menu entry cover it)
    }
};
