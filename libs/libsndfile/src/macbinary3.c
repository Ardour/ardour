/*
** Copyright (C) 2003-2005 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include	"sfconfig.h"

#include	<stdlib.h>
#include	<string.h>

#include	"sndfile.h"
#include	"sfendian.h"
#include	"common.h"

#if (OS_IS_MACOSX == 1)

#include	<CoreServices.h>

int
macbinary3_open (SF_PRIVATE *psf)
{
	if (psf)
		return 0 ;

	return 0 ;
} /* macbinary3_open */

#else

int
macbinary3_open (SF_PRIVATE *psf)
{
	psf = psf ;
	return 0 ;
} /* macbinary3_open */

#endif /* OS_IS_MACOSX */

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: c397a7d7-1a31-4349-9684-bd29ef06211e
*/
