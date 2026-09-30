#ifndef MESH_H
#define MESH_H
#include "Eigen/Sparse"
#include "MeshParts.h"
#include <array>

using namespace MeshParts;
typedef Eigen::SparseMatrix<double> SparseMatrixXd;

class Mesh
{
private:
  Eigen::Vector3d compute_circumcenter_triangle(const Eigen::Vector3d &a,
                                                const Eigen::Vector3d &b,
                                                const Eigen::Vector3d &c);
  double adjustToRange(double angle);
  double wrapToPi(double angle);
  double angle_between_vectors(const Eigen::Vector3d &v,
                               const Eigen::Vector3d &w);
  //-------------------------------------
protected:
  void initialize_complex();
  void compute_hedges();
  void compute_geometry_for_mesh_parts();

  double cotangent(double x) { return 1 / (tan(x)); }

public:
  //-----------Raw Mesh Data----------------
  Eigen::MatrixXd V; // primal vertices
  Eigen::MatrixXi F; // primal triangular faces

  bool open_mesh = false;
  int nBoundaryComponents = -1;
  // int n_boundary_vertices = 0;
  // int n_boundary_edges = 0;

  std::vector<std::vector<int>> F_dual; // indices dual faces

  SparseMatrixXd cotangent_matrix;
  SparseMatrixXd mass_matrix;

  //-----------Parts of the mesh with logic----------------
  std::vector<std::shared_ptr<Vertex>> primal_vertices;
  std::vector<std::shared_ptr<PrimalFace>> primal_faces;
  std::vector<std::shared_ptr<HalfEdge>> hedges;

  Eigen::VectorXd compute_gaussian_curvature();
  void compute_voronoi_area();
  void vertexDegreeStatistics();

  Eigen::MatrixXd compute_vertex_normals();
  Eigen::MatrixXd compute_vertex_normals_hed();

  Eigen::MatrixXd compute_face_normals();
  Eigen::MatrixXd compute_face_normals_hed();

  void count_boundaries();

  //----------------------------------------------
  // Methods to return the field information of tcods and the cross field

  SparseMatrixXd frame_matrix;
  std::vector<Eigen::Vector3d> basis_X;
  std::vector<Eigen::Vector3d> basis_Y;

  Mesh(/* args */) = default;
  Mesh(Eigen::MatrixXd, Eigen::MatrixXi);
  //------------------------------------------

  ~Mesh() = default;
};

#endif
