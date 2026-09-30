#include "Mesh.h"

#include <iostream>
#include <memory>

#include <Eigen/Cholesky>
#include <Eigen/Geometry>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/SparseCholesky>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/util/Constants.h>
#include <Eigen/src/SparseCore/SparseMatrix.h>
#include <igl/cotmatrix.h>
#include <igl/edges.h>
#include <igl/facet_adjacency_matrix.h>
#include <igl/gaussian_curvature.h>
#include <igl/massmatrix.h>

#include "MeshParts.h"

using Eigen::Vector3d, Eigen::Vector3i, Eigen::VectorXd, Eigen::MatrixXd;

Mesh::Mesh(Eigen::MatrixXd V, Eigen::MatrixXi F) : V(V), F(F) {
  this->initialize_complex();
  this->compute_hedges();
  this->compute_geometry_for_mesh_parts();
}

void Mesh::initialize_complex() {
  for (int i = 0; i < V.rows(); ++i) {
    Vector3d pos = V.row(i);
    VertexPtr new_vert = std::make_shared<Vertex>(i);
    new_vert->position = pos;

    primal_vertices.push_back(new_vert);
  }

  for (int i = 0; i < F.rows(); ++i) {
    Vector3i verts = F.row(i);
    PrimalFacePtr new_face = std::make_shared<PrimalFace>();
    new_face->index = i;

    for (auto v : verts) {
      new_face->addVertex(primal_vertices[v]);
      primal_vertices[v]->addOneRingFace(new_face);
    }

    primal_faces.push_back(new_face);
  }
}

void Mesh::compute_hedges() {
  // first pass, set everything but flip edges
  for (int i = 0; i < primal_faces.size(); ++i) {
    PrimalFacePtr face = primal_faces[i];

    for (int first = 0; first < 3; ++first) {
      int second = (first + 1) % 3;
      int hedge_idx = (i * 3) + first;
      auto vertices = face->getVertices();

      HalfEdgePtr new_hedge = std::make_shared<HalfEdge>(hedge_idx);
      new_hedge->setStartVertex(vertices[first]);
      new_hedge->setEndVertex(vertices[second]);
      new_hedge->setPrimalFace(face);

      // if not first hedge in the face, set previous hedge and next of previous
      if (first != 0) {
        new_hedge->setPreviousHalfEdge(hedges[hedge_idx - 1]);
        hedges[hedge_idx - 1]->setNextHalfEdge(new_hedge);
      }

      face->addHalfEdge(new_hedge);
      vertices[first]->addOutgoingHalfEdge(new_hedge);

      hedges.push_back(new_hedge);
    }

    // set remaining previous and next hedges for first and last
    int first_hedge_idx = i * 3, last_hedge_idx = (i * 3) + 2;
    hedges[first_hedge_idx]->setPreviousHalfEdge(hedges[last_hedge_idx]);
    hedges[last_hedge_idx]->setNextHalfEdge(hedges[first_hedge_idx]);
  }

  // set flip edges
  Eigen::SparseMatrix<int> face_adjecency;
  igl::facet_adjacency_matrix(F, face_adjecency);

  for (int k = 0; k < face_adjecency.outerSize(); ++k) {
    for (Eigen::SparseMatrix<int>::InnerIterator it(face_adjecency, k); it; ++it) {
      PrimalFacePtr face1 = primal_faces[it.row()];
      PrimalFacePtr face2 = primal_faces[it.col()];

      for (HalfEdgePtr hedge : face1->getHalfEdges()) {
        for (HalfEdgePtr other_hegde : face2->getHalfEdges()) {
          if (hedge->getIndexOfStartVertex() == other_hegde->getIndexOfEndVertex() &&
              hedge->getIndexOfEndVertex() == other_hegde->getIndexOfStartVertex()) {
            hedge->setFlipHalfEdge(other_hegde);
            other_hegde->setFlipHalfEdge(hedge);
          }
        }
      }
    }
  }

  // check boundaries
  for (HalfEdgePtr hedge : hedges) {
    // check if flip edge is not assigned
    if (hedge->flip.expired()) {
      hedge->boundary = true;
      hedge->getStartVertex()->is_boundary = true;
      hedge->getEndVertex()->is_boundary = true;
      hedge->getPrimalFace()->is_boundary = true;
    }
  }
}

