#include "LaplacianMesh.h"
#include <stdexcept>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/util/Constants.h>
#include <Eigen/src/SparseCore/SparseUtil.h>
#include "Eigen/SparseCholesky"
#include "MeshParts.h"
#include "igl/cotmatrix.h"
#include "igl/massmatrix.h"

using Eigen::MatrixXd, Eigen::MatrixXi, Eigen::VectorXd, Eigen::Vector3d, Eigen::Triplet;

LaplacianMesh::LaplacianMesh(/* args */) {}

LaplacianMesh::~LaplacianMesh() {}

void LaplacianMesh::compute_dirichlet()
{
  const int n = V.rows();

  L.resize(n, n);
  std::vector<Triplet<double>> triplets;
  
  for (VertexPtr vert : primal_vertices) {
    double cot_sum = 0.0;

    for (HalfEdgePtr hedge : vert->getOutgoingHalfEdges()) {
      VertexPtr neigh = hedge->getEndVertex();
      double cot;

      if (!hedge->boundary)
        cot = 0.5 * (hedge->cotangentOfOppAngle + hedge->getFlipHalfEdge()->cotangentOfOppAngle);
      else
        cot = 0.5 * hedge->cotangentOfOppAngle;

      cot_sum += cot;

      triplets.emplace_back(vert->index, neigh->index, cot);
      if (hedge->boundary) triplets.emplace_back(neigh->index, vert->index, -cot);
    }

    triplets.emplace_back(vert->index, vert->index, -cot_sum);
  }

  L.setFromTriplets(triplets.begin(), triplets.end());
}

void LaplacianMesh::compute_area_matrix()
{
  const int n = V.rows();

  A.resize(n, n);
  Ainv.resize(n, n);
  std::vector<Triplet<double>> triplets;

  for (VertexPtr vert : primal_vertices) {
    triplets.emplace_back(vert->index, vert->index, vert->voronoi_area);
  }

  A.setFromTriplets(triplets.begin(), triplets.end());
  Ainv = A.cwiseInverse();
}

void LaplacianMesh::compute_laplacian()
{
  const int n = V.rows();

  Delta.resize(n, n);
  Delta = Ainv * L;

  integratorMatrix.resize(n, n);
  integratorMatrix = A - (timeStep * L);

  solverForBackwardsEuler.analyzePattern(integratorMatrix);
  setTimestep(timeStep); 
}

LaplacianMesh::LaplacianMesh(Eigen::MatrixXd V, Eigen::MatrixXi F)
    : Mesh(V, F)
{
  this->V = V;
  this->F = F;

  compute_dirichlet();
  compute_area_matrix();
  compute_laplacian();

  // std::cout << " constructed LaplacianMesh " << std::endl;
}

void LaplacianMesh::setHeat(std::vector<int> indicesHeatSources, Eigen::VectorXd &u)
{
  u.setZero();
  for (int heatSource : indicesHeatSources)
  {
    u(heatSource) = 0.5;
  }
}

void LaplacianMesh::setTimestep(double t)
{
  timeStep = t;
  integratorMatrix = A - (timeStep * L);

  solverForBackwardsEuler.factorize(integratorMatrix);
  if (solverForBackwardsEuler.info() != Eigen::Success) {
    throw std::runtime_error("Decomposition failed.");
  }
}

void LaplacianMesh::heat_step_explicit(Eigen::VectorXd &u)
{
  // timestep should be at max 0.0001 to be stable (experimentally found)
  u += timeStep * (Delta * u);
}

void LaplacianMesh::heat_step_implicit(Eigen::VectorXd &u)
{
  VectorXd rhs = A * u;
  u = solverForBackwardsEuler.solve(rhs);
}

void LaplacianMesh::simulateHeatFlowForGivenTime(Eigen::VectorXd &u,
                                                 double duration)
{
  int n_steps = std::ceil(duration / timeStep);

  for (int i = 0; i < n_steps; ++i) heat_step_implicit(u);
}

