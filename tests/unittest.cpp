#include <LaplacianMesh.h>
#include <Mesh.h>

#include <Eigen/src/Core/CoreIterators.h>

#include "Eigen/Core"
#include "gtest/gtest.h"
#include "igl/barycenter.h"
#include "igl/circumradius.h"
#include "igl/cotmatrix.h"
#include "igl/doublearea.h"
#include "igl/edges.h"
#include "igl/gaussian_curvature.h"
#include "igl/internal_angles.h"
#include "igl/massmatrix.h"
#include "igl/per_face_normals.h"
#include "igl/read_triangle_mesh.h"
#include "MeshParts.h"

Eigen::MatrixXd V0;
Eigen::MatrixXi F0;
Eigen::MatrixXi E0;

std::string path_to_source = std::string(PROJECT_SOURCE_DIR);
std::string meshBunny = "/data/bunny_fine.off";
std::string fullPath = path_to_source + meshBunny;
int i0 = igl::read_triangle_mesh(fullPath, V0, F0);

Mesh test_mesh = Mesh(V0, F0);
LaplacianMesh lap_mesh = LaplacianMesh(V0, F0);

double tol = 1e-5;

TEST(MeshTest, CheckBasicSanityMesh) {
  EXPECT_EQ(test_mesh.primal_vertices.size(), V0.rows());
  EXPECT_EQ(test_mesh.primal_faces.size(), F0.rows());
}

// Test that the pointer is initially null
TEST(VertexTest, CheckGoodInitialization) {
  int n_vertices = test_mesh.primal_vertices.size();

  for (int idx_vtx = 0; idx_vtx < n_vertices; idx_vtx++) {
    VertexPtr vtx = test_mesh.primal_vertices[idx_vtx];
    std::vector<PrimalFacePtr> adjacent_faces = vtx->getOneRingFaces();
    std::vector<HalfEdgePtr> outgoingHalfedges = vtx->getOutgoingHalfEdges();
    EXPECT_GT(adjacent_faces.size(), 0);
    EXPECT_GT(outgoingHalfedges.size(), 0);
    for (PrimalFacePtr ptr : adjacent_faces) {
      ASSERT_NE(ptr, nullptr);
    }

    for (HalfEdgePtr ptr : outgoingHalfedges) {
      ASSERT_NE(ptr, nullptr);
      ASSERT_NE(ptr->getStartVertex(), nullptr);
      ASSERT_NE(ptr->getEndVertex(), nullptr);
      ASSERT_NE(ptr->getNextHalfEdge(), nullptr);
      ASSERT_NE(ptr->getPrimalFace(), nullptr);

      if (!ptr->boundary)
        ASSERT_NE(ptr->getFlipHalfEdge(), nullptr);
      else
        ASSERT_ANY_THROW(ptr->getFlipHalfEdge());
    }

    EXPECT_LE(vtx->index, n_vertices);
  }
}

TEST(VertexTest, CheckGeometry) {
  int n_vertices = test_mesh.primal_vertices.size();
  Eigen::VectorXd gcurv;
  igl::gaussian_curvature(V0, F0, gcurv);
  Eigen::SparseMatrix<double> mass;
  igl::massmatrix(V0, F0, igl::MASSMATRIX_TYPE_VORONOI, mass);

  // Check that gaussian curvature is correct
  for (int idx_vtx = 0; idx_vtx < n_vertices; idx_vtx++) {
    VertexPtr vtx = test_mesh.primal_vertices[idx_vtx];
    // EXPECT_NEAR(vtx->gaussian_curvature_tip, gcurv[vtx->index], tol);
    EXPECT_NEAR(
        vtx->gaussian_curvature_tip, gcurv[vtx->index] / mass.coeff(vtx->index, vtx->index), tol
    );
  }
  // Check that area is correct
  for (int idx_vtx = 0; idx_vtx < n_vertices; idx_vtx++) {
    VertexPtr vtx = test_mesh.primal_vertices[idx_vtx];
    EXPECT_NEAR(vtx->voronoi_area, mass.coeff(vtx->index, vtx->index), tol);
  }
}

TEST(HalfEdgeTest, CheckBasicSanity) { ASSERT_EQ(test_mesh.hedges.size(), 3 * F0.rows()); }

