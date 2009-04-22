// Copyright (c) 2007-09  INRIA (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you may redistribute it under
// the terms of the Q Public License version 1.0.
// See the file LICENSE.QPL distributed with CGAL.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
//
//
// Author(s)     : Laurent Saboret, Pierre Alliez


#ifndef CGAL_POISSON_RECONSTRUCTION_FUNCTION_H
#define CGAL_POISSON_RECONSTRUCTION_FUNCTION_H

#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

#include <CGAL/Reconstruction_triangulation_3.h>
#include <CGAL/spatial_sort.h>
#include <CGAL/taucs_solver.h>
#include <CGAL/centroid.h>
#include <CGAL/surface_reconstruction_points_assertions.h>
#include <CGAL/Memory_sizer.h>
#include <CGAL/Peak_memory_sizer.h>
#include <CGAL/poisson_refine_triangulation.h>

CGAL_BEGIN_NAMESPACE


/// Kazhdan, Bolitho and Hoppe introduced the Poisson Surface Reconstruction algorithm [Kazhdan06].
/// Given a set of 3D points with oriented normals sampled on the boundary of a 3D solid,
/// this method solves for an approximate indicator function
/// of the inferred solid, whose gradient best matches the input normals.
/// The output scalar function, represented in an adaptive octree, is then iso-contoured
/// using an adaptive marching cubes.
///
/// Poisson_reconstruction_function implements a variant of this algorithm which solves
/// for a piecewise linear function on a 3D Delaunay triangulation instead of an adaptive octree
/// the TAUCS sparse linear solver.
/// In order to get a unique solution, one vertex outside of the surface is constrained to a value of 0.0.
///
/// @heading Is Model for the Concepts:
/// Model of the 'ImplicitFunction' concept.
///
/// @heading Parameters:
/// @param Gt Geometric traits class.
/// @param ReconstructionTriangulation_3 3D Delaunay triangulation,
///        model of ReconstructionTriangulation_3 concept.

template <class Gt, class ReconstructionTriangulation_3>
class Poisson_reconstruction_function
{
// Public types
public:

  typedef ReconstructionTriangulation_3 Triangulation;

  typedef Gt Geom_traits; ///< Kernel's geometric traits

  typedef typename Geom_traits::FT FT;
  typedef typename Geom_traits::Point_3 Point; ///< == Point_3<Gt>
  typedef typename Geom_traits::Vector_3 Vector; ///< == Vector_3<Gt>
  typedef typename Geom_traits::Sphere_3 Sphere;

  typedef typename Triangulation::Point_with_normal Point_with_normal;
                                                     ///< Model of PointWithNormal_3
  typedef typename Point_with_normal::Normal Normal; ///< Model of Kernel::Vector_3 concept.

// Private types
private:

  // Repeat ReconstructionTriangulation_3 types
  typedef typename Triangulation::Triangulation_data_structure Triangulation_data_structure;
  typedef typename Geom_traits::Ray_3 Ray;
  typedef typename Geom_traits::Plane_3 Plane;
  typedef typename Geom_traits::Segment_3 Segment;
  typedef typename Geom_traits::Triangle_3 Triangle;
  typedef typename Geom_traits::Tetrahedron_3 Tetrahedron;
  typedef typename Triangulation::Cell_handle   Cell_handle;
  typedef typename Triangulation::Vertex_handle Vertex_handle;
  typedef typename Triangulation::Cell   Cell;
  typedef typename Triangulation::Vertex Vertex;
  typedef typename Triangulation::Facet  Facet;
  typedef typename Triangulation::Edge   Edge;
  typedef typename Triangulation::Cell_circulator  Cell_circulator;
  typedef typename Triangulation::Facet_circulator Facet_circulator;
  typedef typename Triangulation::Cell_iterator    Cell_iterator;
  typedef typename Triangulation::Facet_iterator   Facet_iterator;
  typedef typename Triangulation::Edge_iterator    Edge_iterator;
  typedef typename Triangulation::Vertex_iterator  Vertex_iterator;
  typedef typename Triangulation::Point_iterator   Point_iterator;
  typedef typename Triangulation::Finite_vertices_iterator Finite_vertices_iterator;
  typedef typename Triangulation::Finite_cells_iterator    Finite_cells_iterator;
  typedef typename Triangulation::Finite_facets_iterator   Finite_facets_iterator;
  typedef typename Triangulation::Finite_edges_iterator    Finite_edges_iterator;
  typedef typename Triangulation::All_cells_iterator       All_cells_iterator;
  typedef typename Triangulation::Locate_type Locate_type;

