//  ChartCanvas.h
//  Multi-series line chart on a plain gui::Canvas with optional logarithmic
//  axes. Two instances drive the proposal's plot panels:
//
//    * convergence curve:  ||grad f(x_k)|| vs iteration, log-scale y,
//      one line per method;
//    * condition study:    iterations vs condition number kappa,
//      log-scale on both axes (SD appears as a straight line of slope ~1,
//      Newton and BFGS as flat lines).
//
#pragma once

#include <gui/Canvas.h>
#include <gui/Shape.h>
#include <gui/DrawableString.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <chrono>

class ChartCanvas : public gui::Canvas
{
public:
    struct Series
    {
        std::vector<double> x, y;
        td::ColorID color = td::ColorID::Crimson;
        td::String  name;
    };

private:
    std::vector<Series> _series;
    td::String _title, _xName, _yName;
    bool _logX = false, _logY = false;
    gui::Size _size {600, 260};

    static constexpr double kLogFloor = 1e-16;

    // step-by-step reveal of the series (Canvas animation frames)
    // pacing: per-step time and total-duration clamp (tune to taste)
    static constexpr double kRevealMsPerStep = 240.0;
    static constexpr double kRevealMinMs     = 1500.0;
    static constexpr double kRevealMaxMs     = 8000.0;
    bool _revealing = false;
    bool _stopPending = false; // see ContourCanvas: never stop mid-draw
    std::chrono::steady_clock::time_point _revealT0;
    std::vector<double> _revealDurMs; // one duration per series

public:
    ChartCanvas() : gui::Canvas({})
    {
        enableResizeEvent(true);
        setPreferredFrameRateRange(30, 60); // used by the step-reveal animation
    }

    void setLabels(const td::String& title, const td::String& xName,
                   const td::String& yName)
    {
        _title = title; _xName = xName; _yName = yName;
    }

    void setLogX(bool b) { _logX = b; }
    void setLogY(bool b) { _logY = b; }

    void clearSeries()
    {
        _series.clear();
        cancelReveal();
        reDraw();
    }

