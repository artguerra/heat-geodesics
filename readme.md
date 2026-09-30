# Meshes
## TD 4 - A first 3D interface to manipulate mesh structures

### 1. Pointer Handling in C++


In this TP on 3D meshes, you will deepen your understanding of pointer management in C++. You’ll work with pointers referencing objects in a dynamic environment, where objects can be modified or deleted. Properly managing pointer updates in such scenarios will be essential for your implementation. By the end of this TD, if you approach it with patience and care, you should feel confident in handling these aspects. If you are not yet comfortable with creating pointers yourself, manage them, don't worry, we provided a little cheatsheet in the CPP-Recap folder for you. 

By the end of this TD you will have implemented your first real geometry processing research paper called "The Heat Method for Distance computations". The article can be found [[here](https://www.cs.cmu.edu/~kmcrane/Projects/HeatMethod/paper.pdf)]

![DistanceBunny ](imgs/bunnyDistance.jpg)




### 2. Geometric Modeling and Data Structures
From a geometric modeling perspective, we will explore two different data structures for triangle mesh manipulation:




- The Halfedge Structure:  
  This structure enables efficient local manipulation of a mesh, such as iterating around a vertex. It offers control over connectivity at the local level.




- Adjacency Matrix Representation:  
  This structure, used in libraries like LibIGL, provides a more global encoding of the mesh using adjacency matrices via vertex labels. While it makes local operations more challenging, such representation offers efficient global computations, which are useful in many applications.




Depending on the context, each structure has its advantages. As you work on this implementation, try to understand the underlying logic of these two connectivity representations. Although specific implementations may vary, the core principles remain consistent across different contexts.


---




## Important Notes




This TD is quite long, but each task is straightforward if you follow the instructions and review any missing C++ concepts. To ensure you have sufficient time, you will have until the day before TD7 to submit your work. The deadline is therefore 01/12/2025 23:59h.

Note that this is the only  **mandatory** TD and you cannot use your jokers on this TD. Its code will be used in TD6, TD8 and TD9, but it is independent of TD5 and TD7.
This TD is indeed long and challenging, but we believe in you that you can do it :)

We recommend you to start soon with it so that we can assist you if you need any help on the way. 

![DistanceBunny ](imgs/academicallyChallenge.jpg)



### 1. Data Structures
This TD has two main parts.  




In the first part, you will set up and implement a mesh data structure, including a half-edge structure. The `namespace MeshParts` contains the classes `Vertex`, `PrimalFace`, and `HalfEdge`. In the mesh class header, you’ll find `std::vector`s with shared pointers to vertices, half-edges, and faces.  Handling these pointers requires care, so we provide a C++ pointers overview in the `CPP-Recap` folder.


Regarding the constructor of the mesh, in the method initialize_complex you should allocate the memory for the needed mesh parts and create the pointers to these instances. This means you should create the shared pointers to all the vertices, primal faces and halfedges. You will see that the classes of the namespace MeshParts contain as class attributes itself std::weak_ptr to other mesh parts.
   
In the method compute_hedges you should now fill the half edge data-structure with life, meaning that you should set all the pointers properly for the mesh parts. The general workflow of what you will need edit in the file `Mesh.cpp` could look as follows: 

```cpp

void Mesh::initialize_complex()
{
    // Initialize the mesh complex
    // allocate the vertices, faces. Create the VertexPtr, PrimalFacePtr 
    //and fill the 
    //-----------Parts of the mesh with logic----------------
    //   std::vector<std::shared_ptr<Vertex>> primal_vertices;
    //   std::vector<std::shared_ptr<PrimalFace>> primal_faces;

    // Directly fill the attributes vertices_face for every face and one_ring_face for every vertex
}
void Mesh::compute_hedges()
{
    // --- 1. Loop over the primal faces. Allocate the halfedges--
    //  Fill the vector
    //   std::vector<std::shared_ptr<HalfEdge>> hedges;
    // Each halfedge has the following attributes:
    // std::weak_ptr<Vertex> start;
    // std::weak_ptr<Vertex> end;
    // std::weak_ptr<HalfEdge> flip;
    // std::weak_ptr<HalfEdge> next;
    // std::weak_ptr<HalfEdge> previous;
    // std::weak_ptr<PrimalFace> primal_face;

    // Directly in the first sweep over the faces you can set all pointers except the flip pointer. Also store directly on each primal face the halfedges and for each vertex the outgoing halfedges (you need to sort them later, but like that you have them already tied to the vertex)
    
    // --- 2. Setting the flip halfedge and boundary detection ---
    // You can use the libigl method 
    // Eigen::SparseMatrix<int> F_adjacency;
    // igl::facet_adjacency_matrix(F, F_adjacency);
    // in order to find for each primal face the adjacent faces. Use this information to find for every halfedge the flip halfedge.

    // Afterwards check which halfedges still do not have a flip halfedge assigned. Mark them as boundary halfedges. Also flag the associated vertices and faces as boundary vertices and faces.

   // Let's go! 
}

void Mesh::compute_geometry_for_mesh_parts()
{
    // Now the fun part starts. Now that you can navigate over your mesh, you need to calculate all the necessary geometric quantities.

    // This involves:
    // - The circumcenter and barycenter for each primal face
    // - The area for every primal face.
    // - The voronoi area and gaussian curvature for every vertex. 
    // - The normal for every vertex and every face
    // - The tip and tail angle for every halfedge. The cotangent for every angle opposite to the half edge
}

```
We highly recommend using the visual interface of polyscope to debug your construction, by clicking on different faces for instance and visualizing the corresponding indices, while checking their consistency. 




A rich set of tests is also provided for this TD, that you can use to test your implementation once you have all the data-structure nicely defined. The first part of the unittests will basically check whether you set all the pointers properly.


```cpp
TEST(HalfEdgeTest, CheckGoodInitialization) {
  [...]
  ASSERT_EQ(hedge->getFlipHalfEdge()->getStartVertex(), hedge->getEndVertex());
  ASSERT_EQ(hedge->getFlipHalfEdge()->getEndVertex(), hedge->getStartVertex());
  ASSERT_EQ(hedge->getFlipHalfEdge()->getFlipHalfEdge(), hedge);
  [...]
}
```

**Remark:**
Once you try to calculate the mesh datastructure for an open mesh, i.e a mesh with boundary, you need to be careful when you set the pointers for HalfEdges on the boundary. For your own sanity, you could add a condition in the method `HalfEdge::getFlipHalfEdge()` that will check (and throw a warning) if you try to call this method for a boundary half edge. In general, feel free to add more attributes or methods to the mesh parts if it helps you in your implementation.   

### 1.1 Vertex statistics
Once you are done with the constructor of the class `Mesh`, to get familiar with the new datastructure compute the degree of all vertices. Complete the method 
`vertexDegreeStatistics`.
You can use `std::chrono::high_resolution_clock::now()` to measure performance. Measure the performance if you only calculate the degree for a single vertex, rather then all of the vertices. 

### 1.2 Time Dependent Coloring
To change the color of the mesh over time, update the color in each frame. In `main.cpp` complete the method
```cpp
void updateColor(Eigen::VectorXd &C, double t = 0.0){
    C = Eigen::VectorXd::Zero(V.rows());    
    // TODO: Implement the time-dependent color update logic here
   
}
```
where you should implement a time dependant color function. Once you tick the checkbox `is animating` a boolean variable is set and you will find in the callback method
```cpp
if(is_animating){
    updateColor(Color,tt);
    polyscope::getSurfaceMesh("Mesh")->addVertexScalarQuantity("Color", Color)->setMapRange({-5.,5.});
    tt+=0.005;
}
```
You do not need to edit this method, but for your understanding what happens here is the following:




1) We load a mesh in the `main.cpp` with for example `igl::readOBJ` that converts into a `MatrixXd V` and `MatrixXi F` that contains the point positions and mesh information.




