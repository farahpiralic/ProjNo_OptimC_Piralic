//  OptimCompare - Project #16 (F. Piralić, 20106)
//
//  ContourCanvas.h
//  The proposal's first plot panel: a 2D contour plot of the selected
//  objective with the optimization trajectories overlaid.
//
//  * Level curves are extracted with marching squares on a 101 x 101 sample
//    grid over the currently visible world rectangle. Levels are chosen as
//    quantiles of the sampled values (3% ... 97%), which distributes the
//    curves sensibly for both flat quadratic bowls and the extreme range of
//    Rosenbrock's valley. Segments are cached in world coordinates and only
//    recomputed when the function or the view changes.
//  * Trajectories: one polyline per method (steepest descent = crimson,
//    Newton = dark orange, BFGS = dark magenta), iterate markers when the
//    trace is short enough to stay readable, and a ring on the final point.
//  * Known stationary points of the benchmark functions are marked (gold
//    circles = minima, gray crosses = saddles), the starting point x0 is a
//    green marker.
//  * Interaction: primary-button drag pans, scroll/pinch zooms,
//    a SECONDARY (right) click sets a new starting point x0.
#pragma once

#include <gui/Canvas.h>
#include <gui/Shape.h>
#include <gui/DrawableString.h>
#include "Objective.h"
#include "Optimizer.h"
#include <vector>
#include <functional>
#include <algorithm>
#include <cmath>
#include <chrono>

class ContourCanvas : public gui::Canvas
{
public:
    std::function<void(double, double)> onPickStart; // right-click => new x0

private:
    const opt::IObjective* _fn = nullptr;
    const std::vector<opt::RunResult>* _runs = nullptr;
    double _x0 = 0.0, _y0 = 0.0;
    bool _hasStart = false;

    // world -> pixel transform: center + uniform scale (px per world unit)
    double _cx = 0.0, _cy = 0.0;
    double _scale = 100.0;
    gui::Size _size {760, 460};
    bool _haveSize = false; // becomes true on the first real onResize
    bool _needFit = false;  // re-fit once the real canvas size is known

    // panning
    bool _panning = false;
    gui::Point _panAnchor {0, 0};
    double _panCX = 0.0, _panCY = 0.0;

    // cached marching-squares segments (world coordinates)
    struct Seg { double x1, y1, x2, y2; int lvl; };
    std::vector<Seg> _segs;
    static constexpr int kLevels = 12;
    static constexpr int kGrid = 101;
    double _ccx = 1e300, _ccy = 1e300, _cscale = -1.0, _cw = -1.0, _ch = -1.0;
    const opt::IObjective* _cfn = nullptr;

    // step-by-step reveal of the trajectories (Canvas animation frames)
    // pacing: per-step time and total-duration clamp (tune to taste)
    static constexpr double kRevealMsPerStep = 240.0;
    static constexpr double kRevealMinMs     = 1500.0;
    static constexpr double kRevealMaxMs     = 8000.0;
    bool _revealing = false;
    bool _stopPending = false; // stop the animation on the NEXT frame, not
                               // mid-draw (mid-draw stops can leave some
                               // backends with a stale pending-frame flag
                               // that swallows later reDraw() calls)
    std::chrono::steady_clock::time_point _revealT0;
    std::vector<double> _revealDurMs; // one duration per run

public:
    ContourCanvas()
    : gui::Canvas({gui::InputDevice::Event::PrimaryClicks,
                   gui::InputDevice::Event::CursorDrag,
                   gui::InputDevice::Event::SecondaryClicks,
                   gui::InputDevice::Event::Zoom})
    {
        enableResizeEvent(true);
        setPreferredFrameRateRange(30, 60); // used by the step-reveal animation
    }

    void setFunction(const opt::IObjective* fn)
    {
        _fn = fn;
        fitToFunction();
        _needFit = !_haveSize; // placeholder size used: re-fit on first resize
    }

