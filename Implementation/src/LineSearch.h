//  OptimCompare - Project #16 (F. Piralić, 20106)
//
//  LineSearch.h
//  The two step-size strategies from the proposal, applied identically to
//  all three methods:
//
//    * Exact line search:  alpha* = argmin_{a>0} phi(a),  phi(a)=f(x + a d).
//      For quadratics the closed form  alpha* = -(g^T d)/(d^T H d)  is used;
//      for general smooth f the minimizer is found numerically (expanding
//      bracket + golden-section refinement), i.e. "exact" to tolerance.
//
//    * Armijo backtracking:  start from alpha0 = 1, shrink by rho = 1/2
//      until the sufficient-decrease condition
//          f(x + a d) <= f(x) + c1 * a * g^T d      (c1 = 1e-4)
//      holds. This is the standard inexact strategy from Nocedal & Wright.
#pragma once

#include "Objective.h"
#include <vector>
#include <cmath>

namespace opt
{

enum class StepStrategy : unsigned char { ExactLineSearch = 0, Armijo };

inline const char* toString(StepStrategy s)
{
    return (s == StepStrategy::ExactLineSearch) ? "exact line search"
                                                : "Armijo backtracking";
}

struct LineSearchResult
{
    double alpha   = 0.0;
    int    fEvals  = 0;   // number of f evaluations spent in the search
    bool   ok      = false;
};

class LineSearch
{
public:
    // gDotD = g(x)^T d (must be < 0 for a descent direction).
    static LineSearchResult run(StepStrategy strategy, const IObjective& fn,
                                const std::vector<double>& x,
                                const std::vector<double>& d,
                                double fx, double gDotD,
                                DblMatrix* HforQuad = nullptr)
    {
        if (strategy == StepStrategy::Armijo)
            return armijo(fn, x, d, fx, gDotD);
        return exact(fn, x, d, fx, gDotD, HforQuad);
    }

private:
    static double phi(const IObjective& fn, const std::vector<double>& x,
                      const std::vector<double>& d, double a,
                      std::vector<double>& scratch)
    {
        const int n = (int)x.size();
        for (int i = 0; i < n; ++i)
            scratch[i] = x[i] + a * d[i];
        return fn.f(scratch.data());
    }

    // ---------------------------------------------------------- Armijo
    static LineSearchResult armijo(const IObjective& fn,
                                   const std::vector<double>& x,
                                   const std::vector<double>& d,
                                   double fx, double gDotD)
    {
        LineSearchResult r;
        const double c1  = 1e-4;
        const double rho = 0.5;
        double a = 1.0;
        std::vector<double> s(x.size());

        for (int it = 0; it < 60; ++it)
        {
            const double fa = phi(fn, x, d, a, s);
            ++r.fEvals;
            if (fa <= fx + c1 * a * gDotD)
            {
                r.alpha = a;
                r.ok = true;
                return r;
            }
            a *= rho;
        }
        r.alpha = a;      // ~1e-18: effectively no progress possible
        r.ok = false;
        return r;
    }

    // ------------------------------------------------------------ exact
    static LineSearchResult exact(const IObjective& fn,
                                  const std::vector<double>& x,
                                  const std::vector<double>& d,
                                  double fx, double gDotD,
                                  DblMatrix* HforQuad)
    {
        LineSearchResult r;

        // Closed form on quadratics: alpha = -(g^T d) / (d^T H d).
        if (fn.isQuadratic() && HforQuad)
        {
            fn.hess(x.data(), *HforQuad);
            auto h = HforQuad->getManipulator();
            const int n = (int)x.size();
            double dHd = 0.0;
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    dHd += d[i] * h(i, j) * d[j];
            if (dHd > 1e-300)
            {
                r.alpha = -gDotD / dHd;
                r.ok = (r.alpha > 0.0);
                return r;
            }
            // fall through to numeric search when curvature is not positive
        }

        // Numeric 1D minimization: expand a bracket [0, b] until phi grows,
        // then golden-section on [lo, hi].
        std::vector<double> s(x.size());
        double a0 = 0.0, f0 = fx;
        double a1 = 1.0, f1 = phi(fn, x, d, a1, s); ++r.fEvals;

        // shrink first if even a=1 overshoots badly
        int guard = 0;
        while (f1 > f0 && a1 > 1e-14 && guard++ < 60)
        {
            a1 *= 0.5;
            f1 = phi(fn, x, d, a1, s); ++r.fEvals;
        }
        if (f1 > f0) { r.alpha = 0.0; r.ok = false; return r; }

        // expand while descending
        double a2 = 2.0 * a1, f2 = phi(fn, x, d, a2, s); ++r.fEvals;
        guard = 0;
        while (f2 < f1 && a2 < 1e12 && guard++ < 90)
        {
            a0 = a1; f0 = f1;
            a1 = a2; f1 = f2;
            a2 *= 2.0;
            f2 = phi(fn, x, d, a2, s); ++r.fEvals;
        }
        // minimum bracketed in [a0, a2]
        double lo = a0, hi = a2;
        const double gr = 0.5 * (std::sqrt(5.0) - 1.0); // 0.618...
        double c = hi - gr * (hi - lo);
        double e = lo + gr * (hi - lo);
        double fc = phi(fn, x, d, c, s); ++r.fEvals;
        double fe = phi(fn, x, d, e, s); ++r.fEvals;
        for (int it = 0; it < 90 && (hi - lo) > 1e-12 * (1.0 + hi); ++it)
        {
            if (fc < fe)
            {
                hi = e; e = c; fe = fc;
                c = hi - gr * (hi - lo);
                fc = phi(fn, x, d, c, s); ++r.fEvals;
            }
            else
            {
                lo = c; c = e; fc = fe;
                e = lo + gr * (hi - lo);
                fe = phi(fn, x, d, e, s); ++r.fEvals;
            }
        }
        r.alpha = 0.5 * (lo + hi);
        r.ok = (r.alpha > 0.0);
        return r;
    }
};

} // namespace opt
