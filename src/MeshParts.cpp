#include "MeshParts.h"
#include <iomanip> // For std::setprecision
#include <iostream>
#include "Mesh.h"

using Eigen::Matrix2d, Eigen::MatrixXd, Eigen::Vector2d, Eigen::Vector3d;

std::vector<double> MeshParts::PrimalFace::getBarycentricCoordinates(Eigen::Vector3d p)
{
    Eigen::Vector3d v0 = this->getVertices()[0]->position;
    Eigen::Vector3d v1 = this->getVertices()[1]->position;
    Eigen::Vector3d v2 = this->getVertices()[2]->position;

    Eigen::Vector3d v0v1 = v1 - v0, v0v2 = v2 - v0, vp = p - v0;

    double d00 = v0v1.dot(v0v1);
    double d01 = v0v1.dot(v0v2);
    double d11 = v0v2.dot(v0v2);
    double d20 = vp.dot(v0v1);
    double d21 = vp.dot(v0v2);

    double denom = d00 * d11 - d01 * d01;

    double v = (d11 * d20 - d01 * d21) / denom;
    double w = (d00 * d21 - d01 * d20) / denom;
    double u = 1.0 - v - w;

    return {u, v, w};
}

//--------------------------------------------------
//-----------------Vertex Class---------------------
//--------------------------------------------------
void MeshParts::Vertex::print()
{
    std::cout << "position of vertex" << this->index << " is \n"
              << this->position << std::endl;
}

bool MeshParts::Vertex::operator==(const Vertex &other) const
{
    return (((position == other.position) && (index == other.index)));
}

//--------------------------------------------------
//-----------------PrimalFace Class-----------------
//--------------------------------------------------
void MeshParts::PrimalFace::print()
{
    std::cout << "vertices of face " << this->index << " are " << this->vertices_face[0].lock()->index
              << " " << this->vertices_face[1].lock()->index << " " << this->vertices_face[2].lock()->index
              << std::endl;
}

bool MeshParts::PrimalFace::operator==(const PrimalFace &other) const
{
    return (this->index == other.index);
}

//--------------------------------------------------
//-----------------HalfEdge Class-------------------
//--------------------------------------------------
void MeshParts::HalfEdge::print()
{
    std::cout << "start: " << this->getStartVertex()->index
              << " end: " << this->getEndVertex()->index << std::endl;
}

bool MeshParts::HalfEdge::operator==(const HalfEdge &other) const
{
    return (
        (index == other.index) && (sign_edge == other.sign_edge) &&
        (this->getIndexOfStartVertex() == other.getIndexOfStartVertex()) &&
        (this->getIndexOfEndVertex() == other.getIndexOfEndVertex()) &&
        (this->getIndexOfFlipHalfEdge() == other.getIndexOfFlipHalfEdge()) &&
        (this->getIndexOfNextHalfEdge() == other.getIndexOfNextHalfEdge()) &&
        (this->getPrimalFace()->index == other.getPrimalFace()->index));
}
