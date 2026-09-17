# OptimCompare

A little C++ desktop app I made to compare three optimization methods — **Steepest Descent**, **Newton's Method**, and **BFGS** — by watching them actually search for the minimum of a 2D function.

You pick a test function, pick a method (or run all three at once), pick how the step size gets chosen, and it draws each method's path on a contour plot along with a convergence chart so you can see who gets there fastest.

## Features

- All three methods run through the same loop, the only thing that changes is how the search direction is picked.
- Two ways to pick the step size: exact line search (closed form for quadratics, bracketing + golden-section search otherwise) or Armijo backtracking.
- 7 test functions, from easy convex bowls to non-convex ones with saddle points and multiple minima (see below).
- Some safety nets so the methods don't blow up: Newton just falls back to plain gradient descent if the Hessian gives a bad direction, and BFGS skips an update if it would make its curvature guess worse, and starts the guess over from scratch if things get numerically unstable.
- A condition-number (κ) study mode that stretches the bowl more and more to see how each method's iteration count scales.
- Everything shown live: contour plot, convergence chart, condition-number chart.

## Test Functions

Functions 1–3 are all the same general quadratic bowl, just stretched (κ) and rotated (θ) differently:

```
f(x,y) = 0.5 (a₁₁x² + 2a₁₂xy + a₂₂y²)
where  a₁₁ = cos²θ + κsin²θ,  a₂₂ = sin²θ + κcos²θ,  a₁₂ = (1−κ)cosθ·sinθ
```

| # | Function | Parameters | Minimum |
|---|---|---|---|
| 1 | Convex quadratic | κ = 2, θ = 0° | (0, 0) |
| 2 | Ill-conditioned quadratic | κ = 100, θ = 15° | (0, 0) |
| 3 | Rotated quadratic | κ = 2000, θ = 30° | (0, 0) |

The rest have their own explicit formulas:

| # | Function | Equation | Minimum |
|---|---|---|---|
| 4 | Strongly convex (quad + exp) | f(x,y) = x² + y² + exp(0.1(x+y)) | (−0.0495, −0.0495) |
| 5 | Double Well | f(x,y) = 0.25x⁴ − 0.5x² + 0.5y² | (−1,0) and (1,0), saddle at (0,0) |
| 6 | Rosenbrock | f(x,y) = (1−x)² + 100(y−x²)² | (1, 1) |
| 7 | Himmelblau | f(x,y) = (x²+y−11)² + (x+y²−7)² | 4 minima, e.g. (3,2), (−2.81,3.13) |

## Requirements

- A C++ compiler 
- CMake ≥ 3.18
- The [natID SDK](https://github.com/idzafic/natID) (`natGUI` + `MatrixLib`), placed in your home folder as `natID.SDK`

## Build

1. Clone the repo: `git clone https://github.com/farahpiralic/ProjNo_OptimC_Piralic.git`
2. Open **CMake GUI**.
3. Set "Where is the source code" to the `Implementation` folder inside the cloned repo.
4. Set "Where to build the binaries" to a `build` folder.
5. Click **Configure**, then **Generate**.

## Prebuilt releases

Tagged pushes (`git tag v1.0.0 && git push origin v1.0.0`) trigger a GitHub Actions workflow that builds installers for Windows, macOS (Apple Silicon + Intel), and Linux. Check the [Releases](../../releases) page for the latest one.
