// $Id$
//
// Cassowary Incremental Constraint Solver
// Original Smalltalk Implementation by Alan Borning
// This C++ Implementation by Greg J. Badros, <gjb@cs.washington.edu>
// http://www.cs.washington.edu/homes/gjb
// (C) 1998, 1999 Greg J. Badros and Alan Borning
// See ../LICENSE for legal details regarding this software
//
// Cassowary.h

#ifndef Cassowary_H
#define Cassowary_H

#if defined(HAVE_CONFIG_H) && !defined(CONFIG_H_INCLUDED) && !defined(CONFIG_INLINE_H_INCLUDED)
	#include <cassowary/config-inline.h>
	#define CONFIG_INLINE_H_INCLUDED
#endif

#ifndef CL_PTR_HASH_DIVISOR
	#define CL_PTR_HASH_DIVISOR 4
#endif

#include "ClConstraintHash.h"
#include <climits>

#include <string>
#include <cassert>
#include <iostream>

typedef double Number;

typedef long FDNumber;

enum { FDN_NOTSET = LONG_MIN };

#define NEWVAR(x) do { cerr << "line " << __LINE__ << ": new " << x << endl; } while (0)
#define DELVAR(x) do { cerr << "line " << __LINE__ << ": del " << x << endl; } while (0)

#endif // Cassowary_H