2) Next, we want to be able to visualize the mesh with Polyscope. In Polyscope you need to register an instance of a surface structure, this can be done with
```cpp
 polyscope::registerSurfaceMesh("Mesh", V, F);
```
Together with `polyscope::show();` this allows to show, with the help of the openGL backend, the mesh. Further, the method `polyscope::state::userCallback = callback;` will basically in each frame check if there is some change on the mesh or related information and walk through the `callback` function. This is where it will find the aforementioned lines for the update of the color  

In your tests experiment with the function `update_color`.


### 1.3 Normal Computation
The goal is to compute the normals using different methods. In the class `Mesh` you will find the methods
```cpp
Eigen::MatrixXd compute_vertex_normals();
Eigen::MatrixXd compute_vertex_normals_hed();

Eigen::MatrixXd compute_face_normals();
Eigen::MatrixXd compute_face_normals_hed();
```
where you should calculate the vertex and face normals using the face-based datastructure and the half-edge datastructure.
When you call the method `compute_vertex_normals_hed()` and `compute_face_normals_hed` write the normal attribute in a single matrix that we can pass to polyscope using the information calculated in the mesh constructor.

For the methods `compute_vertex_normals()` and `compute_face_normals()`, compute the normals again, but this time only with the face based datastructure given by libigl, i.e the only information you are allowed to use is `V` and `F`. 
Once they are implemented you can check with the gui and the button `Compute Normals` whether your normal field looks good. You can visualize the results of both implementations via the interface options as in this figure. There is a checkbox where you can choose whether you want to use the halfedge datastructure or the face based datastructure.