  // TAUCS solver
  typedef Taucs_solver<double>  Solver;
  typedef std::vector<double>   Sparse_vector;

// Data members.
// Warning: the Surface Mesh Generation package makes copies of implicit functions,
// thus this class must be lightweight and stateless.
private:

  Triangulation& m_tr; // f() is pre-computed on vertices of m_tr by solving
                       // the Poisson equation Laplacian(f) = divergent(normals field).

  // contouring and meshing
  Point m_sink; // Point with the minimum value of f()
  mutable Cell_handle m_hint; // last cell found = hint for next search

// Public methods
public:

  /// Creates a scalar function from a set of oriented points.
  /// Inserts the iterator range [first, beyond) into the triangulation 'tr',
  /// refines it and solves for a piecewise linear scalar function
  /// which gradient best matches the input normals.
  ///
  /// If 'tr' is empty, this method creates an empty implicit function.
  ///
  /// @param tr ReconstructionTriangulation_3 base of the Poisson indicator function.
  Poisson_reconstruction_function(ReconstructionTriangulation_3& tr)
  : m_tr(tr)
  {
  }

  /// Creates a scalar function from a set of oriented points.
  /// Inserts the iterator range [first, beyond) into the triangulation 'tr',
  /// refines it and solves for a piecewise linear scalar function
  /// which gradient best matches the input normals.
  ///
  /// @commentheading Precondition:
  /// InputIterator value_type must be convertible to Point_with_normal.
  ///
  /// @param tr ReconstructionTriangulation_3 base of the Poisson indicator function.
  /// @param first Iterator over first point to add.
  /// @param beyond Past-the-end iterator to add.
  template < class InputIterator >
  Poisson_reconstruction_function(ReconstructionTriangulation_3& tr,
                                  InputIterator first, InputIterator beyond)
  : m_tr(tr)
  {
    insert(first, beyond);
  }

  /// Insert points.
  ///
  /// @commentheading Precondition:
  /// InputIterator value_type must be convertible to Point_with_normal.
  ///
  /// @param first Iterator over first point to add.
  /// @param beyond Past-the-end iterator to add.
  /// @return the number of inserted points.
  template < class InputIterator >
  int insert(InputIterator first, InputIterator beyond)
  {
    return m_tr.insert(first, beyond);
  }

  /// Remove all points.
  void clear()
  {
    m_tr.clear();
  }

  /// Get embedded triangulation.
  ReconstructionTriangulation_3& triangulation()
  {
    return m_tr;
  }
  const ReconstructionTriangulation_3& triangulation() const
  {
    return m_tr;
  }

  /// Returns a sphere bounding the inferred surface.
  Sphere bounding_sphere() const
  {
    return m_tr.input_points_bounding_sphere();
  }

  /// The function compute_implicit_function() must be called
  /// after each insertion of oriented points.
  /// It computes the piecewise linear scalar function 'f' by:
  /// - applying Delaunay refinement.
  /// - solving for 'f' at each vertex of the triangulation with a sparse linear solver.
  /// - shifting and orienting 'f' such that 'f=0' at all input points and 'f<0' inside the inferred surface.
  ///
  /// Returns false if the linear solver fails.
  bool compute_implicit_function()
  {
    CGAL::Timer task_timer; task_timer.start();
    CGAL_TRACE_STREAM << "Delaunay refinement...\n";

    // Delaunay refinement
    const FT radius_edge_ratio_bound = 2.5;
    const unsigned int max_vertices = (unsigned int)1e7; // max 10M vertices
    const FT enlarge_ratio = 1.5;
    const FT size = sqrt(bounding_sphere().squared_radius()); // get triangulation's radius
    const FT cell_radius_bound = size/5.; // large
    unsigned int nb_vertices_added = delaunay_refinement(radius_edge_ratio_bound,cell_radius_bound,max_vertices,enlarge_ratio);

    // Print status
    CGAL_TRACE_STREAM << "Delaunay refinement: " << "added " << nb_vertices_added << " Steiner points, "
                                                 << task_timer.time() << " seconds, "
                                                 << (CGAL::Memory_sizer().virtual_size()>>20) << " Mb allocated"
                                                 << std::endl;
    task_timer.reset();

    CGAL_TRACE_STREAM << "Solve Poisson equation...\n";

    // Compute the Poisson indicator function f()
    // at each vertex of the triangulation.
    double lambda = 0.1;
    double duration_assembly, duration_factorization, duration_solve;
    if (!solve_poisson(lambda, &duration_assembly, &duration_factorization, &duration_solve))
    {
      std::cerr << "Error: cannot solve Poisson equation" << std::endl;
      return false;
    }

    // Shift and orient f() such that:
    // - f() = 0 on the input points,
    // - f() < 0 inside the surface.
    set_contouring_value(median_value_at_input_vertices());

    // Print status
    CGAL_TRACE_STREAM << "Solve Poisson equation: " << task_timer.time() << " seconds, "
                                                    << (CGAL::Memory_sizer().virtual_size()>>20) << " Mb allocated"
                                                    << std::endl;
    task_timer.reset();

    return true;
  }

// TEMPORARY HACK
/// @cond SKIP_IN_MANUAL

