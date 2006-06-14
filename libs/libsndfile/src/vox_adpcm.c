/*
** Copyright (C) 2002-2005 Erik de Castro Lopo <erikd@mega-nerd.com>
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

/*
**	This is the OKI / Dialogic ADPCM encoder/decoder. It converts from
**	12 bit linear sample data to a 4 bit ADPCM.
**
**	Implemented from the description found here:
**
**		http://www.comptek.ru:8100/telephony/tnotes/tt1-13.html
**
**	and compared against the encoder/decoder found here:
**
**		http://ibiblio.org/pub/linux/apps/sound/convert/vox.tar.gz
*/

#include	"sfconfig.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"sndfile.h"
#include	"sfendian.h"
#include	"float_cast.h"
#include	"common.h"

#define		VOX_DATA_LEN	2048
#define		PCM_DATA_LEN	(VOX_DATA_LEN *2)

typedef struct
{	short last ;
	short step_index ;

	int		vox_bytes, pcm_samples ;

	unsigned char	vox_data [VOX_DATA_LEN] ;
	short 			pcm_data [PCM_DATA_LEN] ;
} VOX_ADPCM_PRIVATE ;

static int vox_adpcm_encode_block (VOX_ADPCM_PRIVATE *pvox) ;
static int vox_adpcm_decode_block (VOX_ADPCM_PRIVATE *pvox) ;

static short vox_adpcm_decode (char code, VOX_ADPCM_PRIVATE *pvox) ;
static char vox_adpcm_encode (short samp, VOX_ADPCM_PRIVATE *pvox) ;

static sf_count_t vox_read_s (SF_PRIVATE *psf, short *ptr, sf_count_t len) ;
static sf_count_t vox_read_i (SF_PRIVATE *psf, int *ptr, sf_count_t len) ;
static sf_count_t vox_read_f (SF_PRIVATE *psf, float *ptr, sf_count_t len) ;
static sf_count_t vox_read_d (SF_PRIVATE *psf, double *ptr, sf_count_t len) ;

static sf_count_t vox_write_s (SF_PRIVATE *psf, const short *ptr, sf_count_t len) ;
static sf_count_t vox_write_i (SF_PRIVATE *psf, const int *ptr, sf_count_t len) ;
static sf_count_t vox_write_f (SF_PRIVATE *psf, const float *ptr, sf_count_t len) ;
static sf_count_t vox_write_d (SF_PRIVATE *psf, const double *ptr, sf_count_t len) ;

static int vox_read_block (SF_PRIVATE *psf, VOX_ADPCM_PRIVATE *pvox, short *ptr, int len) ;

/*============================================================================================
** Predefined OKI ADPCM encoder/decoder tables.
*/

static short step_size_table [49] =
{	16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
	66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209,
	230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
	724, 796, 876, 963, 1060, 1166, 1282, 1408, 1552
} ; /* step_size_table */

static short step_adjust_table [8] =
{	-1, -1, -1, -1, 2, 4, 6, 8
} ; /* step_adjust_table */

/*------------------------------------------------------------------------------
*/

int
vox_adpcm_init (SF_PRIVATE *psf)
{	VOX_ADPCM_PRIVATE *pvox = NULL ;

	if (psf->mode == SFM_RDWR)
		return SFE_BAD_MODE_RW ;

	if (psf->mode == SFM_WRITE && psf->sf.channels != 1)
		return SFE_CHANNEL_COUNT ;

	if ((pvox = malloc (sizeof (VOX_ADPCM_PRIVATE))) == NULL)
		return SFE_MALLOC_FAILED ;

	psf->fdata = (void*) pvox ;
	memset (pvox, 0, sizeof (VOX_ADPCM_PRIVATE)) ;

	if (psf->mode == SFM_WRITE)
	{	psf->write_short	= vox_write_s ;
		psf->write_int		= vox_write_i ;
		psf->write_float	= vox_write_f ;
		psf->write_double	= vox_write_d ;
		}
	else
	{	psf_log_printf (psf, "Header-less OKI Dialogic ADPCM encoded file.\n") ;
		psf_log_printf (psf, "Setting up for 8kHz, mono, Vox ADPCM.\n") ;

		psf->read_short		= vox_read_s ;
		psf->read_int		= vox_read_i ;
		psf->read_float		= vox_read_f ;
		psf->read_double	= vox_read_d ;
		} ;

	/* Standard sample rate chennels etc. */
	if (psf->sf.samplerate < 1)
		psf->sf.samplerate	= 8000 ;
	psf->sf.channels	= 1 ;

	psf->sf.frames = psf->filelength * 2 ;

	psf->sf.seekable = SF_FALSE ;

	/* Seek back to start of data. */
	if (psf_fseek (psf, 0 , SEEK_SET) == -1)
		return SFE_BAD_SEEK ;

	return 0 ;
} /* vox_adpcm_init */