    //  Full clear: back to the untouched startup look - no function, no
    //  contours, no start marker, nothing.
    void clearAll()
    {
        cancelReveal();
        _fn = nullptr;
        _hasStart = false;
        _cfn = nullptr; // invalidate the contour cache
        reDraw();
    }

    void setRuns(const std::vector<opt::RunResult>* runs)
    {
        _runs = runs;
        cancelReveal(); // new data: show it fully unless beginReveal() follows
        reDraw();
    }

    //  Step-by-step animation: reveals every trajectory point by point.
    //  Short runs (Newton) get ~120 ms per step so each step is visible;
    //  long runs are compressed so nothing takes more than 6 seconds -
    //  which also makes Newton visibly FINISH first, then BFGS, while
    //  steepest descent is still zigzagging.
    void beginReveal()
    {
        if (!_runs || _runs->empty())
            return;
        _revealDurMs.clear();
        for (const opt::RunResult& r : *_runs)
        {
            const double n = (double)r.trace.size();
            _revealDurMs.push_back(std::min(kRevealMaxMs,
                std::max(kRevealMinMs, n * kRevealMsPerStep)));
        }
        _revealT0 = std::chrono::steady_clock::now();
        _revealing = true;
        _stopPending = false;
        startAnimation(); // onDraw now runs at the preferred frame rate
        reDraw();
    }

    void cancelReveal()
    {
        _stopPending = false;
        if (_revealing)
        {
            _revealing = false;
            stopAnimation();
        }
    }

    //  shared trajectory colors (MainView uses this for the chart series too)
    static td::ColorID methodColor(opt::Method m)
    {
        switch (m)
        {
            case opt::Method::SteepestDescent: return td::ColorID::Crimson;
            case opt::Method::Newton:          return td::ColorID::DarkOrange;
            case opt::Method::BFGS:            return td::ColorID::DarkMagenta;
        }
        return td::ColorID::SysText;
    }

    void setStart(double x, double y)
    {
        _x0 = x; _y0 = y; _hasStart = true;
        reDraw();
    }

    void fitToFunction()
    {
        if (!_fn)
            return;
        double x0, x1, y0, y1;
        _fn->suggestedView(x0, x1, y0, y1);
        _cx = 0.5 * (x0 + x1);
        _cy = 0.5 * (y0 + y1);
        const double w = std::max(1e-9, x1 - x0);
        const double h = std::max(1e-9, y1 - y0);
        _scale = 0.92 * std::min((double)_size.width / w,
                                 (double)_size.height / h);
        // self-heal: if a finished reveal left the animation machinery in an
        // inconsistent state, clear it so this reDraw() cannot be swallowed
        if (!_revealing && isAnimating())
        {
            _stopPending = false;
            stopAnimation();
        }
        _cfn = nullptr; // force a contour recompute for the new framing
        reDraw();
    }

protected:
    // ---- transform ----------------------------------------------------------
    double px(double wx) const { return 0.5 * _size.width  + (wx - _cx) * _scale; }
    double py(double wy) const { return 0.5 * _size.height - (wy - _cy) * _scale; }
    double wx(double p)  const { return _cx + (p - 0.5 * _size.width)  / _scale; }
    double wy(double p)  const { return _cy - (p - 0.5 * _size.height) / _scale; }

    void onResize(const gui::Size& newSize) override
    {
        _size = newSize;
        _haveSize = true;
        if (_needFit && _fn)
        {
            _needFit = false;
            fitToFunction(); // now that the real size is known
        }
    }

    void onPrimaryButtonPressed(const gui::InputDevice& dev) override
    {
        _panning = true;
        _panAnchor = dev.getFramePoint();
        _panCX = _cx;
        _panCY = _cy;
    }

    void onPrimaryButtonReleased(const gui::InputDevice& /*dev*/) override
    {
        _panning = false;
    }

    void onCursorDragged(const gui::InputDevice& dev) override
    {
        if (!_panning)
            return;
        const gui::Point& p = dev.getFramePoint();
        _cx = _panCX - (p.x - _panAnchor.x) / _scale;
        _cy = _panCY + (p.y - _panAnchor.y) / _scale;
        reDraw();
    }

