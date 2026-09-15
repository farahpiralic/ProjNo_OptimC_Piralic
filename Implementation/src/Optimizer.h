//  OptimCompare - Project #16 (F. Piralić, 20106)
//
//  Optimizer.h
//  The three methods of the comparative study, sharing one driver:
//
//      x_{k+1} = x_k + alpha_k d_k
//
//    * Steepest descent :  d = -g
//    * Newton           :  solve  H d = -g  with natID's dense::Matrix
//                          (descent safeguard: if H is not positive definite
//                          at x_k - solve fails or d is not a descent
//                          direction - the step falls back to -g and the
//                          iterate is flagged, so saddle behavior is visible
//                          in the log and on the contour plot)
//    * BFGS             :  d = -B g with the inverse-Hessian approximation
//                          B updated by the standard BFGS formula
//                              B+ = (I - r s y^T) B (I - r y s^T) + r s s^T,
//                              r = 1/(y^T s),
//                          skipping updates that violate the curvature
//                          condition y^T s > 0.
//
//  alpha_k comes from LineSearch.h (exact or Armijo - the proposal's two
//  strategies). Every iterate is recorded (x, f, ||g||, alpha) to drive the
//  trajectory overlay and the log-scale convergence curves.
#pragma once

#include "Objective.h"
#include "LineSearch.h"
#include <vector>
#include <string>
#include <chrono>
#include <cmath>

namespace opt
{

enum class Method : unsigned char { SteepestDescent = 0, Newton, BFGS };

inline const char* toString(Method m)
{
    switch (m)
    {
        case Method::SteepestDescent: return "steepest descent";
        case Method::Newton:          return "Newton";
        case Method::BFGS:            return "BFGS";
    }
    return "?";
}

enum class RunStatus : unsigned char
{
    Converged = 0,   // ||g|| <= eps
    MaxIterations,
    Stalled,         // line search cannot make progress
    Diverged         // |x| or f exploded (e.g. pure saddle directions)
};

inline const char* toString(RunStatus s)
{
    switch (s)
    {
        case RunStatus::Converged:     return "Converged";
        case RunStatus::MaxIterations: return "MaxIterations";
        case RunStatus::Stalled:       return "Stalled";
        case RunStatus::Diverged:      return "Diverged";
    }
    return "?";
}

struct Iterate
{
    double x0 = 0.0, x1 = 0.0;  // first two coordinates (all suite fns are 2D)
    double f  = 0.0;
    double gNorm = 0.0;
    double alpha = 0.0;
    bool   newtonFallback = false; // Newton step replaced by -g this iteration
};

struct RunOptions
{
    std::vector<double> x0;
    double eps      = 1e-6;    // stop when ||g|| <= eps
    int    maxIter  = 500;
    double divergeAt = 1e8;    // |x|_inf or |f| beyond this => Diverged
    bool   recordTrace = true; // condition study switches this off
};

struct RunResult
{
    Method       method   = Method::SteepestDescent;
    StepStrategy strategy = StepStrategy::Armijo;
    RunStatus    status   = RunStatus::MaxIterations;

