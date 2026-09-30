// Standard library
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// libigl
#include "igl/read_triangle_mesh.h"

// Polyscope / ImGui
#include "imgui.h"
#include "polyscope/point_cloud.h"
#include "polyscope/polyscope.h"
#include "polyscope/surface_mesh.h"

#include "LaplacianMesh.h"

namespace {

constexpr double kHeatTimeStep = 0.0001;
constexpr double kDistanceDiffusionTime = 0.0001;

Eigen::MatrixXd vertices;
Eigen::MatrixXi faces;

std::unique_ptr<LaplacianMesh> laplacianMesh;
polyscope::SurfaceMesh* surfaceMesh = nullptr;
polyscope::PointCloud* heatSourceCloud = nullptr;

std::vector<int> heatSourceIndices;
std::vector<Eigen::Vector3d> heatSourcePositions;
std::vector<std::filesystem::path> availableMeshes;
int activeMeshIndex = 0;
std::string meshLoadError;

double heatSimulationDuration = 0.001;
bool selectingHeatSources = false;

void clearResultVisualizations() {
  surfaceMesh->removeQuantity("Heat simulation", false);
  surfaceMesh->removeQuantity("Heat used for distance", false);
  surfaceMesh->removeQuantity("Geodesic distance", false);
}

void refreshHeatSourceCloud() {
  heatSourceCloud =
      polyscope::registerPointCloud("Heat sources", heatSourcePositions);
}

bool hasHeatSources() { return !heatSourceIndices.empty(); }

bool loadMesh(const std::filesystem::path& path) {
  Eigen::MatrixXd loadedVertices;
  Eigen::MatrixXi loadedFaces;
  if (!igl::read_triangle_mesh(path.string(), loadedVertices, loadedFaces)) {
    meshLoadError = "Could not load " + path.filename().string();
    return false;
  }

  if (surfaceMesh != nullptr) polyscope::removeSurfaceMesh("Mesh");
  polyscope::removePointCloud("Heat sources");

  vertices = std::move(loadedVertices);
  faces = std::move(loadedFaces);
  heatSourceIndices.clear();
  heatSourcePositions.clear();
  selectingHeatSources = false;

  laplacianMesh = std::make_unique<LaplacianMesh>(vertices, faces);
  surfaceMesh = polyscope::registerSurfaceMesh("Mesh", vertices, faces);
  surfaceMesh->setSelectionMode(polyscope::MeshSelectionMode::VerticesOnly);
  refreshHeatSourceCloud();
  meshLoadError.clear();
  std::cout << "Loaded mesh from " << path << '\n';
  return true;
}

void runHeatSimulation() {
  clearResultVisualizations();

  Eigen::VectorXd heat = Eigen::VectorXd::Zero(vertices.rows());
  laplacianMesh->setTimestep(kHeatTimeStep);
  laplacianMesh->setHeat(heatSourceIndices, heat);
  laplacianMesh->simulateHeatFlowForGivenTime(heat, heatSimulationDuration);

  auto* quantity =
      surfaceMesh->addVertexScalarQuantity("Heat simulation", heat);
  quantity->setEnabled(true);
}

void computeDistanceField() {
  clearResultVisualizations();

  // The complete heat-method pipeline starts from a fresh source field each time.
  Eigen::VectorXd heat = Eigen::VectorXd::Zero(vertices.rows());
  laplacianMesh->setTimestep(kHeatTimeStep);
  laplacianMesh->setHeat(heatSourceIndices, heat);
  laplacianMesh->simulateHeatFlowForGivenTime(heat, kDistanceDiffusionTime);

  const Eigen::MatrixXd gradient = laplacianMesh->computeGradient(heat);
  const Eigen::MatrixXd normalizedGradient =
      laplacianMesh->normalizeVectorfield(gradient);
  const Eigen::VectorXd divergence =
      laplacianMesh->computeDivergenceVectorField(normalizedGradient);
  const Eigen::VectorXd distance =
      laplacianMesh->computeGeodesicDistanceFunction(divergence);

  surfaceMesh->addVertexScalarQuantity("Heat used for distance", heat);
  auto* quantity =
      surfaceMesh->addVertexScalarQuantity("Geodesic distance", distance);
  quantity->setEnabled(true);
  quantity->setIsolinesEnabled(true);
}

void selectHeatSourceAtCursor() {
  ImGuiIO& io = ImGui::GetIO();
  if (!io.MouseClicked[0] || io.WantCaptureMouse) return;

  const glm::vec2 screenCoordinates{io.MousePos.x, io.MousePos.y};
  const polyscope::PickResult pick =
      polyscope::pickAtScreenCoords(screenCoordinates);
  if (!pick.isHit || pick.structure != surfaceMesh) return;

  const polyscope::SurfaceMeshPickResult meshPick =
      surfaceMesh->interpretPickResult(pick);
  if (meshPick.elementType != polyscope::MeshElement::VERTEX) return;

  const int vertexIndex = static_cast<int>(meshPick.index);
  for (int source : heatSourceIndices) {
    if (source == vertexIndex) return;
  }

  heatSourceIndices.push_back(vertexIndex);
  heatSourcePositions.emplace_back(vertices.row(vertexIndex));
  clearResultVisualizations();
  refreshHeatSourceCloud();
}

void callback() {
  ImGui::SetNextItemOpen(true, ImGuiCond_Once);
  if (ImGui::CollapsingHeader("Heat Geodesics", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Choose a mesh, select source vertices, then run an operation.");
    ImGui::SeparatorText("Mesh");
    ImGui::SetNextItemWidth(-1.0f);
    const std::string currentMeshName =
        availableMeshes[activeMeshIndex].filename().string();
    if (ImGui::BeginCombo("Loaded mesh", currentMeshName.c_str())) {
      for (int i = 0; i < static_cast<int>(availableMeshes.size()); ++i) {
        const bool selected = i == activeMeshIndex;
        const std::string meshName = availableMeshes[i].filename().string();
        if (ImGui::Selectable(meshName.c_str(), selected) && !selected) {
          if (loadMesh(availableMeshes[i])) activeMeshIndex = i;
        }
        if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    if (!meshLoadError.empty()) ImGui::TextColored(ImVec4(1.f, .4f, .4f, 1.f), "%s", meshLoadError.c_str());

    ImGui::SeparatorText("Sources");

    ImGui::Checkbox("Pick heat sources on the mesh", &selectingHeatSources);
    ImGui::SameLine();
    ImGui::TextDisabled("%zu selected", heatSourceIndices.size());
    if (selectingHeatSources) {
      ImGui::TextWrapped("Click vertices on the mesh. Clicking a selected vertex again has no effect.");
    }

    if (ImGui::Button("Clear sources")) {
      heatSourceIndices.clear();
      heatSourcePositions.clear();
      clearResultVisualizations();
      refreshHeatSourceCloud();
    }

    ImGui::SeparatorText("Distance field");
    ImGui::TextWrapped("Runs the heat method from the selected sources and displays the geodesic distance with isolines.");
    ImGui::BeginDisabled(!hasHeatSources());
    if (ImGui::Button("Compute distance field", ImVec2(-1.0f, 0.0f))) {
      computeDistanceField();
    }
    ImGui::EndDisabled();

    ImGui::SeparatorText("Heat simulation");
    ImGui::TextDisabled("Implicit Euler, fixed time step: 0.0001");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputDouble("Simulation duration", &heatSimulationDuration, 0.0, 0.0,
                       "%.4f");
    if (heatSimulationDuration < 0.0) heatSimulationDuration = 0.0;
    ImGui::BeginDisabled(!hasHeatSources());
    if (ImGui::Button("Run heat simulation", ImVec2(-1.0f, 0.0f))) {
      runHeatSimulation();
    }
    ImGui::EndDisabled();

    if (!hasHeatSources()) {
      ImGui::TextDisabled("Choose at least one source vertex to enable computations.");
    }
  }

  if (selectingHeatSources) selectHeatSourceAtCursor();
  surfaceMesh->setSelectionMode(polyscope::MeshSelectionMode::VerticesOnly);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <meshPath>\n";
    return 1;
  }

  const std::filesystem::path meshPath = argv[1];
  const std::filesystem::path projectPath = PROJECT_SOURCE_DIR;
  const std::filesystem::path fullPath =
      meshPath.is_absolute() ? meshPath : projectPath / meshPath;

  const std::filesystem::path dataDirectory = projectPath / "data";
  if (std::filesystem::is_directory(dataDirectory)) {
    for (const auto& entry : std::filesystem::directory_iterator(dataDirectory)) {
      if (!entry.is_regular_file()) continue;
      const std::string extension = entry.path().extension().string();
      if (extension == ".obj" || extension == ".off" || extension == ".ply" ||
          extension == ".stl") {
        availableMeshes.push_back(entry.path());
      }
    }
  }
  if (std::find(availableMeshes.begin(), availableMeshes.end(), fullPath) ==
      availableMeshes.end()) {
    availableMeshes.push_back(fullPath);
  }
  std::sort(availableMeshes.begin(), availableMeshes.end());
  activeMeshIndex = static_cast<int>(
      std::find(availableMeshes.begin(), availableMeshes.end(), fullPath) -
      availableMeshes.begin());

  polyscope::view::windowWidth = 1024;
  polyscope::view::windowHeight = 1024;
  polyscope::init();

  if (!loadMesh(fullPath)) {
    std::cerr << "Failed to load mesh: " << fullPath << '\n';
    return 1;
  }

  polyscope::state::userCallback = callback;
  polyscope::show();
  return 0;
}
