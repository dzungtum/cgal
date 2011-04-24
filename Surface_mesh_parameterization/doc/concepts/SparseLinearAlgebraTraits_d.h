// Copyright (c) 2005  INRIA (France).
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
// Author(s)     : Laurent Saboret, Pierre Alliez, Bruno Levy


/// The concept SparseLinearAlgebraTraits_d
/// is used to solve sparse linear systems "A*X = B".

class SparseLinearAlgebraTraits_d : public LinearAlgebraTraits_d
{
// Public types
public:
    typedef xxx Matrix ;
    typedef xxx Vector ;
    typedef xxx NT;

// Public operations
public:
    /// Default constructor
    SparseLinearAlgebraTraits_d();

    /// Solve the sparse linear system "A*X = B".
    /// Return true on success. The solution is then (1/D) * X.
    ///
    /// @commentheading Preconditions:
    /// - A.row_dimension() == B.dimension().
    /// - A.column_dimension() == X.dimension().
    bool linear_solver (const Matrix& A, const Vector& B, Vector& X, NT& D);
};
