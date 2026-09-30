#ifndef LAPLACIANMESH_H
#define LAPLACIANMESH_H
#include "Mesh.h"
#include "Eigen/Sparse"
#include "Eigen/Core"
#include <Eigen/src/Core/util/Constants.h>

typedef Eigen::SparseMatrix<double> SpMat;

class LaplacianMesh : public Mesh
{
private:
  /* data */
  void compute_dirichlet();
  void compute_area_matrix();
  void compute_laplacian();

  SpMat integratorMatrix;

  Eigen::SimplicialLDLT<SpMat> solverForBackwardsEuler;

public:
  LaplacianMesh(/* args */);
  LaplacianMesh(Eigen::MatrixXd V, Eigen::MatrixXi F);
  ~LaplacianMesh();
  SpMat L;     // cotangent
  SpMat A;     // normalization for the laplacian
  SpMat Ainv;  // inverse of A for performance
  SpMat Delta; // laplacian
  double timeStep = 0.0001;
  void setHeat(std::vector<int> indicesHeatSources, Eigen::VectorXd &u);
  void setTimestep(double t);
  void heat_step_explicit(Eigen::VectorXd &u);
  void heat_step_implicit(Eigen::VectorXd &u);
  void simulateHeatFlowForGivenTime(Eigen::VectorXd &u, double duration);
  Eigen::MatrixXd computeGradient(const Eigen::VectorXd &u);
  Eigen::MatrixXd normalizeVectorfield(const Eigen::MatrixXd &gradU);
  Eigen::VectorXd computeDivergenceVectorField(const Eigen::MatrixXd &vectorField);
  Eigen::VectorXd computeGeodesicDistanceFunction(const Eigen::VectorXd &divergenceEikonal);
};

#endif // LAPLACIANMESH_H