/*------------------------------------------------------------------------------
*/

static char
vox_adpcm_encode (short samp, VOX_ADPCM_PRIVATE *pvox)
{	short code ;
	short diff, error, stepsize ;

	stepsize = step_size_table [pvox->step_index] ;
	code = 0 ;

	diff = samp - pvox->last ;
	if (diff < 0)
	{	code = 0x08 ;
		error = -diff ;
		}
	else
		error = diff ;

	if (error >= stepsize)
	{	code = code | 0x04 ;
		error -= stepsize ;
		} ;

	if (error >= stepsize / 2)
	{	code = code | 0x02 ;
		error -= stepsize / 2 ;
		} ;

	if (error >= stepsize / 4)
		code = code | 0x01 ;

	/*
	** To close the feedback loop, the deocder is used to set the
	** estimate of last sample and in doing so, also set the step_index.
	*/
	pvox->last = vox_adpcm_decode (code, pvox) ;

	return code ;
} /* vox_adpcm_encode */

static short
vox_adpcm_decode (char code, VOX_ADPCM_PRIVATE *pvox)
{	short diff, error, stepsize, samp ;

	stepsize = step_size_table [pvox->step_index] ;

	error = stepsize / 8 ;

	if (code & 0x01)
		error += stepsize / 4 ;

	if (code & 0x02)
		error += stepsize / 2 ;

	if (code & 0x04)
		error += stepsize ;

	diff = (code & 0x08) ? -error : error ;
	samp = pvox->last + diff ;

	/*
	**  Apply clipping.
	*/
	if (samp > 2048)
		samp = 2048 ;
	if (samp < -2048)
		samp = -2048 ;

	pvox->last = samp ;
	pvox->step_index += step_adjust_table [code & 0x7] ;

	if (pvox->step_index < 0)
		pvox->step_index = 0 ;
	if (pvox->step_index > 48)
		pvox->step_index = 48 ;

	return samp ;
} /* vox_adpcm_decode */

static int
vox_adpcm_encode_block (VOX_ADPCM_PRIVATE *pvox)
{	unsigned char code ;
	int j, k ;

	/* If data_count is odd, add an extra zero valued sample. */
	if (pvox->pcm_samples & 1)
		pvox->pcm_data [pvox->pcm_samples++] = 0 ;

	for (j = k = 0 ; k < pvox->pcm_samples ; j++)
	{	code = vox_adpcm_encode (pvox->pcm_data [k++] / 16, pvox) << 4 ;
		code |= vox_adpcm_encode (pvox->pcm_data [k++] / 16, pvox) ;
		pvox->vox_data [j] = code ;
		} ;

	pvox->vox_bytes = j ;

	return 0 ;
} /* vox_adpcm_encode_block */

static int
vox_adpcm_decode_block (VOX_ADPCM_PRIVATE *pvox)
{	unsigned char code ;
	int j, k ;

	for (j = k = 0 ; j < pvox->vox_bytes ; j++)
	{	code = pvox->vox_data [j] ;
		pvox->pcm_data [k++] = 16 * vox_adpcm_decode ((code >> 4) & 0x0f, pvox) ;
		pvox->pcm_data [k++] = 16 * vox_adpcm_decode (code & 0x0f, pvox) ;
		} ;

	pvox->pcm_samples = k ;

	return 0 ;
} /* vox_adpcm_decode_block */