![Normal Computation](imgs/normals_bunny.jpg)

### 1.4 Number of Boundaries
Compute the number of boundary components in the mesh (complete `countBoundaries`).

### 1.5 Gaussian Curvature Computation
We will use the half edge data structure to compute the Gaußian curvature at each vertex. Complete the function `compute_gaussian_curvature`. Again, you can use the information that you already computed in the constructor of the `Mesh` class. Normalize based on the area of the dual (Voronoi) cell. You can visualize the Gaußian curvature as a scalar-valued function by clicking on the corresponding button.

![Curvature Computation](imgs/torus_curvature.jpg)

## 2. Mesh Operators


As you have seen in class, the Laplace-Beltrami operator on a mesh is defined as a matrix $\Delta = A^{-1}L \in \mathbb{R}^{\mathcal{V}\times\mathcal{V}}$, where




$$ L_{ij} = \begin{cases} \frac{1}{2}( \text{cot}(\alpha_{ij})+\text{cot}(\beta_{ij})) & \text{if } i \in \mathcal{N}(j)\\ -\sum_{k \in \mathcal{N}(i),\ k\neq i} L_{ik} & \text{if } i = j \\ 0 & \text{if } i \notin \mathcal{N}(j) \end{cases}$$
and $A = \mathrm{diag}((\mathrm{Voronoi-Area})_i)$.
We will now in the following build on top of the mesh datastructure that you implemented before also construct the Laplace-Beltrami operator. We construct therefore a novel class `LaplacianMesh` that inherits from the class `Mesh`.
#### Sparse matrices in Eigen
You will notice that for an ordinary mesh, almost all entries of the Laplace-matrix are zero. Therefore, instead of storing a huge matrix that contains mainly zeros, we will instead use a datastructure, where we only store the non-zero entries. These matrices are called Sparse Matrices. You will find in the file `LaplacianMesh.h` a `typedef Eigen::SparseMatrix<double> SpMat;`, meaning that the Eigen Sparse matrices whose entries are `double` can be identified with this call. A sparse matrix with double values can be declared through Eigen::SparseMatrix<double>. In order to create such a matrix we need to create Triplets
`Eigen::Triplet<double>(row, column, value)` and stack them in a vector. Then we can use the method `setFromTriplets` to fill the sparse matrix. For these kind of matrices, Eigen has special efficient in-build solvers that we are going to use in the following.




Complete the methods needed for the constructor of the class `LaplacianMesh`. All manipulations from now on need to be carried out in the file `LaplacianMesh.cpp`.:w



To verify your implementation, you will now implement a UnitTest comparing your results against the pre-build methods from Libigl.


#### Unit Tests with the Googletest Library - Step by Step Guide

In previous exercises, we've introduced the concept of unit testing. For this, we use the Googletest library. The basic idea is to create a separate executable that runs a series of tests, checking whether the actual outcomes match expected results defined at the start. In the `CMakeLists.txt`, you’ll find the following lines:

```cpp
# === Set up the googletest to test the mesh library ===

include(googletest)

enable_testing()

add_executable(${PROJECT_NAME}_test
    tests/unittest.cpp
)
target_include_directories(${PROJECT_NAME}_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME}_test
    PRIVATE
        MeshLib
        igl::core
        GTest::gtest_main
)

target_compile_definitions(${PROJECT_NAME}_test
PRIVATE 
PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
add_test(NAME ${PROJECT_NAME}_test
    COMMAND ${PROJECT_NAME}_test
)
```

