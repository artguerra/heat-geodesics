cmake_minimum_required(VERSION 3.22)

project(
    geodesicInHeat
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
  add_compile_options(/bigobj)
  add_compile_definitions(_USE_MATH_DEFINES)
endif()

# ------------------------------------------------------------------------------
# dependencies
# ------------------------------------------------------------------------------

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

CPMAddPackage(
  NAME libigl
  GITHUB_REPOSITORY libigl/libigl
  GIT_TAG 788871103938ce92112714d11ea491e8e6b4d972
)

if(NOT EMSCRIPTEN)
  CPMAddPackage(
    NAME polyscope
    GITHUB_REPOSITORY nmwsharp/polyscope
    GIT_TAG v2.6.1
  )
endif()

# ------------------------------------------------------------------------------
# mesh library
# ------------------------------------------------------------------------------

add_library(MeshLib
  src/Mesh.cpp
  src/MeshParts.cpp
  src/LaplacianMesh.cpp
)

target_include_directories(MeshLib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(MeshLib PUBLIC igl::core)

# ------------------------------------------------------------------------------
# application
# ------------------------------------------------------------------------------

if(EMSCRIPTEN)
  add_executable(${PROJECT_NAME}_web web/web_main.cpp)

  target_link_libraries(${PROJECT_NAME}_web PRIVATE MeshLib)

  target_link_options(${PROJECT_NAME}_web PRIVATE
    "-sALLOW_MEMORY_GROWTH=1"
    "-sNO_EXIT_RUNTIME=1"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=HeatGeodesicsModule"
    "-sENVIRONMENT=web"
    "-sEXPORTED_RUNTIME_METHODS=['cwrap']"
    "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/data/bunny_fine.off@/data/bunny_fine.off"
    "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/data/bunny_lowpoly.off@/data/bunny_lowpoly.off"
    "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/data/torus.off@/data/torus.off"
    "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
  )

  set_target_properties(${PROJECT_NAME}_web PROPERTIES
    OUTPUT_NAME "index"
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/site"
    LINK_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html;${CMAKE_CURRENT_SOURCE_DIR}/data/bunny_fine.off;${CMAKE_CURRENT_SOURCE_DIR}/data/bunny_lowpoly.off;${CMAKE_CURRENT_SOURCE_DIR}/data/torus.off"
  )

  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/site")
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/app.js"
    "${CMAKE_BINARY_DIR}/site/app.js"
    COPYONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/web/.nojekyll"
    "${CMAKE_BINARY_DIR}/site/.nojekyll"
    COPYONLY
  )
else()
  add_executable(${PROJECT_NAME} main.cpp)

  target_link_libraries(${PROJECT_NAME} PRIVATE MeshLib polyscope)

  target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

# ------------------------------------------------------------------------------
# tests
# ------------------------------------------------------------------------------

include(CTest)

if(BUILD_TESTING AND NOT EMSCRIPTEN)

  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  CPMAddPackage(
    NAME googletest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.16.0
    OPTIONS
        "BUILD_GMOCK OFF"
        "INSTALL_GTEST OFF"
  )

  add_executable(${PROJECT_NAME}_test tests/unittest.cpp)

  target_include_directories(${PROJECT_NAME}_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

  target_link_libraries(${PROJECT_NAME}_test
    PRIVATE
      MeshLib
      igl::core
      GTest::gtest_main
  )

  target_compile_definitions(${PROJECT_NAME}_test PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

  add_test(
    NAME ${PROJECT_NAME}_test
    COMMAND ${PROJECT_NAME}_test
  )

endif()