/*==============================================================================
*/

static int
vox_read_block (SF_PRIVATE *psf, VOX_ADPCM_PRIVATE *pvox, short *ptr, int len)
{	int	indx = 0, k ;

	while (indx < len)
	{	pvox->vox_bytes = (len - indx > PCM_DATA_LEN) ? VOX_DATA_LEN : (len - indx + 1) / 2 ;

		if ((k = psf_fread (pvox->vox_data, 1, pvox->vox_bytes, psf)) != pvox->vox_bytes)
		{	if (psf_ftell (psf) + k != psf->filelength)
				psf_log_printf (psf, "*** Warning : short read (%d != %d).\n", k, pvox->vox_bytes) ;
			if (k == 0)
				break ;
			} ;

		pvox->vox_bytes = k ;

		vox_adpcm_decode_block (pvox) ;

		memcpy (&(ptr [indx]), pvox->pcm_data, pvox->pcm_samples * sizeof (short)) ;
		indx += pvox->pcm_samples ;
		} ;

	return indx ;
} /* vox_read_block */


static sf_count_t
vox_read_s (SF_PRIVATE *psf, short *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE 	*pvox ;
	int			readcount, count ;
	sf_count_t	total = 0 ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	while (len > 0)
	{	readcount = (len > 0x10000000) ? 0x10000000 : (int) len ;

		count = vox_read_block (psf, pvox, ptr, readcount) ;

		total += count ;
		len -= count ;
		if (count != readcount)
			break ;
		} ;

	return total ;
} /* vox_read_s */

static sf_count_t
vox_read_i	(SF_PRIVATE *psf, int *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE *pvox ;
	short		*sptr ;
	int			k, bufferlen, readcount, count ;
	sf_count_t	total = 0 ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	sptr = psf->u.sbuf ;
	bufferlen = ARRAY_LEN (psf->u.sbuf) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : (int) len ;
		count = vox_read_block (psf, pvox, sptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [total + k] = ((int) sptr [k]) << 16 ;
		total += count ;
		len -= readcount ;
		if (count != readcount)
			break ;
		} ;

	return total ;
} /* vox_read_i */

static sf_count_t
vox_read_f (SF_PRIVATE *psf, float *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE *pvox ;
	short		*sptr ;
	int			k, bufferlen, readcount, count ;
	sf_count_t	total = 0 ;
	float		normfact ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	normfact = (psf->norm_float == SF_TRUE) ? 1.0 / ((float) 0x8000) : 1.0 ;

	sptr = psf->u.sbuf ;
	bufferlen = ARRAY_LEN (psf->u.sbuf) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : (int) len ;
		count = vox_read_block (psf, pvox, sptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [total + k] = normfact * (float) (sptr [k]) ;
		total += count ;
		len -= readcount ;
		if (count != readcount)
			break ;
		} ;

	return total ;
} /* vox_read_f */

static sf_count_t
vox_read_d (SF_PRIVATE *psf, double *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE *pvox ;
	short		*sptr ;
	int			k, bufferlen, readcount, count ;
	sf_count_t	total = 0 ;
	double 		normfact ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	normfact = (psf->norm_double == SF_TRUE) ? 1.0 / ((double) 0x8000) : 1.0 ;

	sptr = psf->u.sbuf ;
	bufferlen = ARRAY_LEN (psf->u.sbuf) ;
	while (len > 0)
	{	readcount = (len >= bufferlen) ? bufferlen : (int) len ;
		count = vox_read_block (psf, pvox, sptr, readcount) ;
		for (k = 0 ; k < readcount ; k++)
			ptr [total + k] = normfact * (double) (sptr [k]) ;
		total += count ;
		len -= readcount ;
		if (count != readcount)
			break ;
		} ;

	return total ;
} /* vox_read_d */