This configuration links the test executable to both our custom library (e.g., mesh files) and the Googletest library. However, note that other libraries, like Polyscope, may not be needed for verifying certain functionalities for mesh correctness. While it’s possible to include the Polyscope header without errors during compilation, issues will arise during the linking phase, causing the build process to fail. This is because the linker won’t be able to resolve the necessary references.

You will now check, whether your implementation of the matrix `L` matches the pre-computed cotangent matrix of Libigl. Therefore, go to the file `unittests.cpp` and include the header `#include "igl/cotmatrix.h"` Also, include the header for your class `LaplacianMesh.h` in the `unittests.cpp`. Next, inside the test file create an instance of your class `LaplacianMesh`. For example 
`LaplacianMesh LapMesh= LaplacianMesh(V0,F0);`

Next, inside the Test for the cotangent matrix call the Libigl method for the construction of the cotangent matrix. For example

```cpp
TEST(LaplacianTest,CheckWithLibigl) {
  SpMat L_check;
  igl::cotmatrix(V0,F0,L_check);
}
```

You have now created the test values for your matrix L. Next, we will iterate over the non-zero entries of matrix L and verify that they match the precomputed values. The goal is to iterate over the non-zero elements of your sparse matrix and compare them with the corresponding entries in the libigl matrix. This can be done as follows:

```cpp
 for (int k = 0; k < L_check.outerSize(); ++k) {
      for (Eigen::SparseMatrix<double>::InnerIterator it(L_check, k); it; ++it) {
          int i = it.row();
          int j = it.col();
          double entry_libigl_ij = it.value();
          double your_entry_ij = LapMesh.L.coeffRef(i,j);
      }
    }

```