  /// Delaunay refinement (break bad tetrahedra, where
  /// bad means badly shaped or too big). The normal of
  /// Steiner points is set to zero.
  /// Return the number of vertices inserted.
  unsigned int delaunay_refinement(FT radius_edge_ratio_bound, ///< radius edge ratio bound (ignored if zero)
                                   FT cell_radius_bound, ///< cell radius bound (ignored if zero)
                                   unsigned int max_vertices, ///< number of vertices bound
                                   FT enlarge_ratio) ///< bounding box enlarge ratio
  {
    CGAL_TRACE("Call delaunay_refinement(radius_edge_ratio_bound=%lf, cell_radius_bound=%lf, max_vertices=%u, enlarge_ratio=%lf)\n",
               radius_edge_ratio_bound, cell_radius_bound, max_vertices, enlarge_ratio);

    Sphere enlarged_bbox = enlarged_bounding_sphere(enlarge_ratio);
    unsigned int nb_vertices_added = poisson_refine_triangulation(m_tr,radius_edge_ratio_bound,cell_radius_bound,max_vertices,enlarged_bbox);

    CGAL_TRACE("End of delaunay_refinement()\n");

    return nb_vertices_added;
  }

  /// Poisson reconstruction.
  /// Return false on error.
  bool solve_poisson(double lambda,
                     double* duration_assembly,
                     double* duration_factorization,
                     double* duration_solve,
                     bool is_normalized = false)
  {
    CGAL_TRACE("Call solve_poisson()\n");

    double time_init = clock();

    *duration_assembly = 0.0;
    *duration_factorization = 0.0;
    *duration_solve = 0.0;

    long old_max_memory = CGAL::Peak_memory_sizer().peak_virtual_size();

    CGAL_TRACE("  %ld Mb allocated, largest free memory block=%ld Mb, #blocks over 100 Mb=%ld\n",
               long(CGAL::Memory_sizer().virtual_size())>>20,
               long(CGAL::Peak_memory_sizer().largest_free_block()>>20),
               long(CGAL::Peak_memory_sizer().count_free_memory_blocks(100*1048576)));
    CGAL_TRACE("  Create matrix...\n");

    // get #variables
    unsigned int nb_variables = m_tr.index_unconstrained_vertices();

    // at least one vertex must be constrained
    if(nb_variables == m_tr.number_of_vertices())
    {
      constrain_one_vertex_on_convex_hull();
      nb_variables = m_tr.index_unconstrained_vertices();
    }

    // Assemble linear system A*X=B
    Solver solver(nb_variables, 9); // average non null elements per line = 8.3
    Sparse_vector X(nb_variables);
    Sparse_vector B(nb_variables);

    Finite_vertices_iterator v;
    for(v = m_tr.finite_vertices_begin();
        v != m_tr.finite_vertices_end();
        v++)
    {
      if(!v->constrained())
      {
        B[v->index()] = is_normalized ? div_normalized(v)
                                      : div(v); // rhs -> divergent
        assemble_poisson_row(solver,v,B,lambda);
      }
    }

    *duration_assembly = (clock() - time_init)/CLOCKS_PER_SEC;
    CGAL_TRACE("  Create matrix: done (%.2lf s)\n", *duration_assembly);

    /*
    time_init = clock();
    if(!solver.solve_conjugate_gradient(B,X,10000,1e-15))
      return false;
    *duration_solve = (clock() - time_init)/CLOCKS_PER_SEC;
    */

    CGAL_TRACE("  %ld Mb allocated, largest free memory block=%ld Mb, #blocks over 100 Mb=%ld\n",
               long(CGAL::Memory_sizer().virtual_size())>>20,
               long(CGAL::Peak_memory_sizer().largest_free_block()>>20),
               long(CGAL::Peak_memory_sizer().count_free_memory_blocks(100*1048576)));
    CGAL_TRACE("  Choleschy factorization...\n");

    // Choleschy factorization M = L L^T
    time_init = clock();
    if(!solver.factorize_ooc())
      return false;
    *duration_factorization = (clock() - time_init)/CLOCKS_PER_SEC;
    CGAL_TRACE("  Choleschy factorization: done (%.2lf s)\n", *duration_factorization);

    // Print peak memory (Windows only)
    long max_memory = CGAL::Peak_memory_sizer().peak_virtual_size();
    if (max_memory > old_max_memory)
      CGAL_TRACE("  Max allocation = %ld Mb\n", max_memory>>20);

    CGAL_TRACE("  %ld Mb allocated, largest free memory block=%ld Mb, #blocks over 100 Mb=%ld\n",
               long(CGAL::Memory_sizer().virtual_size())>>20,
               long(CGAL::Peak_memory_sizer().largest_free_block()>>20),
               long(CGAL::Peak_memory_sizer().count_free_memory_blocks(100*1048576)));
    CGAL_TRACE("  Direct solve...\n");

    // Direct solve by forward and backward substitution
    time_init = clock();
    if(!solver.solve_ooc(B,X))
      return false;
    *duration_solve = (clock() - time_init)/CLOCKS_PER_SEC;
    CGAL_TRACE("  Direct solve: done (%.2lf s)\n", *duration_solve);

    /*
    // Choleschy factorization M = L L^T
    time_init = clock();
    if(!solver.factorize(true))
      return false;
    *duration_factorization = (clock() - time_init)/CLOCKS_PER_SEC;

    // Direct solve by forward and backward substitution
    time_init = clock();
    if(!solver.solve(B,X,1))
      return false;
    *duration_solve = (clock() - time_init)/CLOCKS_PER_SEC;
    */

    CGAL_TRACE("  Choleschy factorization + solve: done (%.2lf s)\n", *duration_factorization + *duration_solve);

    // copy function's values to vertices
    unsigned int index = 0;
    for (v = m_tr.finite_vertices_begin(); v != m_tr.finite_vertices_end(); v++)
      if(!v->constrained())
        v->f() = X[index++];

    CGAL_TRACE("  %ld Mb allocated, largest free memory block=%ld Mb, #blocks over 100 Mb=%ld\n",
               long(CGAL::Memory_sizer().virtual_size())>>20,
               long(CGAL::Peak_memory_sizer().largest_free_block()>>20),
               long(CGAL::Peak_memory_sizer().count_free_memory_blocks(100*1048576)));
    CGAL_TRACE("End of solve_poisson()\n");

    return true;
  }

