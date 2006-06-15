/*
** Copyright (C) 1999-2004 Erik de Castro Lopo <erikd@mega-nerd.com>
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

#include	<stdio.h>

#include	"sndfile.h"
#include	"common.h"

/*------------------------------------------------------------------------------
** Public function.
*/

int
raw_open	(SF_PRIVATE *psf)
{	int	subformat, error = SFE_NO_ERROR ;

	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;

	psf->endian = psf->sf.format & SF_FORMAT_ENDMASK ;

	if (CPU_IS_BIG_ENDIAN && (psf->endian == 0 || psf->endian == SF_ENDIAN_CPU))
		psf->endian = SF_ENDIAN_BIG ;
	else if (CPU_IS_LITTLE_ENDIAN && (psf->endian == 0 || psf->endian == SF_ENDIAN_CPU))
		psf->endian = SF_ENDIAN_LITTLE ;

	psf->blockwidth = psf->bytewidth * psf->sf.channels ;
	psf->dataoffset = 0 ;
	psf->datalength = psf->filelength ;

	switch (subformat)
	{	case SF_FORMAT_PCM_S8 :
				error = pcm_init (psf) ;
				break ;

		case SF_FORMAT_PCM_U8 :
				error = pcm_init (psf) ;
				break ;

		case SF_FORMAT_PCM_16 :
		case SF_FORMAT_PCM_24 :
		case SF_FORMAT_PCM_32 :
				error = pcm_init (psf) ;
				break ;

		case SF_FORMAT_ULAW :
				error = ulaw_init (psf) ;
				break ;

		case SF_FORMAT_ALAW :
				error = alaw_init (psf) ;
				break ;

		case SF_FORMAT_GSM610 :
				error = gsm610_init (psf) ;
				break ;

		/* Lite remove start */
		case SF_FORMAT_FLOAT :
				error = float32_init (psf) ;
				break ;

		case SF_FORMAT_DOUBLE :
				error = double64_init (psf) ;
				break ;

		case SF_FORMAT_DWVW_12 :
				error = dwvw_init (psf, 12) ;
				break ;

		case SF_FORMAT_DWVW_16 :
				error = dwvw_init (psf, 16) ;
				break ;

		case SF_FORMAT_DWVW_24 :
				error = dwvw_init (psf, 24) ;
				break ;

		case SF_FORMAT_VOX_ADPCM :
				error = vox_adpcm_init (psf) ;
				break ;
		/* Lite remove end */

		default : return SFE_BAD_OPEN_FORMAT ;
		} ;

	return error ;
} /* raw_open */
/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: f0066de7-d6ce-4f36-a1e0-e475c07d4e1a
*/