    bool onZoom(const gui::InputDevice& dev) override
    {
        const double s = dev.getScale();
        if (s > 0.0 && std::isfinite(s))
        {
            _scale *= s;
            _scale = std::min(std::max(_scale, 1e-3), 1e7);
            reDraw();
        }
        return true;
    }

    void onSecondaryButtonPressed(const gui::InputDevice& dev) override
    {
        const gui::Point& p = dev.getFramePoint();
        if (onPickStart)
            onPickStart(wx(p.x), wy(p.y));
    }

    // ---- contour extraction -------------------------------------------------
    void addSeg(double x1, double y1, double x2, double y2, int lvl)
    {
        _segs.push_back({x1, y1, x2, y2, lvl});
    }

    void ensureContours()
    {
        if (!_fn)
        {
            _segs.clear();
            return;
        }
        const double W = (double)_size.width, H = (double)_size.height;
        if (_cfn == _fn && _cscale == _scale && _ccx == _cx && _ccy == _cy &&
            _cw == W && _ch == H)
            return; // cache valid
        _cfn = _fn; _cscale = _scale; _ccx = _cx; _ccy = _cy; _cw = W; _ch = H;
        _segs.clear();

        const int N = kGrid;
        const double halfW = 0.5 * W / _scale, halfH = 0.5 * H / _scale;
        const double x0 = _cx - halfW, x1 = _cx + halfW;
        const double y0 = _cy - halfH, y1 = _cy + halfH;

        std::vector<double> xs(N), ys(N), vals((size_t)N * N);
        for (int i = 0; i < N; ++i) xs[i] = x0 + (x1 - x0) * i / (N - 1);
        for (int j = 0; j < N; ++j) ys[j] = y0 + (y1 - y0) * j / (N - 1);

        double p[2];
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N; ++i)
            {
                p[0] = xs[i]; p[1] = ys[j];
                const double v = _fn->f(p);
                vals[(size_t)j * N + i] = std::isfinite(v) ? v : 1e300;
            }

        // quantile levels: robust for both bowls and Rosenbrock's huge range
        std::vector<double> sorted(vals);
        std::sort(sorted.begin(), sorted.end());
        std::vector<double> levels;
        levels.reserve(kLevels);
        for (int L = 0; L < kLevels; ++L)
        {
            const double q = 0.03 + (0.97 - 0.03) * L / (kLevels - 1);
            const double v = sorted[(size_t)(q * (sorted.size() - 1))];
            if (levels.empty() || v > levels.back() + 1e-14 * (1.0 + std::fabs(v)))
                levels.push_back(v);
        }

