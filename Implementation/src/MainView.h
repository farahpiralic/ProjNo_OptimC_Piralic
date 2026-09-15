//  MainView.h
//  Assembles the GUI exactly as section 4 of the proposal describes:
//  a control panel on the left, and on the right THREE plot panels shown
//  simultaneously:
//
//    +-----------+------------------------------------------+
//    | controls  |        contour plot + trajectories       |
//    |           +---------------------+--------------------+
//    |           | convergence (log y) | kappa study(loglog)|
//    +-----------+---------------------+--------------------+
//
//  All splitters are user-adjustable. This class also owns the benchmark
//  suite and the run results, executes the optimizers (they finish in
//  milliseconds, so everything is synchronous on the GUI thread), and
//  routes data to the three canvases.
#pragma once

#include <gui/View.h>
#include <gui/SplitterLayout.h>
#include "Objective.h"
#include "Optimizer.h"
#include "CondStudy.h"
#include "ControlsView.h"
#include "ContourCanvas.h"
#include "ChartCanvas.h"
#include <vector>
#include <memory>

class MainView : public gui::View
{
private:
    std::vector<std::unique_ptr<opt::IObjective>> _suite;
    std::vector<opt::RunResult> _runs;
    const opt::IObjective* _lastFn = nullptr; // to re-fit the view only on change

    ControlsView  _controls;
    ContourCanvas _contour;
    ChartCanvas   _chartConv;
    ChartCanvas   _chartCond;

    gui::View _bottomHost {0, 0, 0, 0};
    gui::View _rightHost  {0, 0, 0, 0};

    gui::SplitterLayout _splitBottom {gui::SplitterLayout::Orientation::Horizontal,
                                      gui::SplitterLayout::AuxiliaryCell::Second};
    gui::SplitterLayout _splitRight  {gui::SplitterLayout::Orientation::Vertical,
                                      gui::SplitterLayout::AuxiliaryCell::Second};
    gui::SplitterLayout _splitMain   {gui::SplitterLayout::Orientation::Horizontal,
                                      gui::SplitterLayout::AuxiliaryCell::First};

public:
    MainView()
    : _suite(opt::makeBenchmarkSuite())
    , _controls(_suite)
    {
        _chartConv.setLabels(tr("convTitle"), tr("axIter"), tr("axGNorm"));
        _chartConv.setLogY(true);

        _chartCond.setLabels(tr("condTitle"), tr("axKappa"), tr("axIters"));
        _chartCond.setLogX(true);
        _chartCond.setLogY(true);

        // --- layout: nested splitters -------------------------------------
        // Without explicit minimums the splitter measures the bare canvases
        // as ~0 and collapses the chart row on startup (canvases have no
        // intrinsic size). UseAsMin also stops the user from dragging the
        // charts completely away.
        _chartConv.setSizeLimits(320, gui::Control::Limit::UseAsMin,
                                 300, gui::Control::Limit::UseAsMin);
        _chartCond.setSizeLimits(380, gui::Control::Limit::UseAsMin,
                                 300, gui::Control::Limit::UseAsMin);
        _bottomHost.setSizeLimits(0, gui::Control::Limit::None,
                                  330, gui::Control::Limit::UseAsMin);

        _splitBottom.setContent(_chartConv, _chartCond);
        _bottomHost.setLayout(&_splitBottom);

        _splitRight.setContent(_contour, _bottomHost);
        _rightHost.setLayout(&_splitRight);

        _splitMain.setContent(_controls, _rightHost);
        setLayout(&_splitMain);

        // --- wiring ---------------------------------------------------------
        _controls.onRun             = [this]() { run(); };
        _controls.onStudy           = [this]() { runStudy(); };
        _controls.onResetView       = [this]() { resetView(); };
        _controls.onFunctionChanged = [this]() { selectFunction(); };

        _contour.onPickStart = [this](double x, double y)
        {
            _controls.setStart(x, y);
            run(); // instant feedback: new trajectories from the picked x0
        };

        // initial state (data only - drawing happens once the window shows)
        applyFunctionDefaults();
    }

    // called by MainWindow once the window is on screen
    void initialRun()
    {
        run();
    }

    opt::IObjective& currentFunction()
    {
        return *_suite[(size_t)_controls.functionIndex() % _suite.size()];
    }

    void selectFunction()
    {
        applyFunctionDefaults();
        _runs.clear();
        _contour.setRuns(&_runs);
        _chartConv.clearSeries();
        run(); // immediately show the comparison on the new function
    }

    //  Reset = clean slate, exactly like before anything ran: empty contour
    //  panel (no function drawn), empty charts, empty log. Pressing Run (or
    //  picking a function / right-clicking the canvas) starts fresh - and
    //  re-frames the view, so this is also the recovery after a wild
    //  pan/zoom.
    void resetView()
    {
        _runs.clear();
        _contour.setRuns(&_runs);
        _contour.clearAll();
        _chartConv.clearSeries();
        _chartCond.clearSeries();
        _controls.clearLog();
        _lastFn = nullptr; // next run re-attaches the function and re-fits
    }

