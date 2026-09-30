# Heat Geodesics

An interactive C++ implementation of the heat method for geodesic distance on triangle meshes. Select one or more source vertices in the Polyscope viewer, then compute a distance field or inspect heat diffusion directly.

The project uses a half-edge mesh representation to construct the cotangent Laplacian and Voronoi mass matrix. Its viewer is built with Polyscope and ImGui; meshes in `data/` can be switched from the interface.

## The heat method

The heat method approximates intrinsic distance by converting the problem into sparse linear solves:

1. Start with heat concentrated at the selected source vertices.
2. Diffuse that heat for a short, fixed time step using implicit Euler integration.
3. Normalize the negative heat gradient to obtain a vector field pointing away from the sources.
4. Compute its divergence and solve a Poisson equation. The result is the geodesic distance field.

This follows Crane, Weischedel, and Wardetzky's [*Geodesics in Heat*](https://www.cs.cmu.edu/~kmcrane/Projects/HeatMethod/paper.pdf).

## Build and run

Requirements: CMake 3.22 or newer, a C++17 compiler, and Git. CMake downloads the pinned libigl, Polyscope, and GoogleTest dependencies during configuration.

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/geodesicInHeat data/bunny_fine.off
```

The program accepts either a project-relative or absolute mesh path. Once open, use the **Loaded mesh** dropdown to switch among supported meshes found in `data/`.