Eigen::Vector3d Mesh::compute_circumcenter_triangle(
    const Eigen::Vector3d& a, const Eigen::Vector3d& b, const Eigen::Vector3d& c
) {
  Vector3d p12 = a - b;
  Vector3d p13 = a - c;
  Vector3d p23 = b - c;

  double denom = 2.0 * p12.cross(p23).squaredNorm();

  double coeff_a = (p23.squaredNorm() * p12.dot(p13)) / denom;
  double coeff_b = (p13.squaredNorm() * p23.dot(-p12)) / denom;
  double coeff_c = (p12.squaredNorm() * p13.dot(p23)) / denom;

  return coeff_a * a + coeff_b * b + coeff_c * c;
}

double Mesh::angle_between_vectors(const Eigen::Vector3d& v, const Eigen::Vector3d& w) {
  return atan2(v.cross(w).norm(), v.dot(w));
}

double Mesh::adjustToRange(double angle) {
  const double twoPi = 2 * M_PI;
  const double pi = M_PI;
  // Ensure the angle is within the range [-pi, pi]
  while (angle < -pi) {
    angle += twoPi;
  }
  while (angle > pi) {
    angle -= twoPi;
  }
  return angle;
};

void Mesh::compute_geometry_for_mesh_parts() {
  // primal face geometry computations
  for (PrimalFacePtr face : primal_faces) {
    std::vector<VertexPtr> verts = face->getVertices();
    Vector3d p1 = verts[0]->position;
    Vector3d p2 = verts[1]->position;
    Vector3d p3 = verts[2]->position;

    Vector3d cross = (p2 - p1).cross(p3 - p1);

    double ang1 = angle_between_vectors(p2 - p1, p3 - p1);
    double ang2 = angle_between_vectors(p1 - p2, p3 - p2);
    double ang3 = angle_between_vectors(p1 - p3, p2 - p3);

    face->circumcenter = compute_circumcenter_triangle(p1, p2, p3);
    face->barycenter = (p1 + p2 + p3) / 3.0;
    face->area = cross.norm() / 2.0;
    face->normal = cross.normalized();
    face->is_obtuse = ang1 > M_PI_2 || ang2 > M_PI_2 || ang3 > M_PI_2;
  }

  // half edge computations
  for (HalfEdgePtr hedge : hedges) {
    Vector3d p_start = hedge->getStartVertex()->position;
    Vector3d p_end = hedge->getEndVertex()->position;
    Vector3d p_opp = hedge->getNextHalfEdge()->getEndVertex()->position;

    hedge->tail_angle = angle_between_vectors(p_end - p_start, p_opp - p_start);
    hedge->tip_angle = angle_between_vectors(p_start - p_end, p_opp - p_end);

    Vector3d u = p_start - p_opp, v = p_end - p_opp;
    hedge->cotangentOfOppAngle = u.dot(v) / u.cross(v).norm();
  }

  // voronoi area around each vertex
  compute_voronoi_area();

  // gaussian curvatures for each vertex
  VectorXd gauss_curvs = compute_gaussian_curvature();

  // vertex computations
  for (VertexPtr vert : primal_vertices) {
    Vector3d normal(0.0, 0.0, 0.0);
    for (PrimalFacePtr face : vert->getOneRingFaces()) {
      normal += face->normal * face->area;
    }

    vert->normal = normal.normalized();
    vert->gaussian_curvature_tip = gauss_curvs[vert->index];
  }
}

void Mesh::vertexDegreeStatistics() {
  std::cout << "Computing vertex degree distribution " << std::endl;
  auto start = std::chrono::high_resolution_clock::now();  // for measuring time
                                                           // performances
  int* vDS = new int[V.rows()];
  std::fill_n(vDS, V.rows(), 0);
  
  for (VertexPtr v : primal_vertices) {
    if (!v->is_boundary) vDS[v->getOutgoingHalfEdges().size()]++;
    else vDS[v->getOutgoingHalfEdges().size() + 1]++;
  }

  for (int i = 3; i < V.rows(); i++)
    if (vDS[i] != 0)
      std::cout << "number of degrees = " << i << " ; number of occurences = " << vDS[i]
                << std::endl;
  std::cout << "Zero for the other vertex degrees between d=3 and n-1" << std::endl;
  auto finish = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = finish - start;

  delete[] vDS;
}

