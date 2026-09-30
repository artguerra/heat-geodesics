#include <memory>
#include <string>
#include <vector>

#include <emscripten/emscripten.h>

#include <Eigen/Core>
#include <igl/read_triangle_mesh.h>

#include "LaplacianMesh.h"

namespace {

constexpr double kHeatTimeStep = 0.0001;
constexpr double kDistanceDiffusionTime = 0.0001;

Eigen::MatrixXd vertices;
Eigen::MatrixXi faces;
Eigen::VectorXd heatField;
Eigen::VectorXd distanceField;
std::vector<int> sourceIndices;
std::unique_ptr<LaplacianMesh> laplacianMesh;
std::string lastError;

bool ready() {
  if (laplacianMesh) return true;
  lastError = "Load a mesh first.";
  return false;
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int app_load_mesh(const char* path) {
  Eigen::MatrixXd loadedVertices;
  Eigen::MatrixXi loadedFaces;
  if (path == nullptr ||
      !igl::read_triangle_mesh(path, loadedVertices, loadedFaces)) {
    lastError = "The selected mesh could not be loaded.";
    return 0;
  }

  vertices = std::move(loadedVertices);
  faces = std::move(loadedFaces);
  sourceIndices.clear();
  heatField = Eigen::VectorXd::Zero(vertices.rows());
  distanceField = Eigen::VectorXd::Zero(vertices.rows());
  laplacianMesh = std::make_unique<LaplacianMesh>(vertices, faces);
  lastError.clear();
  return 1;
}

EMSCRIPTEN_KEEPALIVE void app_clear_sources() {
  sourceIndices.clear();
  heatField.setZero();
  distanceField.setZero();
}

EMSCRIPTEN_KEEPALIVE int app_add_source(int vertexIndex) {
  if (!ready() || vertexIndex < 0 || vertexIndex >= vertices.rows())
    return 0;
  for (int source : sourceIndices) {
    if (source == vertexIndex) return 1;
  }
  sourceIndices.push_back(vertexIndex);
  heatField.setZero();
  distanceField.setZero();
  return 1;
}

EMSCRIPTEN_KEEPALIVE int app_source_count() {
  return static_cast<int>(sourceIndices.size());
}

EMSCRIPTEN_KEEPALIVE int app_vertex_count() {
  return static_cast<int>(vertices.rows());
}

EMSCRIPTEN_KEEPALIVE int app_face_count() {
  return static_cast<int>(faces.rows());
}

EMSCRIPTEN_KEEPALIVE double app_vertex_coordinate(int vertexIndex, int axis) {
  return vertices(vertexIndex, axis);
}

EMSCRIPTEN_KEEPALIVE int app_face_index(int faceIndex, int corner) {
  return faces(faceIndex, corner);
}

EMSCRIPTEN_KEEPALIVE int app_is_source(int vertexIndex) {
  for (int source : sourceIndices) {
    if (source == vertexIndex) return 1;
  }
  return 0;
}

EMSCRIPTEN_KEEPALIVE int app_run_heat(double duration) {
  if (!ready() || sourceIndices.empty()) {
    lastError = "Select at least one source vertex.";
    return 0;
  }
  if (duration < 0.0) {
    lastError = "Simulation duration must be non-negative.";
    return 0;
  }
  heatField = Eigen::VectorXd::Zero(vertices.rows());
  distanceField.setZero();
  laplacianMesh->setTimestep(kHeatTimeStep);
  laplacianMesh->setHeat(sourceIndices, heatField);
  laplacianMesh->simulateHeatFlowForGivenTime(heatField, duration);
  lastError.clear();
  return 1;
}

EMSCRIPTEN_KEEPALIVE int app_compute_distance() {
  if (!ready() || sourceIndices.empty()) {
    lastError = "Select at least one source vertex.";
    return 0;
  }

  heatField = Eigen::VectorXd::Zero(vertices.rows());
  distanceField.setZero();
  laplacianMesh->setTimestep(kHeatTimeStep);
  laplacianMesh->setHeat(sourceIndices, heatField);
  laplacianMesh->simulateHeatFlowForGivenTime(heatField, kDistanceDiffusionTime);

  const Eigen::MatrixXd gradient = laplacianMesh->computeGradient(heatField);
  const Eigen::MatrixXd normalizedGradient =
      laplacianMesh->normalizeVectorfield(gradient);
  const Eigen::VectorXd divergence =
      laplacianMesh->computeDivergenceVectorField(normalizedGradient);
  distanceField = laplacianMesh->computeGeodesicDistanceFunction(divergence);
  lastError.clear();
  return 1;
}

EMSCRIPTEN_KEEPALIVE double app_field_value(int vertexIndex, int distance) {
  return distance ? distanceField(vertexIndex) : heatField(vertexIndex);
}

EMSCRIPTEN_KEEPALIVE const char* app_last_error() {
  return lastError.c_str();
}

}  // extern "C"
