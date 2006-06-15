/*
** Copyright (C) 2006 Paul Davis <paul@linuxaudiosystems.com>
** Copyright (C) 2006 Erik de Castro Lopo <erikd@mega-nerd.com>
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

#include <stdio.h>
#include <string.h>

#include "common.h"

/*
** Allocate and initialize a broadcast info structure.
*/

SF_BROADCAST_INFO*
broadcast_info_alloc (void)
{	SF_BROADCAST_INFO* bext ;

	if ((bext = calloc (1, sizeof (SF_BROADCAST_INFO))) == NULL)
		return NULL ;

	return bext ;
} /* broadcast_info_alloc */

int
broadcast_info_copy (SF_BROADCAST_INFO* dst, SF_BROADCAST_INFO* src)
{	memcpy (dst, src, sizeof (SF_BROADCAST_INFO)) ;

	/* Currently writing this version. */
	dst->version = 1 ;

	return SF_TRUE ;
} /* broadcast_info_copy */

int
broadcast_add_coding_history (SF_BROADCAST_INFO* bext, unsigned int channels, unsigned int samplerate)
{	char chnstr [16] ;
	int count ;

	switch (channels)
	{	case 0 :
			return SF_FALSE ;

		case 1 :
			strncpy (chnstr, "mono", sizeof (chnstr)) ;
			break ;

		case 2 :
			strncpy (chnstr, "stereo", sizeof (chnstr)) ;
			break ;

	default :
		LSF_SNPRINTF (chnstr, sizeof (chnstr), "%uchn", channels) ;
		break ;
	}

	count = LSF_SNPRINTF (bext->coding_history, sizeof (bext->coding_history), "F=%u,A=PCM,M=%s,W=24,T=%s-%s", samplerate, chnstr, PACKAGE, VERSION) ;

	if (count >= SIGNED_SIZEOF (bext->coding_history))
		bext->coding_history_size = sizeof (bext->coding_history) ;
	else
	{	count += count & 1 ;
		bext->coding_history_size = count ;
		} ;

	return SF_TRUE ;
} /* broadcast_add_coding_history */

/*
** Do not edit or modify anything in this comment block.
** The following line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 4b3b69c7-d710-4424-9da0-5048534a0beb
*/
