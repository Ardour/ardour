/*
 * file that facilitates C++ program debugging.
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * NON-COMMERCIAL purpose and without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies, and that the name of Gray Watson not be used in
 * advertising or publicity pertaining to distribution of the document
 * or software without specific, written prior permission.
 *
 * Please see the PERMISSIONS file or contact the author for information
 * about commercial licenses.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://www.letters.com/~gray/
 *
 * $Id$
 */

/*
 * This file is used to effectively redirect new to the more familiar
 * malloc and delete to the more familiar free so they can be debugged
 * with the debug malloc library..  They also give the known error
 * behavior, too.
 *
 * Compile and link this in with the C++ program you want to debug.
 *
 * NOTE: I am not a C++ hacker so feedback in the form of other hints
 * and ideas for C++ users would be much appreciated.
 */

#ifdef DEBUG_MALLOC

extern "C" {
#include <stdlib.h>
#include <dmalloc.h>
#include "/usr/local/src/dmalloc-4.1.2/return.h"
}

/*
 * An overload function for the C++ new.
 */
void *
operator new(size_t size)
{
  char	*file;
  GET_RET_ADDR(file);

  /* handle correct C++ semantics for an alloc of size 0 */
  
  if (size == 0) size = 1;
  
  return _malloc_leap(file, 0, size);
}

/*
 * An overload function for the C++ new[].
 */
void *
operator new[](size_t size)
{
  char	*file;
  GET_RET_ADDR(file);

  /* handle correct C++ semantics for an alloc of size 0 */
  
  if (size == 0) size = 1;

  return _malloc_leap(file, 0, size);
}

/*
 * An overload function for the C++ delete.
 */
void
operator delete(void *pnt)
{
  char	*file;
  GET_RET_ADDR(file);
  _free_leap(file, 0, pnt);
}

/*
 * An overload function for the C++ delete[].  Thanks to Jens Krinke
 * <j.krinke@gmx.de>
 */
void
operator delete[](void *pnt)
{
  char	*file;
  GET_RET_ADDR(file);
  _free_leap(file, 0, pnt);
}

#endif
