//  MainWindow.h
#pragma once
#include <gui/Window.h>
#include "MenuBar.h"
#include "ToolBar.h"
#include "MainView.h"
#include "DialogSettings.h"
#include "Constants.h"

class MainWindow : public gui::Window
{
protected:
    MenuBar _mainMenuBar;
    ToolBar _toolBar;
    MainView _mainView;
    const td::UINT4 _cSettingsDlgID = 1000;

protected:
    void onInitialAppearance() override
    {
        _mainView.initialRun(); // populate all three panels on startup
    }
    
    bool onActionItem(gui::ActionItemDescriptor& aiDesc) override
    {
        auto [menuID, firstSubMenuID, lastSubMenuID, actionID] = aiDesc.getIDs();
        switch (menuID)
        {
            case cMenuApp:
            {
                auto pDlg = getAttachedWindow(_cSettingsDlgID);
                if (pDlg)
                    pDlg->setFocus();
                else
                {
                    DialogSettings* pSettingsDlg =
                        new DialogSettings(this, _cSettingsDlgID);
                    pSettingsDlg->keepOnTopOfParent();
                    pSettingsDlg->setMainTB(&_toolBar);
                    pSettingsDlg->open();
                }
                return true;
            }
            case cMenuRun:
            {
                if (actionID == cActionRun)
                {
                    _mainView.run();
                    return true;
                }
                if (actionID == cActionStudy)
                {
                    _mainView.runStudy();
                    return true;
                }
                if (actionID == cActionResetView)
                {
                    _mainView.resetView();
                    return true;
                }
                break;
            }
            case cMenuHelp:
            {
                if (actionID == cActionAbout)
                {
                    showAlert(tr("aboutTitle"), tr("aboutText"));
                    return true;
                }
                break;
            }
            default:
                break;
        }
        return false;
    }

public:
    MainWindow()
    : gui::Window(gui::Geometry(40, 30, 1240, 850))
    {
        setTitle(tr("appTitle"));
        _mainMenuBar.setAsMain(this);
        setToolBar(_toolBar);
        setCentralView(&_mainView);
    }
};