    void run()
    {
        opt::IObjective& fn = currentFunction();

        opt::RunOptions opts;
        opts.x0.resize(2, 0.0);
        _controls.getStart(opts.x0[0], opts.x0[1]);
        opts.eps = _controls.eps();
        opts.maxIter = _controls.maxIter();

        const opt::StepStrategy strat = _controls.strategy();
        const int sel = _controls.methodSelection();

        std::vector<opt::Method> methods;
        if (sel == 0) methods = {opt::Method::SteepestDescent};
        else if (sel == 1) methods = {opt::Method::Newton};
        else if (sel == 2) methods = {opt::Method::BFGS};
        else methods = {opt::Method::SteepestDescent, opt::Method::Newton,
                        opt::Method::BFGS};

        _runs.clear();
        for (opt::Method m : methods)
            _runs.push_back(opt::Optimizer::run(m, strat, fn, opts));

        // contour panel
        if (&fn != _lastFn)
        {
            _contour.setFunction(&fn); // re-fits the view for the new function
            _lastFn = &fn;
        }
        _contour.setStart(opts.x0[0], opts.x0[1]);
        _contour.setRuns(&_runs);

        // convergence panel: ||g|| vs iteration, one line per method
        _chartConv.clearSeries();
        for (const opt::RunResult& r : _runs)
        {
            std::vector<double> xs, ys;
            xs.reserve(r.trace.size());
            ys.reserve(r.trace.size());
            for (size_t k = 0; k < r.trace.size(); ++k)
            {
                xs.push_back((double)k);
                ys.push_back(r.trace[k].gNorm);
            }
            _chartConv.addSeries(xs, ys, ContourCanvas::methodColor(r.method),
                                 opt::toString(r.method));
        }

        // step-by-step animation of the trajectories and convergence curves
        if (_controls.animate())
        {
            _contour.beginReveal();
            _chartConv.beginReveal();
        }

        // log
        td::String line;
        line.format("--- %s | %s | x0=(%g, %g) | eps=%g | maxIter=%d\n",
                    fn.name(), opt::toString(strat), opts.x0[0], opts.x0[1],
                    opts.eps, opts.maxIter);
        _controls.appendLog(line);
        for (const opt::RunResult& r : _runs)
        {
            if (r.newtonFallbacks > 0)
                line.format("%s: %s, %d it, f*=%g, |g|=%g, %d fallback(s), %.2f ms\n",
                            opt::toString(r.method), opt::toString(r.status),
                            r.iterations, r.finalF, r.finalGNorm,
                            r.newtonFallbacks, r.ms);
            else
                line.format("%s: %s, %d it, f*=%g, |g|=%g, %.2f ms\n",
                            opt::toString(r.method), opt::toString(r.status),
                            r.iterations, r.finalF, r.finalGNorm, r.ms);
            _controls.appendLog(line);
        }
    }

    void runStudy()
    {
        const opt::StepStrategy strat = _controls.strategy();
        opt::CondStudyResult cs = opt::CondStudy::run(strat, 1e-6);

        std::vector<double> ks, sd, nw, bf;
        bool allConverged = true;
        for (const opt::CondStudyPoint& p : cs.points)
        {
            ks.push_back(p.kappa);
            sd.push_back((double)p.itersSD);
            nw.push_back((double)p.itersNewton);
            bf.push_back((double)p.itersBFGS);
            if (p.stSD != opt::RunStatus::Converged ||
                p.stNewton != opt::RunStatus::Converged ||
                p.stBFGS != opt::RunStatus::Converged)
                allConverged = false;
        }

        _chartCond.clearSeries();
        _chartCond.addSeries(ks, sd, ContourCanvas::methodColor(opt::Method::SteepestDescent),
                             opt::toString(opt::Method::SteepestDescent));
        _chartCond.addSeries(ks, nw, ContourCanvas::methodColor(opt::Method::Newton),
                             opt::toString(opt::Method::Newton));
        _chartCond.addSeries(ks, bf, ContourCanvas::methodColor(opt::Method::BFGS),
                             opt::toString(opt::Method::BFGS));

        if (_controls.animate())
            _chartCond.beginReveal();

        td::String line;
        line.format("--- kappa study (%s): kappa = 1 .. 1e4, eps = 1e-6\n",
                    opt::toString(strat));
        _controls.appendLog(line);
        const opt::CondStudyPoint& last = cs.points.back();
        line.format("kappa=1e4: SD=%d, Newton=%d, BFGS=%d iterations%s\n",
                    last.itersSD, last.itersNewton, last.itersBFGS,
                    allConverged ? "" : " (some runs hit the iteration cap)");
        _controls.appendLog(line);
    }

private:
    void applyFunctionDefaults()
    {
        opt::IObjective& fn = currentFunction();
        double x0[2] = {0.0, 0.0};
        fn.suggestedStart(x0);
        _controls.setStart(x0[0], x0[1]);
        _contour.setFunction(&fn); // fits the suggested view
        _lastFn = &fn;
        _contour.setStart(x0[0], x0[1]);
    }
};