Eigen::MatrixXd LaplacianMesh::computeGradient(const Eigen::VectorXd &u)
{

  Eigen::MatrixXd gradient = Eigen::MatrixXd::Zero(F.rows(), 3);

  for (PrimalFacePtr face : primal_faces) {
    Eigen::Vector3d grad(0.0, 0.0, 0.0);

    for (HalfEdgePtr hedge : face->getHalfEdges()) {
      double u_vert = u[hedge->getNextHalfEdge()->getIndexOfEndVertex()];
      Eigen::Vector3d edge = hedge->getEndVertex()->position - hedge->getStartVertex()->position;

      grad += u_vert * face->normal.cross(edge);
    }

    grad /= 2 * face->area;
    
    gradient.row(face->index) = grad;
  }

  return gradient;
}

Eigen::MatrixXd
LaplacianMesh::normalizeVectorfield(const Eigen::MatrixXd &gradU)
{

  Eigen::MatrixXd normalized = Eigen::MatrixXd::Zero(gradU.rows(), 3);

  for (int i = 0; i < gradU.rows(); ++i) {
    normalized.row(i) = -gradU.row(i).normalized();
  }

  return normalized;
}

Eigen::VectorXd LaplacianMesh::computeDivergenceVectorField(
    const Eigen::MatrixXd &vectorField)
{
  Eigen::VectorXd div = Eigen::VectorXd::Zero(V.rows());

  for (VertexPtr vert : primal_vertices) {
    double div_acc = 0.0;

    for (PrimalFacePtr face : vert->getOneRingFaces()) {
      for (HalfEdgePtr hedge : face->getHalfEdges()) {
        if (hedge->getIndexOfStartVertex() != vert->index &&
            hedge->getIndexOfEndVertex() != vert->index) {
            continue;
        }

        bool is_start_vert = vert->index == hedge->getIndexOfStartVertex();
        Eigen::Vector3d edge = hedge->getEndVertex()->position - hedge->getStartVertex()->position;

        if (is_start_vert) edge = -edge;

        div_acc += hedge->cotangentOfOppAngle * edge.dot(vectorField.row(face->index));
      }
    }

    div_acc /= 2 * vert->voronoi_area;
    div[vert->index] = div_acc;
  }

  return div;
}

Eigen::VectorXd LaplacianMesh::computeGeodesicDistanceFunction(
    const Eigen::VectorXd &divergenceEikonal)
{
  Eigen::SimplicialLDLT<SpMat> solver;
  solver.analyzePattern(L);
  solver.factorize(L);

  if (solver.info() != Eigen::Success) {
    throw std::runtime_error("Decomposition failed (geodesic computation).");
  }

  //--------------------------------------------------
  // DO NOT REMOVE: Important surgery to avoid numerical issues.
  //--------------------------------------------------

  double projectAwayFromKernelUp = 0.;
  double projectAwayFromKernelDown = 0.;
  for (VertexPtr vertex : primal_vertices)
  {
    projectAwayFromKernelUp += vertex->voronoi_area * divergenceEikonal(vertex->index);
    projectAwayFromKernelDown += vertex->voronoi_area;
  }
  double projectAwayFromKernel = projectAwayFromKernelUp / projectAwayFromKernelDown;
  Eigen::VectorXd divergenceEikonalCopy = divergenceEikonal;
  for (size_t i = 0; i < divergenceEikonal.size(); ++i)
  {
    divergenceEikonalCopy(i) -= projectAwayFromKernel;
  }
  //--------------------------------------------------
  // Numerical surgery end
  //--------------------------------------------------

  Eigen::VectorXd distanceFunction = solver.solve(A * divergenceEikonalCopy);

  distanceFunction *= -1.0;
  distanceFunction.array() -= distanceFunction.minCoeff();

  return distanceFunction;
}