Now, that you have your value and the comparison value, you can indeed do the googletest assertion. There are multiple checks that can be done. A full overview can be found [here](https://google.github.io/googletest/reference/assertions.html). For the test here, we want to check if two double values match. Here, for instance the assertion `EXPECT_NEAR` is what you want. The full example could look as follows

```cpp
#include <LaplacianMesh.h>
LaplacianMesh LapMesh = LaplacianMesh(V0,F0);
TEST(LaplacianTest,CheckWithLibigl) {
  SpMat L_check;
  igl::cotmatrix(V0,F0,L_check);
  for (int k = 0; k < L_check.outerSize(); ++k) {
    for (Eigen::SparseMatrix<double>::InnerIterator it(L_check, k); it; ++it) {
      int i = it.row();
      int j = it.col();
      double entry_libigl_ij = it.value();
      double your_entry_ij = LapMesh.L.coeffRef(i,j);
      EXPECT_NEAR(entry_libigl_ij,your_entry_ij,1e-8);
      
    }
  }
}

```

Now, repeat the same for the second test with the area matrix, you can use `igl::massmatrix(V0, F0, igl::MASSMATRIX_TYPE_VORONOI, A_check);` to compare.



### 2.1 The Heat Flow
The heat equation is given by:


$$ \frac{\partial u}{\partial t} = \Delta u $$


Given the heat at time $u^{k}$, you can calculate heat at time $u^{k+1}$ using:




Explicit integration: $u^{k+1} = dt \cdot A^{-1} \cdot L \cdot u^{k} + u^{k}$


Semi-implicit integration: $(A - dt \ L) u^{k+1} = A u^{k}$


Implement both in `heat_step_explicit` and `heat_step_implicit`. Note that for the implicit Euler, you will need to solve in each step a Linear System. Fortunately Eigen provides us with a set of tailored sparse solvers.
The steps to use these solvers is as follows, illustrated in the example of a Cholesky solver. 

You can set the heat sources with the user interface as follows:

<video width="640" height="360" controls>
  <source src="imgs/selectSources.mp4" type="video/mp4">
  Your browser does not support the video tag.
</video>


The class `LaplacianMesh`, that inherits from the class `Mesh`, has already as an attribute a Cholesky solver. Once you set the initial time step you can pre-factorize the integrator matrix. Note that this is also an excellent test to see if everything went well so far, if the matrix is not symmetric positive definite, the solver is going to break. 
```cpp
 Eigen::SimplicialLDLT<SpMat> solverForBackwardsEuler;
```

As a note, we recommend you to first analyze the sparsity pattern of your integrator matrix in the methofd `compute_laplacian`, and then carry out the factorization. This has the advantage that if you change the time step during your simulation, the factorization can be carried out much more efficiently.  

```cpp
void LaplacianMesh::compute_laplacian() {
  // Delta.resize(V.rows(),V.rows());
  Delta = SpMat(V.rows(), V.rows());
  Ainv = A.cwiseInverse();
  Delta = Ainv * L;
  integratorMatrix = SpMat(V.rows(), V.rows());
  integratorMatrix = A - timeStep * L;
  // Here in this step we let Eigen do the symbolic factorization. 
  // Basically we analyze the sparsity pattern of the matrix. 
  // If we change the entries of the matrix, like when we change the timestep, we can make use of this information and just re-factorize much faster. This is what we do with factorize();
  this->solverForBackwardsEuler.analyzePattern(integratorMatrix);
  this->setTimestep(this->timeStep);
}
void LaplacianMesh::setTimestep(double t) {
  this->timeStep = t;
  SpMat tL = timeStep * L;
  integratorMatrix = A - tL;
  this->solverForBackwardsEuler.factorize(integratorMatrix);
  if (this->solverForBackwardsEuler.info() != Eigen::Success) {
    throw std::runtime_error("Decomposition failed");
  } else {
    std::cout << "Decomposition successfull" << std::endl;
  }
}
```

The implicit heat step can now be carried out as follows

```cpp
void LaplacianMesh::heat_step_implicit(Eigen::VectorXd &u) {
  // TODO
  Eigen::VectorXd RHS = A * u;
  u = this->solverForBackwardsEuler.solve(RHS);
}
```

 An overview can be found [here](https://eigen.tuxfamily.org/dox/group__TopicSparseSystems.html) and depending on the structure of your linear system, different solvers might be appropriate. 

 Visualize heat flow by modifying the animation loop. Experiment how big you can choose the step size of your update scheme to get a stable solution.


![heat flow Computation](imgs/heat_bunny.jpg)

### For you to do:
To this point you should have computed the following, where of course a lot of previously calculated information can be recycled.
```cpp
    SpMat L; //cotangent
    SpMat A; //normalization for the laplacian
    SpMat Ainv; //inverse of A for performance
    SpMat Delta; //laplacian
    void setTimestep(double t);
    void heat_step_explicit(Eigen::VectorXd & u);
    void heat_step_implicit(Eigen::VectorXd & u);    
    void simulateHeatFlowForGivenTime(Eigen::VectorXd & u,double duration); // simulate the heat flow until the passed time
```

# 3. The Heat Method for Distance computations


Now that you can simulate the heat for a certain time, it is time to implement the first real research article. 

An excellent explanation of what you have to do can be found here 

[![teaser talk](imgs/thumbnail.jpg)](https://www.youtube.com/watch?v=Dgu-V9ciGi8&t=1s)

I highly recommend to check the video and the linked article in the enonce out. It is not just done nicely, but will also help you to understand what is going to happen in the following.

The overall idea is that distance computation on a surface is challenging. The idea is to use a heatflow for a fixed time and measure the heat at every point of the surface. Before we get in the details, think about it, that it makes sense indeed to use heat to measure distance. Look for instance at the bunny and the heat distribution. In some sense this resembles actually pretty well some notion of "distance away from the source"


Given a source point, the aim will be to find a scalar valued function $\phi$ such that the the gradient of the function satisfies $|\nabla \phi | =1$ and $\phi(\mathrm{source}) = 0$.
To quote the paper " Intuitively, this equation says something very simple: as we move away from the
source, the distance function $\phi$ must change at a rate of “one
meter per meter.” This equation is called the Eikonal equation.

How can we find this function? We can simply follow the algorithm provided in the paper.

1. Integrate the heatflow for a fixed time $\dot{u} = \Delta u$

You have done that already !

 2. Evaluate the vector field $X = -\frac{\nabla u_t}{|\nabla u_t|}$. 

 This involves for you to compute the discrete gradient of a scalar valued function. Here the scalar valued function is the heat at a certain time and is represented as one scalar per vertex. The gradient will be one vector per primal face. To quote the paper "The gradient in a given triangle can be expressed as

 $$(\nabla u)_f = \frac{1}{2 A_f} \sum_{i} u_i (N\times e_i) $$
![grad Computation](imgs/gradCalculation.png)

Compute this in c++ in the method
```cpp
    Eigen::MatrixXd computeGradient(const Eigen::VectorXd& u);
```
in the `LaplacianMesh`
where you give in the vector $u$ defined as scalar per vertex and return a matrix with $|F|$ rows and 3 columns that carry the gradient per face. After you simulate the heat flow for a certain time, you can click on the button `compute gradient of heat`.
You will now see in the menu on the left a quantity called `gradient`. Enable it and you will see the gradient vector field.

![grad Computation](imgs/enableQuantity.png)

In the next step you need to calculate the normalized gradient that points away from the source, i.e compute 
$X = -\frac{\nabla u_t}{|\nabla u_t|}$. 
You can do this in the method 
```cpp
    Eigen::MatrixXd normalizeVectorfield(const Eigen::MatrixXd& gradU);
```
Again, you can check with pressing the button `Normalize the gradient of Heat`. You should see a new vector quantity in the menu. When you enable it, you will see a field like that:

![grad Computation](imgs/bunnyFlippedGradient.jpg)

Now, it is time to prepare for step 3 and the solve of the distance function. Now, we have 
$|X|=1$. Thus, ideally we would like to find a function $\phi$ such that $X = \mathrm{grad}(\phi)$. 

You may remember the following identity from vector calculus that $\mathrm{div}(\mathrm{grad}(\phi)) = \Delta \phi$. Hence, in order to find the function $\phi$, we will solve the equation 
$$\mathrm{div}(X) = \Delta \phi$$
The discrete divergence of a vector field is according to the paper given as follows:

$$\mathrm{div}_v(X) = \frac{1}{2} \sum_{j} \mathrm{cot}(\theta_1) (e_1\cdot X_j) + \mathrm{cot}(\theta_2) (e_2\cdot X_j)$$
This is the integrated divergence, however we will compute the pointwose divergence, i.e the divergence normalized by the Voronoi area. This means
$$\mathrm{div}_v(X) = (\frac{1}{2} \sum_{j} \mathrm{cot}(\theta_1) (e_1\cdot X_j) + \mathrm{cot}(\theta_2) (e_2\cdot X_j))\frac{1}{A_v}$$


![Curvature Computation](imgs/divCalculation.png)

Here all your hard work to this point comes in nicely, because all these quantities can now be easily calculated by a local traversal with your half edge datastructure. 
Calculate the divergence in the method `Eigen::VectorXd computeDivergenceVectorField(const Eigen::MatrixXd& vectorField);`

Again, you can verify this by clicking the button on the right in the gui. 

Now, as a last step you can now solve for the potential. You have to solve the poisson system 
$$\mathrm{div}(X) = \Delta \phi$$

Note that the cotangent matrix is positive demi-definite. Therefore instead of solving 
$$\mathrm{div}(X) = A^{-1} L \phi$$
we solve 
$$L\phi = A\  \mathrm{div}(X)$$

Note that there is some code left, that is needed to ensure that the system we are trying to solve is actually solvable. We know that from a mathematical perspective they should, but with the calculation of the gradient and the divergence, there may be numerical noise. Therefore a little surgery is needed, but for now you don't need to worry about the details. In the method you need to create the Cholesky solver for the Poisson system, then we do the surgery for you, next you need to prepare the right hand side of the system and lastly you need to solve for the distance function.
```cpp
Eigen::VectorXd LaplacianMesh::computeGeodesicDistanceFunction(
    const Eigen::VectorXd &divergenceEikonal)
{
  // TODO Create the Cholesky factorization of the cotangent matrix
  

  //--------------------------------------------------
  // DO NOT REMOVE: Important surgery to avoid numerical issues.
  //--------------------------------------------------

  ...
  //--------------------------------------------------
  // Numerical surgery end
  //--------------------------------------------------

  // TODO: Build the RHS from the repaired divergenceEikonalCopy
  // Solve for the geodesig distance function

  // now look at the value at the sources
  Eigen::VectorXd potential;
  return potential;
}
```

Note that we solve for a function whose gradient should satisfy the Eikonal equation. The gradient is a differential operator, therefore shifting the potential in the end by a constant will not change the gradient. Therefore you may get something like -12 as a distance. The distance between the source and any other point is now the difference in the potential values. But if you enable the iso lines you can already see the equidistant lines from your source vertex :) 

Voila! Have fun and good luck! 

![Curvature Computation](imgs/distances.png)
Play with the color code, truncate the range and attach a snapshot to your solution.



