//  OptimCompare - Project #16 (F. Piralić, 20106)
//
//  ControlsView.h
//  The control panel from the proposal (section 4.1):
//    * method selector (steepest descent / Newton / BFGS / all three),
//    * step-size strategy selector (exact line search / Armijo),
//    * function picker over the benchmark suite,
//    * input fields for the starting point x0, the maximum number of
//      iterations, and the convergence tolerance eps,
//    * Run / kappa-study / Reset-view buttons and a read-only results log.
//
//  The view is passive: it exposes the current parameter values through
//  getters and notifies MainView through std::function callbacks.
#pragma once

#include <gui/View.h>
#include <gui/Label.h>
#include <gui/ComboBox.h>
#include <gui/LineEdit.h>
#include <gui/NumericEdit.h>
#include <gui/CheckBox.h>
#include <gui/Button.h>
#include <gui/TextEdit.h>
#include <gui/GridLayout.h>
#include <gui/GridComposer.h>
#include "Objective.h"
#include "LineSearch.h"
#include <functional>
#include <cstdlib>
#include <vector>
#include <memory>

class ControlsView : public gui::View
{
public:
    std::function<void()> onRun;
    std::function<void()> onStudy;
    std::function<void()> onResetView;
    std::function<void()> onFunctionChanged;

private:
    gui::Label _lblFunction;
    gui::ComboBox _cmbFunction;
    gui::Label _lblMethod;
    gui::ComboBox _cmbMethod;
    gui::Label _lblStrategy;
    gui::ComboBox _cmbStrategy;
    gui::Label _lblX0x;
    gui::NumericEdit _neX0x;
    gui::Label _lblX0y;
    gui::NumericEdit _neX0y;
    gui::Label _lblMaxIter;
    gui::NumericEdit _neMaxIter;
    gui::Label _lblEps;
    gui::LineEdit _leEps;
    gui::CheckBox _chbAnimate;
    gui::Button _btnRun;
    gui::Button _btnStudy;
    gui::Button _btnReset;
    gui::TextEdit _log;
    gui::GridLayout _gl;

public:
    explicit ControlsView(const std::vector<std::unique_ptr<opt::IObjective>>& suite)
    : _lblFunction(tr("lblFunction"))
    , _lblMethod(tr("lblMethod"))
    , _lblStrategy(tr("lblStrategy"))
    , _lblX0x(tr("lblX0x"))
    , _neX0x(td::real8, gui::LineEdit::Messages::DoNotSend, false, "", 4)
    , _lblX0y(tr("lblX0y"))
    , _neX0y(td::real8, gui::LineEdit::Messages::DoNotSend, false, "", 4)
    , _lblMaxIter(tr("lblMaxIter"))
    , _neMaxIter(td::int4, gui::LineEdit::Messages::DoNotSend, false)
    , _lblEps(tr("lblEps"))
    , _chbAnimate(tr("chbAnimate"))
    , _btnRun(tr("btnRun"))
    , _btnStudy(tr("btnStudy"))
    , _btnReset(tr("btnReset"))
    , _log(gui::TextEdit::HorizontalScroll::Yes, gui::TextEdit::Events::DoNotSend, true)
    , _gl(12, 2)
    {
        for (const auto& fn : suite)
            _cmbFunction.addItem(fn->name());
        _cmbFunction.selectIndex(0);

        _cmbMethod.addItem(tr("methodSD"));
        _cmbMethod.addItem(tr("methodNewton"));
        _cmbMethod.addItem(tr("methodBFGS"));
        _cmbMethod.addItem(tr("methodAll"));
        _cmbMethod.selectIndex(3); // "all three" - the comparative default

        _cmbStrategy.addItem(tr("stratExact"));
        _cmbStrategy.addItem(tr("stratArmijo"));
        _cmbStrategy.selectIndex(0);

        _neMaxIter.setValue(td::Variant((td::INT4)500));
        _leEps.setText("1e-6");
        _chbAnimate.setChecked(true);

        gui::GridComposer gc(_gl);
        gc.appendRow(_lblFunction) << _cmbFunction;
        gc.appendRow(_lblMethod)   << _cmbMethod;
        gc.appendRow(_lblStrategy) << _cmbStrategy;
        gc.appendRow(_lblX0x)      << _neX0x;
        gc.appendRow(_lblX0y)      << _neX0y;
        gc.appendRow(_lblMaxIter)  << _neMaxIter;
        gc.appendRow(_lblEps)      << _leEps;
        gc.appendRow(_chbAnimate, 0);
        gc.appendRow(_btnRun, 0);
        gc.appendRow(_btnStudy, 0);
        gc.appendRow(_btnReset, 0);
        gc.appendRow(_log, 0);
        setLayout(&_gl);

        _btnRun.onClick([this]()   { if (onRun) onRun(); });
        _btnStudy.onClick([this]() { if (onStudy) onStudy(); });
        _btnReset.onClick([this]() { if (onResetView) onResetView(); });
        _cmbFunction.onChangedSelection([this]() {
            if (onFunctionChanged) onFunctionChanged();
        });
    }

    int functionIndex() const
    {
        const int i = _cmbFunction.getSelectedIndex();
        return (i < 0) ? 0 : i;
    }

    // 0 = SD, 1 = Newton, 2 = BFGS, 3 = all three
    int methodSelection() const
    {
        const int i = _cmbMethod.getSelectedIndex();
        return (i < 0) ? 3 : i;
    }

    opt::StepStrategy strategy() const
    {
        return (_cmbStrategy.getSelectedIndex() == 1)
                   ? opt::StepStrategy::Armijo
                   : opt::StepStrategy::ExactLineSearch;
    }

    void getStart(double& x, double& y)
    {
        x = 0.0; y = 0.0;
        _neX0x.getValue(x);
        _neX0y.getValue(y);
    }

    void setStart(double x, double y)
    {
        _neX0x.setValue(td::Variant(x));
        _neX0y.setValue(td::Variant(y));
    }

    int maxIter()
    {
        td::INT4 v = 500;
        _neMaxIter.getValue(v);
        if (v < 1)
            v = 1;
        return (int)v;
    }

    bool animate() const
    {
        return _chbAnimate.isChecked();
    }

    double eps() const
    {
        const td::String s = _leEps.getText();
        const double v = std::strtod(s.c_str(), nullptr);
        return (v > 0.0 && std::isfinite(v)) ? v : 1e-6;
    }

    void appendLog(const td::String& line)
    {
        _log.appendString(line);
    }

    void clearLog()
    {
        _log.setText("");
    }
};