  /// Shift and orient the implicit function such that:
  /// - the implicit function = 0 for points / f() = contouring_value,
  /// - the implicit function < 0 inside the surface.
  ///
  /// Return the minimum value of the implicit function.
  FT set_contouring_value(FT contouring_value)
  {
    // median value set to 0.0
    shift_f(-contouring_value);

    // check value on convex hull (should be positive)
    Vertex_handle v = any_vertex_on_convex_hull();
    if(v->f() < 0.0)
      flip_f();

    // Update m_sink
    FT sink_value = find_sink();
    return sink_value;
  }

// TEMPORARY HACK
/// @endcond

  /// Evaluates the implicit function at a given 3D query point.
  FT f(const Point& p) const
  {
    m_hint = m_tr.locate(p,m_hint);

    if(m_hint == NULL)
      return 1e38;

    if(m_tr.is_infinite(m_hint))
      return 1e38;

    FT a,b,c,d;
    barycentric_coordinates(p,m_hint,a,b,c,d);
    return a * m_hint->vertex(0)->f() +
           b * m_hint->vertex(1)->f() +
           c * m_hint->vertex(2)->f() +
           d * m_hint->vertex(3)->f();
  }

  /// 'ImplicitFunction' interface: evaluate implicit function for any 3D point.
  FT operator()(const Point& p) const
  {
    return f(p);
  }