    std::vector<Iterate> trace;   // trace[0] is the starting point
    int    iterations = 0;        // number of steps taken (= trace.size()-1)
    int    fEvals     = 0;        // objective evaluations (line searches)
    int    gEvals     = 0;
    int    newtonFallbacks = 0;
    double finalF     = 0.0;
    double finalGNorm = 0.0;
    double ms         = 0.0;
};

class Optimizer
{
public:
    static RunResult run(Method method, StepStrategy strategy,
                         const IObjective& fn, const RunOptions& opts)
    {
        const auto t0 = std::chrono::high_resolution_clock::now();

        const int n = fn.dim();
        RunResult res;
        res.method   = method;
        res.strategy = strategy;

        std::vector<double> x = opts.x0;
        x.resize((size_t)n, 0.0);
        std::vector<double> g(n), d(n), xNew(n), gNew(n), s(n), y(n);

        DblMatrix H((td::UINT4)n, (td::UINT4)n, nullptr, true);   // Hessian
        DblMatrix B((td::UINT4)n, (td::UINT4)n, nullptr, true);   // BFGS inv approx
        DblMatrix rhs((td::UINT4)n, (td::UINT4)1, nullptr, true); // Newton RHS

        if (method == Method::BFGS)
            setIdentity(B);

        double fx = fn.f(x.data());  ++res.fEvals;
        fn.grad(x.data(), g.data()); ++res.gEvals;
        double gn = norm2(g);

        if (opts.recordTrace)
            record(res, x, fx, gn, 0.0, false);

        for (int k = 0; k < opts.maxIter; ++k)
        {
            if (gn <= opts.eps)
            {
                res.status = RunStatus::Converged;
                break;
            }
            if (!std::isfinite(fx) || std::fabs(fx) > opts.divergeAt ||
                maxAbs(x) > opts.divergeAt)
            {
                res.status = RunStatus::Diverged;
                break;
            }

            // ---- direction --------------------------------------------------
            bool fallback = false;
            switch (method)
            {
                case Method::SteepestDescent:
                    for (int i = 0; i < n; ++i)
                        d[i] = -g[i];
                    break;

                case Method::Newton:
                {
                    fn.hess(x.data(), H);
                    auto b = rhs.getManipulator();
                    for (int i = 0; i < n; ++i)
                        b(i, 0) = -g[i];

                    DblMatrix Hcopy(H); // solve() overwrites the matrix
                    const bool ok = Hcopy.solve(rhs);
                    if (ok)
                    {
                        auto xr = rhs.getManipulator();
                        for (int i = 0; i < n; ++i)
                            d[i] = xr(i, 0);
                    }
                    if (!ok || dot(g, d) >= -1e-14 * gn * norm2(d))
                    {
                        // singular / indefinite Hessian: not a descent
                        // direction - safeguarded fall back to -g.
                        for (int i = 0; i < n; ++i)
                            d[i] = -g[i];
                        fallback = true;
                        ++res.newtonFallbacks;
                    }
                    break;
                }

                case Method::BFGS:
                {
                    auto b = B.getManipulator();
                    for (int i = 0; i < n; ++i)
                    {
                        double di = 0.0;
                        for (int j = 0; j < n; ++j)
                            di -= b(i, j) * g[j];
                        d[i] = di;
                    }
                    if (dot(g, d) >= 0.0)
                    {
                        // numerical loss of positive definiteness: reset B
                        setIdentity(B);
                        for (int i = 0; i < n; ++i)
                            d[i] = -g[i];
                    }
                    break;
                }
            }

            // ---- step length ------------------------------------------------
            const double gDotD = dot(g, d);
            LineSearchResult ls = LineSearch::run(strategy, fn, x, d, fx,
                                                  gDotD, &H);
            res.fEvals += ls.fEvals;
            if (!ls.ok || ls.alpha <= 0.0)
            {
                res.status = RunStatus::Stalled;
                break;
            }

            // ---- update -----------------------------------------------------
            for (int i = 0; i < n; ++i)
                xNew[i] = x[i] + ls.alpha * d[i];
            const double fNew = fn.f(xNew.data()); ++res.fEvals;
            fn.grad(xNew.data(), gNew.data());     ++res.gEvals;

            if (method == Method::BFGS)
            {
                for (int i = 0; i < n; ++i)
                {
                    s[i] = xNew[i] - x[i];
                    y[i] = gNew[i] - g[i];
                }
                bfgsUpdate(B, s, y);
            }

            x.swap(xNew);
            g.swap(gNew);
            fx = fNew;
            gn = norm2(g);
            ++res.iterations;

            if (opts.recordTrace)
                record(res, x, fx, gn, ls.alpha, fallback);
        }

        if (res.status == RunStatus::MaxIterations && gn <= opts.eps)
            res.status = RunStatus::Converged; // converged on the last step

        res.finalF = fx;
        res.finalGNorm = gn;

        const auto t1 = std::chrono::high_resolution_clock::now();
        res.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return res;
    }

private:
    static double dot(const std::vector<double>& a, const std::vector<double>& b)
    {
        double s = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            s += a[i] * b[i];
        return s;
    }

    static double norm2(const std::vector<double>& v)
    {
        return std::sqrt(dot(v, v));
    }

    static double maxAbs(const std::vector<double>& v)
    {
        double m = 0.0;
        for (double t : v)
            m = std::max(m, std::fabs(t));
        return m;
    }

    static void setIdentity(DblMatrix& M)
    {
        auto m = M.getManipulator();
        const int n = (int)M.getNoOfRows();
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                m(i, j) = (i == j) ? 1.0 : 0.0;
    }

    //  B+ = (I - r s y^T) B (I - r y s^T) + r s s^T,  r = 1/(y^T s)
    static void bfgsUpdate(DblMatrix& B, const std::vector<double>& s,
                           const std::vector<double>& y)
    {
        const int n = (int)s.size();
        double ys = 0.0;
        for (int i = 0; i < n; ++i)
            ys += y[i] * s[i];
        if (ys <= 1e-12)
            return; // curvature condition violated: skip the update

        const double r = 1.0 / ys;
        auto b = B.getManipulator();

        // t = B y
        std::vector<double> t(n, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                t[i] += b(i, j) * y[j];

        const double yBy = [&]{
            double v = 0.0;
            for (int i = 0; i < n; ++i)
                v += y[i] * t[i];
            return v;
        }();

        // expanded form:
        // B+ = B - r (s t^T + t s^T) + r^2 (y^T B y) s s^T + r s s^T
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                b(i, j) += -r * (s[i] * t[j] + t[i] * s[j])
                           + (r * r * yBy + r) * s[i] * s[j];
    }

    static void record(RunResult& res, const std::vector<double>& x,
                       double f, double gNorm, double alpha, bool fb)
    {
        Iterate it;
        it.x0 = x.size() > 0 ? x[0] : 0.0;
        it.x1 = x.size() > 1 ? x[1] : 0.0;
        it.f = f;
        it.gNorm = gNorm;
        it.alpha = alpha;
        it.newtonFallback = fb;
        res.trace.push_back(it);
    }
};

} // namespace opt