// Test case for verifying if all the pointer attributes are indeed attributed
TEST(HalfEdgeTest, CheckGoodInitialization) {
  for (int hedge_itr = 0; hedge_itr < test_mesh.hedges.size(); hedge_itr++) {
    HalfEdgePtr hedge = test_mesh.hedges[hedge_itr];
    // make sure that the pointer attributes are actually all initialized
    ASSERT_NE(hedge->getStartVertex(), nullptr);
    ASSERT_NE(hedge->getEndVertex(), nullptr);
    ASSERT_NE(hedge->getNextHalfEdge(), nullptr);
    ASSERT_NE(hedge->getPrimalFace(), nullptr);
    if (!hedge->boundary)
      ASSERT_NE(hedge->getFlipHalfEdge(), nullptr);
    else
      ASSERT_ANY_THROW(hedge->getFlipHalfEdge());

    if (!hedge->boundary) {
      ASSERT_EQ(hedge->getFlipHalfEdge()->getStartVertex(), hedge->getEndVertex());
      ASSERT_EQ(hedge->getFlipHalfEdge()->getEndVertex(), hedge->getStartVertex());
      ASSERT_EQ(hedge->getFlipHalfEdge()->getFlipHalfEdge(), hedge);
      ASSERT_NE(hedge->getFlipHalfEdge()->getNextHalfEdge(), nullptr);
      ASSERT_NE(hedge->getFlipHalfEdge()->getPrimalFace(), nullptr);
    }
  }
}

// Test case for verifying if the two vectors contain the same pointers for each Vertex
TEST(HalfEdgeTest, SanityCheckFlipFlip) {
  // Check if the flip attribute is set properly
  for (int hedge_itr = 0; hedge_itr < test_mesh.hedges.size(); hedge_itr++) {
    HalfEdgePtr hedge = test_mesh.hedges[hedge_itr];
    if (!hedge->boundary) EXPECT_EQ(hedge, (hedge->getFlipHalfEdge()->getFlipHalfEdge()));
  }
}

// Test to make sure that everywhere there is a pointer as a class attribute, it is indeed not a
// nullpointer
TEST(HalfEdgeTest, SanityCheckNext) {
  // Check if for a triangle mesh there is actually next next next fine

  for (int hedge_itr = 0; hedge_itr < test_mesh.hedges.size(); hedge_itr++) {
    HalfEdgePtr hedge = test_mesh.hedges[hedge_itr];
    PrimalFacePtr face = hedge->getPrimalFace();
    EXPECT_EQ(hedge, (hedge->getNextHalfEdge()->getNextHalfEdge()->getNextHalfEdge()));
    EXPECT_EQ(face, (hedge->getNextHalfEdge()->getPrimalFace()));
    EXPECT_EQ(face, (hedge->getNextHalfEdge()->getNextHalfEdge()->getPrimalFace()));
    EXPECT_EQ(
        face, (hedge->getNextHalfEdge()->getNextHalfEdge()->getNextHalfEdge()->getPrimalFace())
    );
  }
}

TEST(HalfEdgeTest, SanityCheckPrimalFace) {
  // Check if the pointer attributes are actually all initialized
  for (int hedge_itr = 0; hedge_itr < test_mesh.hedges.size(); hedge_itr++) {
    HalfEdgePtr hedge = test_mesh.hedges[hedge_itr];
    ASSERT_NE(hedge->getPrimalFace(), nullptr);
    ASSERT_NE(hedge->getNextHalfEdge()->getPrimalFace(), nullptr);
    if (!hedge->boundary) {
      ASSERT_NE(hedge->getFlipHalfEdge()->getPrimalFace(), nullptr);
      ASSERT_EQ(hedge->getPrimalFace(), hedge->getFlipHalfEdge()->getFlipHalfEdge()->getPrimalFace());
    }
  }
}