        for (int li = 0; li < (int)levels.size(); ++li)
        {
            const double L = levels[(size_t)li];
            for (int j = 0; j < N - 1; ++j)
                for (int i = 0; i < N - 1; ++i)
                {
                    const double v00 = vals[(size_t)j * N + i];
                    const double v10 = vals[(size_t)j * N + i + 1];
                    const double v11 = vals[(size_t)(j + 1) * N + i + 1];
                    const double v01 = vals[(size_t)(j + 1) * N + i];
                    const int c = (v00 > L ? 1 : 0) | (v10 > L ? 2 : 0) |
                                  (v11 > L ? 4 : 0) | (v01 > L ? 8 : 0);
                    if (c == 0 || c == 15)
                        continue;

                    const double xa = xs[i], xb = xs[i + 1];
                    const double ya = ys[j], yb = ys[j + 1];
                    auto ip = [L](double va, double vb, double a, double b)
                    {
                        const double den = vb - va;
                        const double t = (std::fabs(den) < 1e-300) ? 0.5 : (L - va) / den;
                        return a + std::min(1.0, std::max(0.0, t)) * (b - a);
                    };
                    // edge crossings: bottom, right, top, left
                    const double bxp = ip(v00, v10, xa, xb), byp = ya;
                    const double rxp = xb, ryp = ip(v10, v11, ya, yb);
                    const double txp = ip(v01, v11, xa, xb), typ = yb;
                    const double lxp = xa, lyp = ip(v00, v01, ya, yb);

                    switch (c)
                    {
                        case 1:  case 14: addSeg(lxp, lyp, bxp, byp, li); break;
                        case 2:  case 13: addSeg(bxp, byp, rxp, ryp, li); break;
                        case 3:  case 12: addSeg(lxp, lyp, rxp, ryp, li); break;
                        case 4:  case 11: addSeg(rxp, ryp, txp, typ, li); break;
                        case 6:  case 9:  addSeg(bxp, byp, txp, typ, li); break;
                        case 7:  case 8:  addSeg(lxp, lyp, txp, typ, li); break;
                        case 5:  case 10:
                        {
                            const double vc = 0.25 * (v00 + v10 + v11 + v01);
                            const bool centerAbove = vc > L;
                            if ((c == 5) == centerAbove)
                            {
                                addSeg(lxp, lyp, bxp, byp, li);
                                addSeg(rxp, ryp, txp, typ, li);
                            }
                            else
                            {
                                addSeg(lxp, lyp, txp, typ, li);
                                addSeg(bxp, byp, rxp, ryp, li);
                            }
                            break;
                        }
                        default: break;
                    }
                }
        }
    }

    static td::ColorID levelColor(int lvl, int nLevels)
    {
        static const td::ColorID ramp[] = {
            td::ColorID::SeaGreen,   td::ColorID::Teal,
            td::ColorID::CadetBlue,  td::ColorID::SteelBlue,
            td::ColorID::DodgerBlue, td::ColorID::RoyalBlue,
            td::ColorID::SlateBlue,  td::ColorID::MediumBlue,
            td::ColorID::Navy };
        const int nRamp = (int)(sizeof(ramp) / sizeof(ramp[0]));
        int idx = (nLevels <= 1) ? 0 : (lvl * (nRamp - 1)) / (nLevels - 1);
        idx = std::min(std::max(idx, 0), nRamp - 1);
        return ramp[idx];
    }

    // ---- drawing --------------------------------------------------------------
    void onDraw(const gui::Rect& /*rect*/) override
    {
        if (_stopPending && !_revealing)
        {
            // deferred stop: the reveal finished on a previous frame;
            // end the animation at the START of a fresh frame and push one
            // clean invalidation through the now-stopped state
            _stopPending = false;
            stopAnimation();
            reDraw();
        }

        const double W = (double)_size.width, H = (double)_size.height;

        ensureContours();

        // axes through the origin (only when a function is displayed)
        if (_fn && px(0.0) >= 0 && px(0.0) <= W)
            gui::Shape::drawLine({px(0.0), 0.0}, {px(0.0), H},
                                 td::ColorID::LightGray, 1.0f, td::LinePattern::Dash);
        if (_fn && py(0.0) >= 0 && py(0.0) <= H)
            gui::Shape::drawLine({0.0, py(0.0)}, {W, py(0.0)},
                                 td::ColorID::LightGray, 1.0f, td::LinePattern::Dash);

        // contour lines
        const int nLv = kLevels;
        for (const Seg& s : _segs)
        {
            gui::Shape::drawLine({px(s.x1), py(s.y1)}, {px(s.x2), py(s.y2)},
                                 levelColor(s.lvl, nLv), 1.0f);
        }

        // known stationary points
        if (_fn)
        {
            for (const auto& sp : _fn->stationaryPoints())
            {
                const gui::Point c(px(sp.x), py(sp.y));
                if (sp.minimum)
                {
                    gui::Shape mark;
                    mark.createCircle(gui::Circle(c, 5.0), 1.6f);
                    mark.drawFillAndWire(td::ColorID::Gold, td::ColorID::DarkOrange);
                }
                else
                {
                    gui::Shape::drawLine({c.x - 5, c.y - 5}, {c.x + 5, c.y + 5},
                                         td::ColorID::Gray, 1.8f);
                    gui::Shape::drawLine({c.x - 5, c.y + 5}, {c.x + 5, c.y - 5},
                                         td::ColorID::Gray, 1.8f);
                }
            }
        }

        // trajectories (revealed progressively while the animation runs)
        if (_runs)
        {
            double elapsedMs = 0.0;
            if (_revealing)
            {
                elapsedMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - _revealT0).count();
                bool allDone = true;
                for (double d : _revealDurMs)
                    if (elapsedMs < d) { allDone = false; break; }
                if (allDone)
                {
                    _revealing = false;
                    _stopPending = true; // stop on the next frame, not mid-draw
                }
            }

            for (size_t ri = 0; ri < _runs->size(); ++ri)
            {
                const opt::RunResult& r = (*_runs)[ri];
                const auto& tr_ = r.trace;
                if (tr_.empty())
                    continue;
                const td::ColorID col = methodColor(r.method);

                size_t nVis = tr_.size();
                bool done = true;
                if (_revealing && ri < _revealDurMs.size())
                {
                    const double frac =
                        std::min(1.0, elapsedMs / _revealDurMs[ri]);
                    nVis = 1 + (size_t)(frac * (double)(tr_.size() - 1));
                    done = (frac >= 1.0);
                }

                std::vector<gui::Point> pts;
                pts.reserve(nVis);
                for (size_t k = 0; k < nVis; ++k)
                    pts.push_back({px(tr_[k].x0), py(tr_[k].x1)});

                if (pts.size() > 1)
                {
                    gui::Shape line;
                    line.createPolyLine(pts.data(), pts.size(), 2.4f);
                    line.drawWire(col);
                }
                if (tr_.size() <= 80) // decision on the full trace: stable look
                {
                    for (const gui::Point& p : pts)
                    {
                        gui::Shape dot;
                        dot.createCircle(gui::Circle(p, 2.6), 1.0f);
                        dot.drawFill(col);
                    }
                }
                if (done)
                {
                    // ring on the final iterate
                    gui::Shape ring;
                    ring.createCircle(gui::Circle(pts.back(), 5.0), 2.0f);
                    ring.drawWire(col);
                }
                else
                {
                    // the "moving point": this method's current iterate
                    gui::Shape cur;
                    cur.createCircle(gui::Circle(pts.back(), 4.0), 1.2f);
                    cur.drawFill(col);
                }
            }
        }

        // starting point marker
        if (_hasStart)
        {
            const gui::Point c(px(_x0), py(_y0));
            gui::Shape mark;
            mark.createCircle(gui::Circle(c, 4.6), 1.4f);
            mark.drawFillAndWire(td::ColorID::ForestGreen, td::ColorID::SysText);
            gui::DrawableString::draw("x0", gui::Point(c.x + 7, c.y - 7),
                                      gui::Font::ID::SystemSmaller,
                                      td::ColorID::ForestGreen);
        }

        // title (function name) + per-run legend
        double ly = 6.0;
        if (_fn)
        {
            gui::DrawableString::draw(_fn->name(), gui::Point(10, ly),
                                      gui::Font::ID::SystemBold, td::ColorID::SysText);
            ly += 22.0;
        }
        if (_runs)
        {
            td::String lbl;
            for (const opt::RunResult& r : *_runs)
            {
                const td::ColorID col = methodColor(r.method);
                gui::Shape::drawLine({10.0, ly + 7.0}, {34.0, ly + 7.0}, col, 3.0f);
                if (r.newtonFallbacks > 0)
                    lbl.format("%s: %d it (%s, %d fb)", opt::toString(r.method),
                               r.iterations, opt::toString(r.status),
                               r.newtonFallbacks);
                else
                    lbl.format("%s: %d it (%s)", opt::toString(r.method),
                               r.iterations, opt::toString(r.status));
                gui::DrawableString::draw(lbl, gui::Point(40, ly),
                                          gui::Font::ID::SystemSmaller,
                                          td::ColorID::SysText);
                ly += 17.0;
            }
        }

    }
};