/*------------------------------------------------------------------------------
*/

static int
vox_write_block (SF_PRIVATE *psf, VOX_ADPCM_PRIVATE *pvox, const short *ptr, int len)
{	int	indx = 0, k ;

	while (indx < len)
	{	pvox->pcm_samples = (len - indx > PCM_DATA_LEN) ? PCM_DATA_LEN : len - indx ;

		memcpy (pvox->pcm_data, &(ptr [indx]), pvox->pcm_samples * sizeof (short)) ;

		vox_adpcm_encode_block (pvox) ;

		if ((k = psf_fwrite (pvox->vox_data, 1, pvox->vox_bytes, psf)) != pvox->vox_bytes)
			psf_log_printf (psf, "*** Warning : short read (%d != %d).\n", k, pvox->vox_bytes) ;

		indx += pvox->pcm_samples ;
		} ;

	return indx ;
} /* vox_write_block */

static sf_count_t
vox_write_s (SF_PRIVATE *psf, const short *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE 	*pvox ;
	int			writecount, count ;
	sf_count_t	total = 0 ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	while (len)
	{	writecount = (len > 0x10000000) ? 0x10000000 : (int) len ;

		count = vox_write_block (psf, pvox, ptr, writecount) ;

		total += count ;
		len -= count ;
		if (count != writecount)
			break ;
		} ;

	return total ;
} /* vox_write_s */

static sf_count_t
vox_write_i	(SF_PRIVATE *psf, const int *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE *pvox ;
	short		*sptr ;
	int			k, bufferlen, writecount, count ;
	sf_count_t	total = 0 ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	sptr = psf->u.sbuf ;
	bufferlen = ARRAY_LEN (psf->u.sbuf) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		for (k = 0 ; k < writecount ; k++)
			sptr [k] = ptr [total + k] >> 16 ;
		count = vox_write_block (psf, pvox, sptr, writecount) ;
		total += count ;
		len -= writecount ;
		if (count != writecount)
			break ;
		} ;

	return total ;
} /* vox_write_i */

static sf_count_t
vox_write_f (SF_PRIVATE *psf, const float *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE *pvox ;
	short		*sptr ;
	int			k, bufferlen, writecount, count ;
	sf_count_t	total = 0 ;
	float		normfact ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	normfact = (psf->norm_float == SF_TRUE) ? (1.0 * 0x7FFF) : 1.0 ;

	sptr = psf->u.sbuf ;
	bufferlen = ARRAY_LEN (psf->u.sbuf) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		for (k = 0 ; k < writecount ; k++)
			sptr [k] = lrintf (normfact * ptr [total + k]) ;
		count = vox_write_block (psf, pvox, sptr, writecount) ;
		total += count ;
		len -= writecount ;
		if (count != writecount)
			break ;
		} ;

	return total ;
} /* vox_write_f */

static sf_count_t
vox_write_d	(SF_PRIVATE *psf, const double *ptr, sf_count_t len)
{	VOX_ADPCM_PRIVATE *pvox ;
	short		*sptr ;
	int			k, bufferlen, writecount, count ;
	sf_count_t	total = 0 ;
	double 		normfact ;

	if (! psf->fdata)
		return 0 ;
	pvox = (VOX_ADPCM_PRIVATE*) psf->fdata ;

	normfact = (psf->norm_double == SF_TRUE) ? (1.0 * 0x7FFF) : 1.0 ;

	sptr = psf->u.sbuf ;
	bufferlen = ARRAY_LEN (psf->u.sbuf) ;
	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		for (k = 0 ; k < writecount ; k++)
			sptr [k] = lrint (normfact * ptr [total + k]) ;
		count = vox_write_block (psf, pvox, sptr, writecount) ;
		total += count ;
		len -= writecount ;
		if (count != writecount)
			break ;
		} ;

	return total ;
} /* vox_write_d */


/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: e15e97fe-ff9d-4b46-a489-7059fb2d0b1e
*/
