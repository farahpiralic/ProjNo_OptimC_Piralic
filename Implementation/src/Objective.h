//  OptimCompare - Project #16: First- vs Second-Order Unconstrained
//  Optimization Methods (steepest descent / Newton / BFGS)
//  Student: F. Piralić, index 20106
//
//  Objective.h
//  Interface for a smooth objective f : R^n -> R with analytic gradient and
//  Hessian, plus the benchmark suite required by the proposal:
//
//    A) well-conditioned strongly convex functions,
//    B) ill-conditioned quadratics (axis-aligned and rotated, plus a
//       kappa-parametric family used by the condition-number study),
//    C) non-convex landscapes with saddle points (double well, Rosenbrock,
//       Himmelblau).
//
//  The Hessian is returned through natID's dense::Matrix, which is also what
//  Newton's method factorizes (dense::Matrix::solve) - the "second-order
//  information" of the project title flows through the framework's dense
//  linear algebra, exactly as the proposal states.
//
//  All benchmark functions are two-dimensional so every experiment can be
//  rendered on the contour canvas; the optimizer itself (Optimizer.h) is
//  written for general n.
#pragma once

#include <dense/Matrix.h>
#include <vector>
#include <string>
#include <cmath>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace opt
{

typedef dense::Matrix<double> DblMatrix;

// ---------------------------------------------------------------- interface
class IObjective
{
public:
    virtual ~IObjective() = default;

    virtual int  dim() const = 0;
    virtual const char* name() const = 0;

    virtual double f(const double* x) const = 0;
    virtual void   grad(const double* x, double* g) const = 0;
    virtual void   hess(const double* x, DblMatrix& H) const = 0; // dim x dim

    // Quadratics report themselves so the exact line search can use the
    // closed form  alpha* = -(g^T d)/(d^T H d).
    virtual bool isQuadratic() const { return false; }

    // Defaults for the GUI: starting point, framing of the contour view,
    // and the known stationary points (drawn as markers).
    virtual void suggestedStart(double* x0) const
    {
        for (int i = 0; i < dim(); ++i)
            x0[i] = 1.0;
    }
    virtual void suggestedView(double& xMin, double& xMax,
                               double& yMin, double& yMax) const
    {
        xMin = -3; xMax = 3; yMin = -3; yMax = 3;
    }
    struct Stationary { double x, y; bool minimum; }; // minimum or saddle
    virtual std::vector<Stationary> stationaryPoints() const { return {}; }
};

// ------------------------------------------------------ quadratic (general)
//  f(x) = 0.5 x^T A x,   A symmetric positive definite (2x2 here).
//  Built from eigenvalues (1, kappa) and a rotation angle, so both the
//  axis-aligned and the rotated ill-conditioned quadratics - and the whole
//  kappa-family of the condition study - are one class.
class Quadratic2D : public IObjective
{
    double _a11, _a12, _a22; // A entries
    double _kappa;
    double _c, _s;           // rotation (eigenvectors v1=(c,s), v2=(-s,c))
    std::string _name;
    double _view;

public:
    Quadratic2D(double kappa, double angleRad, const char* displayName,
                double viewHalfSpan = 3.0)
    : _kappa(kappa)
    , _c(std::cos(angleRad))
    , _s(std::sin(angleRad))
    , _name(displayName)
    , _view(viewHalfSpan)
    {
        const double c = _c, s = _s;
        // A = R diag(1, kappa) R^T
        _a11 = c * c + _kappa * s * s;
        _a22 = s * s + _kappa * c * c;
        _a12 = (1.0 - _kappa) * c * s;
    }

    double kappa() const { return _kappa; }

    int dim() const override { return 2; }
    const char* name() const override { return _name.c_str(); }
    bool isQuadratic() const override { return true; }

    double f(const double* x) const override
    {
        return 0.5 * (_a11 * x[0] * x[0] + 2.0 * _a12 * x[0] * x[1]
                      + _a22 * x[1] * x[1]);
    }

    void grad(const double* x, double* g) const override
    {
        g[0] = _a11 * x[0] + _a12 * x[1];
        g[1] = _a12 * x[0] + _a22 * x[1];
    }

    void hess(const double* /*x*/, DblMatrix& H) const override
    {
        auto h = H.getManipulator();
        h(0, 0) = _a11; h(0, 1) = _a12;
        h(1, 0) = _a12; h(1, 1) = _a22;
    }

    //  Worst-case start for steepest descent: eigencoordinates (a, a/kappa),
    //  which balance the gradient components (1*a = kappa*(a/kappa)) and make
    //  exact-line-search SD contract by the full ((k-1)/(k+1))^2 factor per
    //  step - the classic zigzag. Generic starts on quadratics can converge
    //  deceptively fast (the slow mode is barely excited), so both the GUI
    //  presets and the condition study use this initialization.
    void suggestedStart(double* x0) const override
    {
        const double a = 2.0, b = 2.0 / _kappa;
        x0[0] = a * _c - b * _s;
        x0[1] = a * _s + b * _c;
    }

    void suggestedView(double& xMin, double& xMax,
                       double& yMin, double& yMax) const override
    {
        xMin = -_view; xMax = _view; yMin = -_view; yMax = _view;
    }

    std::vector<Stationary> stationaryPoints() const override
    {
        return { {0.0, 0.0, true} };
    }
};

// ------------------------------------- strongly convex, non-quadratic
//  f(x,y) = x^2 + y^2 + exp(0.1 (x + y))
//  Hessian = 2 I + 0.01 e^{0.1(x+y)} * [1 1; 1 1]  >=  2 I  (strongly convex)
class StronglyConvexExp : public IObjective
{
public:
    int dim() const override { return 2; }
    const char* name() const override { return "Strongly convex (quad + exp)"; }

    double f(const double* x) const override
    {
        return x[0] * x[0] + x[1] * x[1] + std::exp(0.1 * (x[0] + x[1]));
    }

    void grad(const double* x, double* g) const override
    {
        const double e = std::exp(0.1 * (x[0] + x[1]));
        g[0] = 2.0 * x[0] + 0.1 * e;
        g[1] = 2.0 * x[1] + 0.1 * e;
    }

    void hess(const double* x, DblMatrix& H) const override
    {
        const double e = 0.01 * std::exp(0.1 * (x[0] + x[1]));
        auto h = H.getManipulator();
        h(0, 0) = 2.0 + e; h(0, 1) = e;
        h(1, 0) = e;       h(1, 1) = 2.0 + e;
    }

    void suggestedStart(double* x0) const override { x0[0] = 2.0; x0[1] = -1.5; }

    std::vector<Stationary> stationaryPoints() const override
    {
        // grad = 0 => x = y and 2x + 0.1 e^{0.2 x} = 0; solved numerically
        // once (Newton on 1D) - value below is accurate to ~1e-12.
        return { {-0.049544705205334, -0.049544705205334, true} };
    }
};

// ------------------------------------------------ double well (saddle demo)
//  f(x,y) = 0.25 x^4 - 0.5 x^2 + 0.5 y^2
//  Minima at (+-1, 0); saddle at (0,0) with indefinite Hessian diag(-1, 1).
//  Started near the saddle, pure Newton walks INTO the saddle - the classic
//  motivation for safeguards, visible on the contour canvas.
class DoubleWell : public IObjective
{
public:
    int dim() const override { return 2; }
    const char* name() const override { return "Double well (saddle at 0)"; }

    double f(const double* x) const override
    {
        return 0.25 * x[0] * x[0] * x[0] * x[0]
             - 0.5  * x[0] * x[0]
             + 0.5  * x[1] * x[1];
    }

    void grad(const double* x, double* g) const override
    {
        g[0] = x[0] * x[0] * x[0] - x[0];
        g[1] = x[1];
    }

    void hess(const double* x, DblMatrix& H) const override
    {
        auto h = H.getManipulator();
        h(0, 0) = 3.0 * x[0] * x[0] - 1.0; h(0, 1) = 0.0;
        h(1, 0) = 0.0;                     h(1, 1) = 1.0;
    }

    void suggestedStart(double* x0) const override { x0[0] = 0.08; x0[1] = 1.6; }

    void suggestedView(double& xMin, double& xMax,
                       double& yMin, double& yMax) const override
    {
        xMin = -2.0; xMax = 2.0; yMin = -2.0; yMax = 2.0;
    }

    std::vector<Stationary> stationaryPoints() const override
    {
        return { {-1.0, 0.0, true}, {1.0, 0.0, true}, {0.0, 0.0, false} };
    }
};

// ------------------------------------------------------------- Rosenbrock
//  f(x,y) = (1-x)^2 + 100 (y - x^2)^2, minimum (1,1), curved ill-conditioned
//  valley - the classic first- vs second-order stress test.
class Rosenbrock : public IObjective
{
public:
    int dim() const override { return 2; }
    const char* name() const override { return "Rosenbrock"; }

    double f(const double* x) const override
    {
        const double a = 1.0 - x[0];
        const double b = x[1] - x[0] * x[0];
        return a * a + 100.0 * b * b;
    }

    void grad(const double* x, double* g) const override
    {
        const double b = x[1] - x[0] * x[0];
        g[0] = -2.0 * (1.0 - x[0]) - 400.0 * x[0] * b;
        g[1] = 200.0 * b;
    }

    void hess(const double* x, DblMatrix& H) const override
    {
        auto h = H.getManipulator();
        h(0, 0) = 2.0 - 400.0 * (x[1] - 3.0 * x[0] * x[0]);
        h(0, 1) = -400.0 * x[0];
        h(1, 0) = -400.0 * x[0];
        h(1, 1) = 200.0;
    }

    void suggestedStart(double* x0) const override { x0[0] = -1.2; x0[1] = 1.0; }

    void suggestedView(double& xMin, double& xMax,
                       double& yMin, double& yMax) const override
    {
        xMin = -2.0; xMax = 2.0; yMin = -1.0; yMax = 3.0;
    }

    std::vector<Stationary> stationaryPoints() const override
    {
        return { {1.0, 1.0, true} };
    }
};

// -------------------------------------------------------------- Himmelblau
//  f(x,y) = (x^2 + y - 11)^2 + (x + y^2 - 7)^2
//  Four minima, saddle points between them - non-convex with multiple basins.
class Himmelblau : public IObjective
{
public:
    int dim() const override { return 2; }
    const char* name() const override { return "Himmelblau (4 minima)"; }

    double f(const double* x) const override
    {
        const double a = x[0] * x[0] + x[1] - 11.0;
        const double b = x[0] + x[1] * x[1] - 7.0;
        return a * a + b * b;
    }

    void grad(const double* x, double* g) const override
    {
        const double a = x[0] * x[0] + x[1] - 11.0;
        const double b = x[0] + x[1] * x[1] - 7.0;
        g[0] = 4.0 * x[0] * a + 2.0 * b;
        g[1] = 2.0 * a + 4.0 * x[1] * b;
    }

    void hess(const double* x, DblMatrix& H) const override
    {
        const double a = x[0] * x[0] + x[1] - 11.0;
        const double b = x[0] + x[1] * x[1] - 7.0;
        auto h = H.getManipulator();
        h(0, 0) = 12.0 * x[0] * x[0] + 4.0 * x[1] - 44.0 + 2.0;
        h(0, 1) = 4.0 * x[0] + 4.0 * x[1];
        h(1, 0) = h(0, 1);
        h(1, 1) = 12.0 * x[1] * x[1] + 4.0 * x[0] - 28.0 + 2.0;
    }

    void suggestedStart(double* x0) const override { x0[0] = 0.0; x0[1] = 0.0; }

    void suggestedView(double& xMin, double& xMax,
                       double& yMin, double& yMax) const override
    {
        xMin = -5.5; xMax = 5.5; yMin = -5.5; yMax = 5.5;
    }

    std::vector<Stationary> stationaryPoints() const override
    {
        return {
            { 3.0,        2.0,       true},
            {-2.805118,   3.131312,  true},
            {-3.779310,  -3.283186,  true},
            { 3.584428,  -1.848126,  true},
            {-0.270845,  -0.923039,  false} // central saddle-ish max region
        };
    }
};

// ------------------------------------------------------------ benchmark set
//  Ordered as in the proposal: A) well-conditioned strongly convex,
//  B) ill-conditioned quadratics, C) non-convex with saddles.
inline std::vector<std::unique_ptr<IObjective>> makeBenchmarkSuite()
{
    std::vector<std::unique_ptr<IObjective>> v;
    // A
    v.push_back(std::make_unique<Quadratic2D>(2.0, 0.0,
                    "Convex quadratic (kappa = 2)"));
    v.push_back(std::make_unique<StronglyConvexExp>());
    // B
    v.push_back(std::make_unique<Quadratic2D>(100.0, 15.0 * M_PI / 180.0,
                    "Ill-conditioned quadratic (kappa = 100)"));
    v.push_back(std::make_unique<Quadratic2D>(2000.0, 30.0 * M_PI / 180.0,
                    "Rotated quadratic (kappa = 2000)"));
    // C
    v.push_back(std::make_unique<DoubleWell>());
    v.push_back(std::make_unique<Rosenbrock>());
    v.push_back(std::make_unique<Himmelblau>());
    return v;
}

} // namespace opt
