// $Id$
//
// Cassowary Incremental Constraint Solver
// Original Smalltalk Implementation by Alan Borning
// This C++ Implementation by Greg J. Badros, <gjb@cs.washington.edu>
// http://www.cs.washington.edu/homes/gjb
// (C) 1998, 1999 Greg J. Badros and Alan Borning
// See ../LICENSE for legal details regarding this software
//
// ClTypedefs.h

#ifndef CL_TYPEDEFS_H__
#define CL_TYPEDEFS_H__

#if defined(HAVE_CONFIG_H) && !defined(CONFIG_H_INCLUDED) && !defined(CONFIG_INLINE_H_INCLUDED)
#include <cassowary/config-inline.h>
#define CONFIG_INLINE_H_INCLUDED
#endif

#include "ClLinearExpression_fwd.h"
#include <set> 
#include <map>
#include <vector>

using std::set;
using std::map;
using std::vector;

class ClVariable;
class ClConstraint;
class ClEditInfo;

typedef set<ClVariable> ClVarSet;  
typedef map<ClVariable, ClVarSet > ClTableauColumnsMap;
typedef map<ClVariable, ClLinearExpression *> ClTableauRowsMap;

// For Solver
typedef map<const ClConstraint *, ClVarSet> ClConstraintToVarSetMap;
typedef map<const ClConstraint *, ClVariable> ClConstraintToVarMap;
typedef map<ClVariable, const ClConstraint *> ClVarToConstraintMap;
typedef vector<ClVariable> ClVarVector;

typedef set<const ClConstraint *> ClConstraintSet;

// For FDSolver
typedef map<ClVariable, ClConstraintSet> ClVarToConstraintSetMap;

#endif
