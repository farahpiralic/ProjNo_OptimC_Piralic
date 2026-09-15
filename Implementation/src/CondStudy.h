//  OptimCompare - Project #16 (F. Piralić, 20106)
//
//  CondStudy.h
//  The proposal's third plot: "Condition number vs. iterations - generated
//  by running each method on quadratics with increasing condition numbers."
//
//  For kappa on a logarithmic grid (10^0 ... 10^4) the study minimizes
//      f(x) = 0.5 x^T R diag(1, kappa) R^T x
//  from the eigen-balanced worst-case start (see Quadratic2D::suggestedStart),
//  which is what makes the classical rates visible
//  with each method and the selected step strategy, and records the number
//  of iterations needed to reach ||g|| <= eps.
//
//  Theory the plot should reproduce:
//    * steepest descent with exact line search contracts by
//      ((kappa-1)/(kappa+1))^2 per step  =>  iterations grow ~ O(kappa),
//    * Newton solves a quadratic in one step regardless of kappa,
//    * BFGS with exact line search terminates on an n-dimensional quadratic
//      in at most n steps (here n = 2) - essentially flat as well.
#pragma once

#include "Optimizer.h"
#include <vector>

namespace opt
{

struct CondStudyPoint
{
    double kappa = 1.0;
    int    itersSD     = 0;
    int    itersNewton = 0;
    int    itersBFGS   = 0;
    RunStatus stSD     = RunStatus::MaxIterations;
    RunStatus stNewton = RunStatus::MaxIterations;
    RunStatus stBFGS   = RunStatus::MaxIterations;
};

struct CondStudyResult
{
    StepStrategy strategy = StepStrategy::ExactLineSearch;
    double eps = 1e-6;
    int maxIter = 2000000;
    std::vector<CondStudyPoint> points;
};

class CondStudy
{
public:
    static CondStudyResult run(StepStrategy strategy,
                               double eps = 1e-6,
                               int pointsPerDecade = 2,
                               double kappaMax = 1e4,
                               int maxIter = 200000)
    {
        CondStudyResult res;
        res.strategy = strategy;
        res.eps = eps;
        res.maxIter = maxIter;

        RunOptions opts;
        opts.x0.assign(2, 0.0);
        opts.eps = eps;
        opts.maxIter = maxIter;
        opts.recordTrace = false; // only iteration counts are needed

        const double step = std::pow(10.0, 1.0 / pointsPerDecade);
        for (double kappa = 1.0; kappa <= kappaMax * 1.0000001; kappa *= step)
        {
            Quadratic2D q(kappa, 30.0 * M_PI / 180.0, "study"); // rotated:
            // (1,1) is generic in the eigenbasis, so SD shows the true
            // ((kappa-1)/(kappa+1))^2 zigzag rate instead of the lucky
            // axis-aligned special case.
            q.suggestedStart(opts.x0.data()); // eigen-balanced worst case
            CondStudyPoint p;
            p.kappa = kappa;

            RunResult r1 = Optimizer::run(Method::SteepestDescent, strategy, q, opts);
            p.itersSD = r1.iterations;
            p.stSD    = r1.status;

            RunResult r2 = Optimizer::run(Method::Newton, strategy, q, opts);
            p.itersNewton = r2.iterations;
            p.stNewton    = r2.status;

            RunResult r3 = Optimizer::run(Method::BFGS, strategy, q, opts);
            p.itersBFGS = r3.iterations;
            p.stBFGS    = r3.status;

            res.points.push_back(p);
        }
        return res;
    }
};

} // namespace opt