  /// Returns a point located inside the inferred surface.
  Point get_inner_point() const
  {
    // Get point / the implicit function is minimum
    return m_sink;
  }

// TEMPORARY HACK
/// @cond SKIP_IN_MANUAL

/// Get median value of the implicit function over input vertices.
  FT median_value_at_input_vertices() const
  {
    std::deque<FT> values;
    Finite_vertices_iterator v;
    for(v = m_tr.finite_vertices_begin();
        v != m_tr.finite_vertices_end();
        v++)
      if(v->type() == Triangulation::INPUT)
        values.push_back(v->f());

    int size = values.size();
    if(size == 0)
    {
      std::cerr << "Contouring: no input points\n";
      return 0.0;
    }

    std::sort(values.begin(),values.end());
    int index = size/2;
    // return values[size/2];
    return 0.5 * (values[index] + values[index+1]); // avoids singular cases
  }

// TEMPORARY HACK
/// @endcond

// Private methods:
private:

  // PA: todo change type (FT)
  // check if this is in CGAL already
  void barycentric_coordinates(const Point& p,
                               Cell_handle cell,
                               double& a,
                               double& b,
                               double& c,
                               double& d) const
  {
    const Point& pa = cell->vertex(0)->point();
    const Point& pb = cell->vertex(1)->point();
    const Point& pc = cell->vertex(2)->point();
    const Point& pd = cell->vertex(3)->point();
    Tetrahedron ta(pb,pc,pd,p);
    Tetrahedron tb(pa,pc,pd,p);
    Tetrahedron tc(pb,pa,pd,p);
    Tetrahedron td(pb,pc,pa,p);
    Tetrahedron tet(pa,pb,pc,pd);
    double v = tet.volume();
    a = std::fabs(ta.volume() / v);
    b = std::fabs(tb.volume() / v);
    c = std::fabs(tc.volume() / v);
    d = std::fabs(td.volume() / v);
  }

  FT find_sink()
  {
    m_sink = CGAL::ORIGIN;
    FT min_f = 1e38;
    Finite_vertices_iterator v;
    for(v = m_tr.finite_vertices_begin();
        v != m_tr.finite_vertices_end();
        v++)
    {
      if(v->f() < min_f)
      {
        m_sink = v->point();
        min_f = v->f();
      }
    }
    return min_f;
  }

  void shift_f(const FT shift)
  {
    Finite_vertices_iterator v;
    for(v = m_tr.finite_vertices_begin();
        v != m_tr.finite_vertices_end();
        v++)
      v->f() += shift;
  }

  void flip_f()
  {
    Finite_vertices_iterator v;
    for(v = m_tr.finite_vertices_begin();
        v != m_tr.finite_vertices_end();
        v++)
      v->f() = -v->f();
  }

  Vertex_handle any_vertex_on_convex_hull()
  {
    // TODO: return NULL if none and assert
    std::vector<Vertex_handle> vertices;
    m_tr.incident_vertices(m_tr.infinite_vertex(),std::back_inserter(vertices));
    typename std::vector<Vertex_handle>::iterator it = vertices.begin();
    return *it;
  }

  void constrain_one_vertex_on_convex_hull(const FT value = 0.0)
  {
    Vertex_handle v = any_vertex_on_convex_hull();
    v->constrained() = true;
    v->f() = value;
  }

  // divergent
  FT div(Vertex_handle v)
  {
    std::vector<Cell_handle> cells;
    m_tr.incident_cells(v,std::back_inserter(cells));
    if(cells.size() == 0)
      return 0.0;

    FT div = 0.0;
    typename std::vector<Cell_handle>::iterator it;
    for(it = cells.begin(); it != cells.end(); it++)
    {
      Cell_handle cell = *it;
      if(m_tr.is_infinite(cell))
        continue;

      // compute average normal per cell
      Vector n = cell_normal(cell);

      // zero normal - no need to compute anything else
      if(n == CGAL::NULL_VECTOR)
        continue;

      // compute n'
      int index = cell->index(v);
      const Point& a = cell->vertex((index+1)%4)->point();
      const Point& b = cell->vertex((index+2)%4)->point();
      const Point& c = cell->vertex((index+3)%4)->point();
      Vector nn = (index%2==0) ? CGAL::cross_product(b-a,c-a) : CGAL::cross_product(c-a,b-a);
      nn = nn / std::sqrt(nn*nn); // normalize

      Triangle face(a,b,c);
      FT area = std::sqrt(face.squared_area());

      div += n * nn * area;
    }
    return div;
  }