TEST(HalfEdgeTest, AnglesAndCotangent) {
  // Check if tail angles and tip angles are consistent
  for (int hedge_itr = 0; hedge_itr < test_mesh.hedges.size(); hedge_itr++) {
    HalfEdgePtr hedge = test_mesh.hedges[hedge_itr];
    ASSERT_EQ(hedge->tip_angle, hedge->getNextHalfEdge()->tail_angle);
  }
  // Check if the angles values are correct (compare product to avoid indexing differences issues)
  Eigen::MatrixXd K0;
  igl::internal_angles(V0, F0, K0);
  for (std::shared_ptr<PrimalFace> face : test_mesh.primal_faces) {
    std::vector<MeshParts::HalfEdgePtr> heptr = face->getHalfEdges();
    EXPECT_NEAR(
        heptr[0]->tip_angle * heptr[1]->tip_angle * heptr[2]->tip_angle, K0.row(face->index).prod(),
        tol
    );
  }
}

TEST(FaceTest, CheckGeometry) {
  Eigen::VectorXd FA, R;
  Eigen::MatrixXd BC, CC, B, N;
  igl::barycenter(V0, F0, BC);
  igl::doublearea(V0, F0, FA);
  igl::circumradius(V0, F0, R, CC, B);
  igl::per_face_normals(V0, F0, N);

  for (int face_itr = 0; face_itr < test_mesh.primal_faces.size(); face_itr++) {
    // Check barycenter
    PrimalFacePtr ptr = test_mesh.primal_faces[face_itr];
    Eigen::Vector3d bc_custom = ptr->barycenter;
    Eigen::Vector3d bc_igl = BC.row(ptr->index);
    EXPECT_NEAR(bc_custom.x(), bc_igl.x(), tol);
    EXPECT_NEAR(bc_custom.y(), bc_igl.y(), tol);
    EXPECT_NEAR(bc_custom.z(), bc_igl.z(), tol);

    // Check area
    EXPECT_NEAR(2. * ptr->area, FA[ptr->index], tol);

    // Check circumcenter
    Eigen::Vector3d cc_custom = ptr->circumcenter;
    Eigen::Vector3d cc_igl = CC.row(ptr->index);
    EXPECT_NEAR(cc_custom.x(), cc_igl.x(), tol);
    EXPECT_NEAR(cc_custom.y(), cc_igl.y(), tol);
    EXPECT_NEAR(cc_custom.z(), cc_igl.z(), tol);

    // Check normals
    Eigen::Vector3d n_custom = ptr->normal;
    Eigen::Vector3d n_igl = N.row(ptr->index);
    EXPECT_NEAR(n_custom.x(), n_igl.x(), tol);
    EXPECT_NEAR(n_custom.y(), n_igl.y(), tol);
    EXPECT_NEAR(n_custom.z(), n_igl.z(), tol);
  }
}

TEST(LaplacianTest, CheckWithLibigl) {
  //  Use the cotmatrix from libigl to compare with the dirichlet matrix from the mesh
  SpMat L_check;
  igl::cotmatrix(V0, F0, L_check);

  for (int k = 0; k < L_check.outerSize(); ++k) {
    for (SpMat::InnerIterator it(L_check, k); it; ++it) {
      int i = it.row();
      int j = it.col();

      double entry_igl_ij = it.value();
      double entry_L_ij = lap_mesh.L.coeffRef(i, j);

      EXPECT_NEAR(entry_L_ij, entry_igl_ij, tol);
    }
  }

  SpMat A_check;
  igl::massmatrix(V0, F0, igl::MASSMATRIX_TYPE_VORONOI, A_check);

  for (int k = 0; k < A_check.outerSize(); ++k) {
    for (SpMat::InnerIterator it(A_check, k); it; ++it) {
      int i = it.row();
      int j = it.col();

      double entry_igl_ij = it.value();
      double entry_A_ij = lap_mesh.A.coeffRef(i, j);

      EXPECT_NEAR(entry_A_ij, entry_igl_ij, tol);
    }
  }
}

TEST(AreaMatrixTest, CheckWithLibigl) {
  Eigen::SparseMatrix<double> M;
  igl::massmatrix(V0, F0, igl::MASSMATRIX_TYPE_VORONOI, M);

  for (int i = 0; i < test_mesh.primal_vertices.size(); ++i) {
    const double area = test_mesh.primal_vertices[i]->voronoi_area;
    const double igl_area = M.coeff(i, i);
    EXPECT_NEAR(area, igl_area, tol) << "mismatch at vertex " << i;
  }
}