Eigen::VectorXd Mesh::compute_gaussian_curvature() {
  Eigen::VectorXd K(primal_vertices.size());

  for (VertexPtr vert : primal_vertices) {
    double ang = 0.0;
    for (HalfEdgePtr hedge : vert->getOutgoingHalfEdges()) ang += hedge->tail_angle;

    K[vert->index] = (2 * M_PI - ang) / vert->voronoi_area;
  }

  return K;
}

void Mesh::compute_voronoi_area() {
  for (VertexPtr vert : primal_vertices) {
    double area = 0.0;

    for (HalfEdgePtr hedge : vert->getOutgoingHalfEdges()) {
      PrimalFacePtr face = hedge->getPrimalFace();
      bool obtuse = face->is_obtuse;

      if (!obtuse) {
        Vector3d pi = hedge->getStartVertex()->position;
        Vector3d pj = hedge->getEndVertex()->position;
        Vector3d p_opp = hedge->getNextHalfEdge()->getEndVertex()->position;

        area += 0.125 *
                (hedge->cotangentOfOppAngle * (pi - pj).squaredNorm() +
                 hedge->getPreviousHalfEdge()->cotangentOfOppAngle * (pi - p_opp).squaredNorm());
      } else {
        bool obtuse_at_p = hedge->tail_angle > M_PI_2;

        if (obtuse_at_p)
          area += face->area / 2.0;
        else
          area += face->area / 4.0;
      }
    }

    vert->voronoi_area = area;
  }
}

void Mesh::count_boundaries() {
  int boundaries = 0;
  for (int i = 0; i < primal_faces.size(); ++i)
    if (primal_faces[i]->is_boundary) boundaries++;

  this->nBoundaryComponents = boundaries;

  std::cout << "The number of boundaries is " << this->nBoundaryComponents << std::endl;
}

Eigen::MatrixXd Mesh::compute_vertex_normals() {
  Eigen::MatrixXd normals = Eigen::MatrixXd::Zero(V.rows(), 3);

  for (int i = 0; i < F.rows(); ++i) {
    Vector3i face = F.row(i);
    Vector3d p0 = V.row(face[0]);
    Vector3d p1 = V.row(face[1]);
    Vector3d p2 = V.row(face[2]);

    Vector3d cross = (p1 - p0).cross(p2 - p0); // 2 * area * n

    normals.row(face[0]) += cross;
    normals.row(face[1]) += cross;
    normals.row(face[2]) += cross;
  }

  for (int i = 0; i < V.rows(); ++i) normals.row(i).normalize();

  return normals;
}

MatrixXd Mesh::compute_vertex_normals_hed() {
  MatrixXd normals = MatrixXd::Zero(V.rows(), 3);

  for (PrimalFacePtr face : primal_faces) {
    std::vector<VertexPtr> verts = face->getVertices();
    Vector3d p0 = verts[0]->position;
    Vector3d p1 = verts[1]->position;
    Vector3d p2 = verts[2]->position;

    Vector3d cross = (p1 - p0).cross(p2 - p0); // 2 * area * n

    normals.row(verts[0]->index) += cross;
    normals.row(verts[1]->index) += cross;
    normals.row(verts[2]->index) += cross;
  }

  for (VertexPtr vert : primal_vertices) normals.row(vert->index).normalize();

  return normals;
}

MatrixXd Mesh::compute_face_normals() {
  MatrixXd normals = MatrixXd::Zero(F.rows(), 3);

  for (int i = 0; i < F.rows(); ++i) {
    Vector3i face = F.row(i);
    Vector3d p0 = V.row(face[0]);
    Vector3d p1 = V.row(face[1]);
    Vector3d p2 = V.row(face[2]);

    Vector3d cross = (p1 - p0).cross(p2 - p0);

    normals.row(i) = cross.normalized();
  }

  return normals;
}

MatrixXd Mesh::compute_face_normals_hed() {
  MatrixXd normals = MatrixXd::Zero(F.rows(), 3);

  for (PrimalFacePtr face : primal_faces) {
    std::vector<VertexPtr> verts = face->getVertices();
    Vector3d p0 = verts[0]->position;
    Vector3d p1 = verts[1]->position;
    Vector3d p2 = verts[2]->position;

    Vector3d cross = (p1 - p0).cross(p2 - p0);

    normals.row(face->index) = cross.normalized();
  }

  return normals;
}