    //  Step-by-step reveal, same per-series duration rule as the contour
    //  canvas so both panels stay visually in sync.
    void beginReveal()
    {
        if (_series.empty())
            return;
        _revealDurMs.clear();
        for (const Series& s : _series)
        {
            const double n = (double)std::min(s.x.size(), s.y.size());
            _revealDurMs.push_back(std::min(kRevealMaxMs,
                std::max(kRevealMinMs, n * kRevealMsPerStep)));
        }
        _revealT0 = std::chrono::steady_clock::now();
        _revealing = true;
        _stopPending = false;
        startAnimation();
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

    void addSeries(const std::vector<double>& x, const std::vector<double>& y,
                   td::ColorID color, const td::String& name)
    {
        Series s;
        s.x = x; s.y = y; s.color = color; s.name = name;
        _series.push_back(std::move(s));
        reDraw();
    }

protected:
    void onResize(const gui::Size& newSize) override { _size = newSize; }

    static double niceStep(double rough)
    {
        if (rough <= 0.0 || !std::isfinite(rough)) return 1.0;
        const double p = std::pow(10.0, std::floor(std::log10(rough)));
        const double r = rough / p;
        if (r < 1.5) return p;
        if (r < 3.5) return 2.0 * p;
        if (r < 7.5) return 5.0 * p;
        return 10.0 * p;
    }

    double tx(double v) const { return _logX ? std::log10(std::max(v, kLogFloor)) : v; }
    double ty(double v) const { return _logY ? std::log10(std::max(v, kLogFloor)) : v; }

    void onDraw(const gui::Rect& /*rect*/) override
    {
        if (_stopPending && !_revealing)
        {
            _stopPending = false;
            stopAnimation();
            reDraw();
        }

        const double L = 62, R = 14, T = 26, B = 40;
        const double w = (double)_size.width, h = (double)_size.height;
        const double plotW = std::max(10.0, w - L - R);
        const double plotH = std::max(10.0, h - T - B);

        gui::DrawableString::draw(_title, gui::Point(L, 5),
                                  gui::Font::ID::SystemBold, td::ColorID::SysText);

        const gui::Point fr[4] = { {L, T}, {L + plotW, T},
                                   {L + plotW, T + plotH}, {L, T + plotH} };
        gui::Shape frame;
        frame.createPolygon(fr, 4, 1.0f);
        frame.drawWire(td::ColorID::Gray);

        //data ranges (in transformed coordinates) 
        bool any = false;
        double xMin = 0, xMax = 1, yMin = 0, yMax = 1;
        for (const Series& s : _series)
            for (size_t i = 0; i < s.x.size() && i < s.y.size(); ++i)
            {
                const double X = tx(s.x[i]), Y = ty(s.y[i]);
                if (!std::isfinite(X) || !std::isfinite(Y)) continue;
                if (!any) { xMin = xMax = X; yMin = yMax = Y; any = true; }
                xMin = std::min(xMin, X); xMax = std::max(xMax, X);
                yMin = std::min(yMin, Y); yMax = std::max(yMax, Y);
            }
        if (!any)
        {
            gui::DrawableString::draw(tr("chartNoData"),
                                      gui::Point(L + 12, T + 12),
                                      gui::Font::ID::SystemNormal, td::ColorID::Gray);
            return;
        }
        if (!_logY) yMin = std::min(0.0, yMin);
        if (xMax - xMin < 1e-12) xMax = xMin + 1.0;
        if (yMax - yMin < 1e-12) yMax = yMin + 1.0;
        yMax += (yMax - yMin) * 0.06;

        auto PX = [&](double v) { return L + (tx(v) - xMin) / (xMax - xMin) * plotW; };
        auto PY = [&](double v) { return T + plotH - (ty(v) - yMin) / (yMax - yMin) * plotH; };

        td::String lbl;

        //ticks + grid
        if (_logX)
        {
            for (int e = (int)std::ceil(xMin); e <= (int)std::floor(xMax); ++e)
            {
                const double px = L + (e - xMin) / (xMax - xMin) * plotW;
                gui::Shape::drawLine({px, T}, {px, T + plotH},
                                     td::ColorID::LightGray, 0.6f, td::LinePattern::Dot);
                lbl.format("1e%d", e);
                gui::DrawableString::draw(lbl, gui::Point(px - 12, T + plotH + 6),
                                          gui::Font::ID::SystemSmaller, td::ColorID::SysText);
            }
        }
        else
        {
            const double sx = niceStep((xMax - xMin) / 6.0);
            for (double v = std::ceil(xMin / sx) * sx; v <= xMax + 1e-9; v += sx)
            {
                const double px = L + (v - xMin) / (xMax - xMin) * plotW;
                gui::Shape::drawLine({px, T}, {px, T + plotH},
                                     td::ColorID::LightGray, 0.6f, td::LinePattern::Dot);
                lbl.format("%g", v);
                gui::DrawableString::draw(lbl, gui::Point(px - 10, T + plotH + 6),
                                          gui::Font::ID::SystemSmaller, td::ColorID::SysText);
            }
        }

        if (_logY)
        {
            int e0 = (int)std::ceil(yMin), e1 = (int)std::floor(yMax);
            int step = std::max(1, (e1 - e0) / 6);
            for (int e = e0; e <= e1; e += step)
            {
                const double py = T + plotH - (e - yMin) / (yMax - yMin) * plotH;
                gui::Shape::drawLine({L, py}, {L + plotW, py},
                                     td::ColorID::LightGray, 0.6f, td::LinePattern::Dot);
                lbl.format("1e%d", e);
                gui::DrawableString::draw(lbl, gui::Point(10, py - 7),
                                          gui::Font::ID::SystemSmaller, td::ColorID::SysText);
            }
        }
        else
        {
            const double sy = niceStep((yMax - yMin) / 5.0);
            for (double v = std::ceil(yMin / sy) * sy; v <= yMax + 1e-9; v += sy)
            {
                const double py = T + plotH - (v - yMin) / (yMax - yMin) * plotH;
                gui::Shape::drawLine({L, py}, {L + plotW, py},
                                     td::ColorID::LightGray, 0.6f, td::LinePattern::Dot);
                lbl.format("%g", v);
                gui::DrawableString::draw(lbl, gui::Point(10, py - 7),
                                          gui::Font::ID::SystemSmaller, td::ColorID::SysText);
            }
        }

        gui::DrawableString::draw(_xName, gui::Point(L + plotW * 0.5 - 14, h - 18),
                                  gui::Font::ID::SystemSmaller, td::ColorID::SysText);
        gui::DrawableString::draw(_yName, gui::Point(6, T - 16),
                                  gui::Font::ID::SystemSmaller, td::ColorID::SysText);

        // ---- series (revealed progressively while the animation runs) -------
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

        for (size_t si = 0; si < _series.size(); ++si)
        {
            const Series& s = _series[si];
            const size_t nPts = std::min(s.x.size(), s.y.size());
            if (nPts == 0) continue;

            size_t nVis = nPts;
            bool done = true;
            if (_revealing && si < _revealDurMs.size())
            {
                const double frac = std::min(1.0, elapsedMs / _revealDurMs[si]);
                nVis = 1 + (size_t)(frac * (double)(nPts - 1));
                done = (frac >= 1.0);
            }

            std::vector<gui::Point> pts;
            pts.reserve(nVis);
            for (size_t i = 0; i < nVis; ++i)
                pts.push_back({PX(s.x[i]), PY(s.y[i])});

            if (pts.size() > 1)
            {
                gui::Shape line;
                line.createPolyLine(pts.data(), pts.size(), 2.2f);
                line.drawWire(s.color);
            }
            if (nPts <= 80) // markers only when readable (full-size decision)
            {
                for (const gui::Point& p : pts)
                {
                    gui::Shape dot;
                    dot.createCircle(gui::Circle(p, 2.6), 1.0f);
                    dot.drawFill(s.color);
                }
            }
            if (!done)
            {
                // the "pen tip": current end of this curve
                gui::Shape cur;
                cur.createCircle(gui::Circle(pts.back(), 3.4), 1.0f);
                cur.drawFill(s.color);
            }
        }

        // ---- legend (top-right) ---------------------------------------------
        double ly = T + 8;
        for (const Series& s : _series)
        {
            const double lx = L + plotW - 170;
            gui::Shape::drawLine({lx, ly + 6}, {lx + 24, ly + 6}, s.color, 3.0f);
            gui::DrawableString::draw(s.name, gui::Point(lx + 30, ly - 1),
                                      gui::Font::ID::SystemSmaller, td::ColorID::SysText);
            ly += 17;
        }
    }
};
