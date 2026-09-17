# OptimCompare

A C++ desktop app made to compare three optimization methods — **Steepest Descent**, **Newton's Method**, and **BFGS** — by watching them actually search for the minimum of a 2D function.

Pick a test function, pick a method (or run all three at once), pick how the step size gets chosen, and it draws each method's path on a contour plot along with a convergence chart so one can see who gets there fastest.

## Features

- All three methods run through the same loop, the only thing that changes is how the search direction is picked.
- Two ways to pick the step size: exact line search (closed form for quadratics, bracketing + golden-section search otherwise) or Armijo backtracking.
- 7 test functions across 3 difficulty levels — easy convex bowls, ill-conditioned/rotated bowls, and non-convex stuff with saddle points and multiple minima (Double Well, Rosenbrock, Himmelblau).
- Some safety nets so the methods don't blow up: Newton just falls back to plain gradient descent if the Hessian gives a bad direction, and BFGS skips an update if it would make its curvature guess worse, and starts the guess over from scratch if things get numerically unstable.
- A condition-number (κ) study mode that stretches the bowl more and more to see how each method's iteration count scales.
- Everything shown live: contour plot, convergence chart, condition-number chart.

## Requirements

- CMake ≥ 3.18
- A C++ compiler 
- The [natID SDK](https://github.com/idzafic/natID) (`natGUI` + `MatrixLib`), expected at `~/natID.SDK`

## Build

```bash
git clone https://github.com/farahpiralic/ProjNo_OptimC_Piralic.git
cd ProjNo_OptimC_Piralic/Implementation
cmake -S . -B build
cmake --build build --config Release
```

## Prebuilt releases

Pushing a tag (`git tag v1.0.0 && git push origin v1.0.0`) kicks off a GitHub Actions workflow that builds installers for Windows, macOS (Apple Silicon + Intel), and Linux. Check the [Releases](../../releases) page for the latest one.