  FT div_normalized(Vertex_handle v)
  {
    std::vector<Cell_handle> cells;
    m_tr.incident_cells(v,std::back_inserter(cells));
    if(cells.size() == 0)
      return 0.0;

	FT length = 100000;
	int counter = 0;
    FT div = 0.0;
    typename std::vector<Cell_handle>::iterator it;
    for(it = cells.begin(); it != cells.end(); it++)
    {
      Cell_handle cell = *it;
      if(m_tr.is_infinite(cell))
        continue;

      // compute average normal per cell
      Vector n = cell_normal(cell);

      // zero normal - no need to compute anything else
      if(n == CGAL::NULL_VECTOR)
        continue;

      // compute n'
      int index = cell->index(v);
	  const Point& x = cell->vertex(index)->point();
      const Point& a = cell->vertex((index+1)%4)->point();
      const Point& b = cell->vertex((index+2)%4)->point();
      const Point& c = cell->vertex((index+3)%4)->point();
      Vector nn = (index%2==0) ? CGAL::cross_product(b-a,c-a) : CGAL::cross_product(c-a,b-a);
	  nn = nn / std::sqrt(nn*nn); // normalize
	  Vector p = a - x;
      Vector q = b - x;
	  Vector r = c - x;
	  FT p_n = std::sqrt(p*p);
	  FT q_n = std::sqrt(q*q);
	  FT r_n = std::sqrt(r*r);
	  FT solid_angle = p*(CGAL::cross_product(q,r));
	  solid_angle = std::abs(solid_angle * 1.0 / (p_n*q_n*r_n + (p*q)*r_n + (q*r)*p_n + (r*p)*q_n));
	  Triangle face(a,b,c);
      FT area = std::sqrt(face.squared_area());
	  length = std::sqrt((x-a)*(x-a)) + std::sqrt((x-b)*(x-b)) + std::sqrt((x-c)*(x-c));
	  counter++;
		div += n * nn * area * 3 / length ;
     }
    return div;
  }

  Vector cell_normal(Cell_handle cell)
  {
    const Vector& n0 = cell->vertex(0)->normal();
    const Vector& n1 = cell->vertex(1)->normal();
    const Vector& n2 = cell->vertex(2)->normal();
    const Vector& n3 = cell->vertex(3)->normal();
    Vector n = n0 + n1 + n2 + n3;
    FT sq_norm = n*n;
    if(sq_norm != 0.0)
      return n / std::sqrt(sq_norm); // normalize
    else
      return CGAL::NULL_VECTOR;
  }

  // cotan formula as area(voronoi face) / len(primal edge)
  FT cotan_geometric(Edge& edge)
  {
    Cell_handle cell = edge.first;
    Vertex_handle vi = cell->vertex(edge.second);
    Vertex_handle vj = cell->vertex(edge.third);

    // primal edge
    const Point& pi = vi->point();
    const Point& pj = vj->point();
    Vector primal = pj - pi;
    FT len_primal = std::sqrt(primal * primal);
    return area_voronoi_face(edge) / len_primal;
  }

  // spin around edge
  // return area(voronoi face)
  FT area_voronoi_face(Edge& edge)
  {
    // circulate around edge
    Cell_circulator circ = m_tr.incident_cells(edge);
    Cell_circulator done = circ;
    std::vector<Point> voronoi_points;
    do
    {
      Cell_handle cell = circ;
      if(!m_tr.is_infinite(cell))
        voronoi_points.push_back(m_tr.dual(cell));
      else // one infinite tet, switch to another calculation
        return area_voronoi_face_boundary(edge);
      circ++;
    }
    while(circ != done);

    if(voronoi_points.size() < 3)
    {
      CGAL_surface_reconstruction_points_assertion(false);
      return 0.0;
    }

    // sum up areas
    FT area = 0.0;
    const Point& a = voronoi_points[0];
    unsigned int nb_triangles = voronoi_points.size() - 2;
    for(unsigned int i=1;i<nb_triangles;i++)
    {
      const Point& b = voronoi_points[i];
      const Point& c = voronoi_points[i+1];
      Triangle triangle(a,b,c);
      area += std::sqrt(triangle.squared_area());
    }
    return area;
  }

  // approximate area when a cell is infinite
  FT area_voronoi_face_boundary(Edge& edge)
  {
    FT area = 0.0;
    Vertex_handle vi = edge.first->vertex(edge.second);
    Vertex_handle vj = edge.first->vertex(edge.third);

    const Point& pi = vi->point();
    const Point& pj = vj->point();
    Point m = CGAL::midpoint(pi,pj);

    // circulate around each incident cell
    Cell_circulator circ = m_tr.incident_cells(edge);
    Cell_circulator done = circ;
    do
    {
      Cell_handle cell = circ;
      if(!m_tr.is_infinite(cell))
      {
        // circumcenter of cell
        Point c = m_tr.dual(cell);
        Tetrahedron tet = m_tr.tetrahedron(cell);

        int i = cell->index(vi);
        int j = cell->index(vj);
        int k = -1, l = -1;
        other_two_indices(i,j, &k,&l);
        Vertex_handle vk = cell->vertex(k);
        Vertex_handle vl = cell->vertex(l);

        const Point& pk = vk->point();
        const Point& pl = vl->point();

        // if circumcenter is outside tet
        // pick barycenter instead
        if(tet.has_on_unbounded_side(c))
        {
          Point cell_points[4] = {pi,pj,pk,pl};
          c = CGAL::centroid(cell_points, cell_points+4);
        }

        Point ck = CGAL::circumcenter(pi,pj,pk);
        Point cl = CGAL::circumcenter(pi,pj,pl);

        Triangle mcck(m,m,ck);
        Triangle mccl(m,m,cl);

        area += std::sqrt(mcck.squared_area());
        area += std::sqrt(mccl.squared_area());
      }
      circ++;
    }
    while(circ != done);
    return area;
  }

  // Get indices different from i and j
  void other_two_indices(int i, int j, int* k, int* l)
  {
    CGAL_surface_reconstruction_points_assertion(i != j);
    bool k_done = false;
    bool l_done = false;
    for(int index=0;index<4;index++)
    {
      if(index != i && index != j)
      {
        if(!k_done)
        {
          *k = index;
          k_done = true;
        }
        else
        {
          *l = index;
          l_done = true;
        }
      }
    }
    CGAL_surface_reconstruction_points_assertion(k_done);
    CGAL_surface_reconstruction_points_assertion(l_done);
  }

  // Assemble vi's row of the linear system A*X=B
  void assemble_poisson_row(Solver& solver,
                            Vertex_handle vi,
                            Sparse_vector& B,
                            double lambda)
  {
    // assemble new row
    solver.begin_row();

    // for each vertex vj neighbor of vi
    std::vector<Vertex_handle> vertices;
    m_tr.incident_vertices(vi,std::back_inserter(vertices));
    double diagonal = 0.0;
    for(typename std::vector<Vertex_handle>::iterator it = vertices.begin();
        it != vertices.end();
        it++)
    {
      Vertex_handle vj = *it;
      if(m_tr.is_infinite(vj))
        continue;

      // get corresponding edge
      Edge edge = sorted_edge(vi,vj);

      double cij = cotan_geometric(edge);
      if(vj->constrained())
        B[vi->index()] -= cij * vj->f(); // change rhs
      else
        solver.add_value(vj->index(),-cij); // off-diagonal coefficient

      diagonal += cij;
    }

    // diagonal coefficient
    if (vi->type() == Triangulation::INPUT)
      solver.add_value(vi->index(),diagonal + lambda) ;
    else
      solver.add_value(vi->index(),diagonal);

    // end matrix row
    solver.end_row();
  }

  Edge sorted_edge(Vertex_handle vi,
                   Vertex_handle vj)
  {
    int i1 = 0;
    int i2 = 0;
    Cell_handle cell = NULL;
    bool success;
    if(vi->index() > vj->index())
      success = m_tr.is_edge(vi,vj,cell,i1,i2);
    else
      success = m_tr.is_edge(vj,vi,cell,i1,i2);
    CGAL_surface_reconstruction_points_assertion(success);
    return Edge(cell,i1,i2);
  }

  /// Compute enlarged geometric bounding sphere of the embedded triangulation.
  Sphere enlarged_bounding_sphere(FT ratio) const
  {
    Sphere bbox = bounding_sphere(); // triangulation's bounding sphere
    return Sphere(bbox.center(), bbox.squared_radius() * ratio*ratio);
  }

}; // end of Poisson_reconstruction_function


CGAL_END_NAMESPACE

#endif // CGAL_POISSON_RECONSTRUCTION_FUNCTION_H
