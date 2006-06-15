/*
** Copyright (C) 2004, 2005 Erik de Castro Lopo <erikd@mega-nerd.com>
** Copyright (C) 2004 Tobias Gehrig <tgehrig@ira.uka.de>
**
** This program is free software ; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation ; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY ; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program ; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include	"sfconfig.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<string.h>
#include	<ctype.h>

#include	"sndfile.h"
#include	"common.h"


#ifndef HAVE_FLAC_ALL_H

int
flac_open (SF_PRIVATE *psf)
{	if (psf)
		return SFE_UNIMPLEMENTED ;
	return (psf && 0) ;
} /* flac_open */


#else

#include	<FLAC/all.h>

#include	"sfendian.h"
#include	"float_cast.h"

/*------------------------------------------------------------------------------
** Private static functions.
*/

#define ENC_BUFFER_SIZE 4096

typedef enum
{	PFLAC_PCM_SHORT = 0,
	PFLAC_PCM_INT = 1,
	PFLAC_PCM_FLOAT = 2,
	PFLAC_PCM_DOUBLE = 3
} PFLAC_PCM ;

typedef struct
{	FLAC__SeekableStreamDecoder *fsd ;
	FLAC__SeekableStreamEncoder *fse ;
	PFLAC_PCM pcmtype ;
	void* ptr ;
	unsigned pos, len, remain ;

	const FLAC__int32 * const * wbuffer ;
	FLAC__int32 * rbuffer [FLAC__MAX_CHANNELS] ;

	FLAC__int32* encbuffer ;
	unsigned bufferpos ;

	const FLAC__Frame *frame ;
	FLAC__bool bufferbackup ;
} FLAC_PRIVATE ;

static sf_count_t	flac_seek (SF_PRIVATE *psf, int mode, sf_count_t offset) ;
static int			flac_close (SF_PRIVATE *psf) ;

static int			flac_enc_init (SF_PRIVATE *psf) ;
static int			flac_read_header (SF_PRIVATE *psf) ;

static sf_count_t	flac_read_flac2s (SF_PRIVATE *psf, short *ptr, sf_count_t len) ;
static sf_count_t	flac_read_flac2i (SF_PRIVATE *psf, int *ptr, sf_count_t len) ;
static sf_count_t	flac_read_flac2f (SF_PRIVATE *psf, float *ptr, sf_count_t len) ;
static sf_count_t	flac_read_flac2d (SF_PRIVATE *psf, double *ptr, sf_count_t len) ;

static sf_count_t	flac_write_s2flac (SF_PRIVATE *psf, const short *ptr, sf_count_t len) ;
static sf_count_t	flac_write_i2flac (SF_PRIVATE *psf, const int *ptr, sf_count_t len) ;
static sf_count_t	flac_write_f2flac (SF_PRIVATE *psf, const float *ptr, sf_count_t len) ;
static sf_count_t	flac_write_d2flac (SF_PRIVATE *psf, const double *ptr, sf_count_t len) ;

static void		f2flac8_array (const float *src, FLAC__int32 *dest, int count, int normalize) ;
static void		f2flac16_array (const float *src, FLAC__int32 *dest, int count, int normalize) ;
static void		f2flac24_array (const float *src, FLAC__int32 *dest, int count, int normalize) ;
static void		f2flac8_clip_array (const float *src, FLAC__int32 *dest, int count, int normalize) ;
static void		f2flac16_clip_array (const float *src, FLAC__int32 *dest, int count, int normalize) ;
static void		f2flac24_clip_array (const float *src, FLAC__int32 *dest, int count, int normalize) ;
static void		d2flac8_array (const double *src, FLAC__int32 *dest, int count, int normalize) ;
static void		d2flac16_array (const double *src, FLAC__int32 *dest, int count, int normalize) ;
static void		d2flac24_array (const double *src, FLAC__int32 *dest, int count, int normalize) ;
static void		d2flac8_clip_array (const double *src, FLAC__int32 *dest, int count, int normalize) ;
static void		d2flac16_clip_array (const double *src, FLAC__int32 *dest, int count, int normalize) ;
static void		d2flac24_clip_array (const double *src, FLAC__int32 *dest, int count, int normalize) ;

static int flac_command (SF_PRIVATE *psf, int command, void *data, int datasize) ;

/* Decoder Callbacks */
static FLAC__SeekableStreamDecoderReadStatus sf_flac_read_callback (const FLAC__SeekableStreamDecoder *decoder, FLAC__byte buffer [], unsigned *bytes, void *client_data) ;
static FLAC__SeekableStreamDecoderSeekStatus sf_flac_seek_callback (const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data) ;
static FLAC__SeekableStreamDecoderTellStatus sf_flac_tell_callback (const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data) ;
static FLAC__SeekableStreamDecoderLengthStatus sf_flac_length_callback (const FLAC__SeekableStreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data) ;
static FLAC__bool sf_flac_eof_callback (const FLAC__SeekableStreamDecoder *decoder, void *client_data) ;
static FLAC__StreamDecoderWriteStatus sf_flac_write_callback (const FLAC__SeekableStreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer [], void *client_data) ;
static void sf_flac_meta_callback (const FLAC__SeekableStreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) ;
static void sf_flac_error_callback (const FLAC__SeekableStreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) ;

/* Encoder Callbacks */
static FLAC__SeekableStreamEncoderSeekStatus sf_flac_enc_seek_callback (const FLAC__SeekableStreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data) ;
#ifdef HAVE_FLAC_1_1_1
static FLAC__SeekableStreamEncoderTellStatus sf_flac_enc_tell_callback (const FLAC__SeekableStreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data) ;
#endif
static FLAC__StreamEncoderWriteStatus sf_flac_enc_write_callback (const FLAC__SeekableStreamEncoder *encoder, const FLAC__byte buffer [], unsigned bytes, unsigned samples, unsigned current_frame, void *client_data) ;

static const int legal_sample_rates [] =
{	8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000
} ;

static inline void
s2flac8_array (const short *src, FLAC__int32 *dest, int count)
{	while (--count >= 0)
		dest [count] = src [count] >> 8 ;
} /* s2flac8_array */

static inline void
s2flac16_array (const short *src, FLAC__int32 *dest, int count)
{	while (--count >= 0)
		dest [count] = src [count] ;
} /* s2flac16_array */

static inline void
s2flac24_array (const short *src, FLAC__int32 *dest, int count)
{	while (--count >= 0)
		dest [count] = src [count] << 8 ;
} /* s2flac24_array */

static inline void
i2flac8_array (const int *src, FLAC__int32 *dest, int count)
{	while (--count >= 0)
		dest [count] = src [count] >> 24 ;
} /* i2flac8_array */

static inline void
i2flac16_array (const int *src, FLAC__int32 *dest, int count)
{
  while (--count >= 0)
    dest [count] = src [count] >> 16 ;
} /* i2flac16_array */

static inline void
i2flac24_array (const int *src, FLAC__int32 *dest, int count)
{	while (--count >= 0)
		dest [count] = src [count] >> 8 ;
} /* i2flac24_array */

static sf_count_t
flac_buffer_copy (SF_PRIVATE *psf)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	const FLAC__Frame *frame = pflac->frame ;
	const FLAC__int32* const *buffer = pflac->wbuffer ;
	unsigned i = 0, j, offset ;

	if (pflac->ptr == NULL)
	{	/*
		**	Not sure why this code is here and not elsewhere.
		**	Removing it causes valgrind errors.
		*/
		pflac->bufferbackup = SF_TRUE ;
		for (i = 0 ; i < frame->header.channels ; i++)
		{	if (pflac->rbuffer [i] == NULL)
				pflac->rbuffer [i] = calloc (frame->header.blocksize, sizeof (FLAC__int32)) ;
			memcpy (pflac->rbuffer [i], buffer [i], frame->header.blocksize * sizeof (FLAC__int32)) ;
			} ;
		pflac->wbuffer = (const FLAC__int32* const*) pflac->rbuffer ;

		return 0 ;
		} ;

	switch (pflac->pcmtype)
	{	case PFLAC_PCM_SHORT :
			{	short *retpcm = ((short*) pflac->ptr) ;
				int shift = 16 - frame->header.bits_per_sample ;
				if (shift < 0)
				{	shift = abs (shift) ;
					for (i = 0 ; i < frame->header.blocksize && pflac->remain > 0 ; i++)
					{	offset = pflac->pos + i * frame->header.channels ;
						for (j = 0 ; j < frame->header.channels ; j++)
							retpcm [offset + j] = buffer [j][pflac->bufferpos] >> shift ;
						pflac->remain -= frame->header.channels ;
						pflac->bufferpos++ ;
						}
					}
				else
				{	for (i = 0 ; i < frame->header.blocksize && pflac->remain > 0 ; i++)
					{	offset = pflac->pos + i * frame->header.channels ;

						if (pflac->bufferpos >= frame->header.blocksize)
							break ;

						for (j = 0 ; j < frame->header.channels ; j++)
							retpcm [offset + j] = (buffer [j][pflac->bufferpos]) << shift ;

						pflac->remain -= frame->header.channels ;
						pflac->bufferpos++ ;
						} ;
					} ;
				} ;
			break ;

		case PFLAC_PCM_INT :
			{	int *retpcm = ((int*) pflac->ptr) ;
				int shift = 32 - frame->header.bits_per_sample ;
				for (i = 0 ; i < frame->header.blocksize && pflac->remain > 0 ; i++)
				{	offset = pflac->pos + i * frame->header.channels ;

					if (pflac->bufferpos >= frame->header.blocksize)
						break ;

					for (j = 0 ; j < frame->header.channels ; j++)
						retpcm [offset + j] = buffer [j][pflac->bufferpos] << shift ;
					pflac->remain -= frame->header.channels ;
					pflac->bufferpos++ ;
					} ;
				} ;
			break ;

		case PFLAC_PCM_FLOAT :
			{	float *retpcm = ((float*) pflac->ptr) ;
				float norm = (psf->norm_float == SF_TRUE) ? 1.0 / (1 << (frame->header.bits_per_sample - 1)) : 1.0 ;

				for (i = 0 ; i < frame->header.blocksize && pflac->remain > 0 ; i++)
				{	offset = pflac->pos + i * frame->header.channels ;

					if (pflac->bufferpos >= frame->header.blocksize)
						break ;

					for (j = 0 ; j < frame->header.channels ; j++)
						retpcm [offset + j] = buffer [j][pflac->bufferpos] * norm ;
					pflac->remain -= frame->header.channels ;
					pflac->bufferpos++ ;
					} ;
				} ;
			break ;

		case PFLAC_PCM_DOUBLE :
			{	double *retpcm = ((double*) pflac->ptr) ;
				double norm = (psf->norm_double == SF_TRUE) ? 1.0 / (1 << (frame->header.bits_per_sample - 1)) : 1.0 ;

				for (i = 0 ; i < frame->header.blocksize && pflac->remain > 0 ; i++)
				{	offset = pflac->pos + i * frame->header.channels ;

					if (pflac->bufferpos >= frame->header.blocksize)
						break ;

					for (j = 0 ; j < frame->header.channels ; j++)
						retpcm [offset + j] = buffer [j][pflac->bufferpos] * norm ;
					pflac->remain -= frame->header.channels ;
					pflac->bufferpos++ ;
					} ;
				} ;
			break ;

		default :
			return 0 ;
		} ;

	offset = i * frame->header.channels ;
	pflac->pos += i * frame->header.channels ;

	return offset ;
} /* flac_buffer_copy */


static FLAC__SeekableStreamDecoderReadStatus
sf_flac_read_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), FLAC__byte buffer [], unsigned *bytes, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	*bytes = psf_fread (buffer, 1, *bytes, psf) ;
	if (*bytes > 0 && psf->error == 0)
		return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_OK ;

    return FLAC__SEEKABLE_STREAM_DECODER_READ_STATUS_ERROR ;
} /* sf_flac_read_callback */

static FLAC__SeekableStreamDecoderSeekStatus
sf_flac_seek_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), FLAC__uint64 absolute_byte_offset, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	psf_fseek (psf, absolute_byte_offset, SEEK_SET) ;
	if (psf->error)
		return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_ERROR ;

	return FLAC__SEEKABLE_STREAM_DECODER_SEEK_STATUS_OK ;
} /* sf_flac_seek_callback */

static FLAC__SeekableStreamDecoderTellStatus
sf_flac_tell_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), FLAC__uint64 *absolute_byte_offset, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	*absolute_byte_offset = psf_ftell (psf) ;
	if (psf->error)
		return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_ERROR ;

	return FLAC__SEEKABLE_STREAM_DECODER_TELL_STATUS_OK ;
} /* sf_flac_tell_callback */

static FLAC__SeekableStreamDecoderLengthStatus
sf_flac_length_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), FLAC__uint64 *stream_length, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	if ((*stream_length = psf->filelength) == 0)
		return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_ERROR ;

	return FLAC__SEEKABLE_STREAM_DECODER_LENGTH_STATUS_OK ;
} /* sf_flac_length_callback */

static FLAC__bool
sf_flac_eof_callback (const FLAC__SeekableStreamDecoder *UNUSED (decoder), void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	if (psf_ftell (psf) == psf->filelength)
		return SF_TRUE ;

    return SF_FALSE ;
} /* sf_flac_eof_callback */

static FLAC__StreamDecoderWriteStatus
sf_flac_write_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), const FLAC__Frame *frame, const FLAC__int32 * const buffer [], void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;
	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;

	pflac->frame = frame ;
	pflac->bufferpos = 0 ;

	pflac->bufferbackup = SF_FALSE ;
	pflac->wbuffer = buffer ;

	flac_buffer_copy (psf) ;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE ;
} /* sf_flac_write_callback */

static void
sf_flac_meta_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), const FLAC__StreamMetadata *metadata, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	switch (metadata->type)
	{	case FLAC__METADATA_TYPE_STREAMINFO :
			psf->sf.channels = metadata->data.stream_info.channels ;
			psf->sf.samplerate = metadata->data.stream_info.sample_rate ;
			psf->sf.frames = metadata->data.stream_info.total_samples ;

			switch (metadata->data.stream_info.bits_per_sample)
			{	case 8 :
					psf->sf.format |= SF_FORMAT_PCM_S8 ;
					break ;
				case 16 :
					psf->sf.format |= SF_FORMAT_PCM_16 ;
					break ;
				case 24 :
					psf->sf.format |= SF_FORMAT_PCM_24 ;
					break ;
				default :
					psf_log_printf (psf, "sf_flac_meta_callback : bits_per_sample %d not yet implemented.\n", metadata->data.stream_info.bits_per_sample) ;
					break ;
				} ;
			break ;

		default :
			psf_log_printf (psf, "sf_flac_meta_callback : metadata-type %d not yet implemented.\n", metadata->type) ;
		break ;
		} ;

	return ;
} /* sf_flac_meta_callback */

static void
sf_flac_error_callback (const FLAC__SeekableStreamDecoder * UNUSED (decoder), FLAC__StreamDecoderErrorStatus status, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	psf_log_printf (psf, "ERROR : %s\n", FLAC__StreamDecoderErrorStatusString [status]) ;

	switch (status)
	{	case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC :
			psf->error = SFE_FLAC_LOST_SYNC ;
			break ;
		case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER :
			psf->error = SFE_FLAC_BAD_HEADER ;
			break ;
		default :
			psf->error = SFE_FLAC_UNKOWN_ERROR ;
			break ;
		} ;

	return ;
} /* sf_flac_error_callback */

static FLAC__SeekableStreamEncoderSeekStatus
sf_flac_enc_seek_callback (const FLAC__SeekableStreamEncoder * UNUSED (encoder), FLAC__uint64 absolute_byte_offset, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	psf_fseek (psf, absolute_byte_offset, SEEK_SET) ;
	if (psf->error)
		return FLAC__SEEKABLE_STREAM_ENCODER_SEEK_STATUS_ERROR ;

    return FLAC__SEEKABLE_STREAM_ENCODER_SEEK_STATUS_OK ;
} /* sf_flac_enc_seek_callback */

#ifdef HAVE_FLAC_1_1_1
static FLAC__SeekableStreamEncoderTellStatus
sf_flac_enc_tell_callback (const FLAC__SeekableStreamEncoder *UNUSED (encoder), FLAC__uint64 *absolute_byte_offset, void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	*absolute_byte_offset = psf_ftell (psf) ;
	if (psf->error)
		return FLAC__SEEKABLE_STREAM_ENCODER_TELL_STATUS_ERROR ;

	return FLAC__SEEKABLE_STREAM_ENCODER_TELL_STATUS_OK ;
} /* sf_flac_enc_tell_callback */
#endif

static FLAC__StreamEncoderWriteStatus
sf_flac_enc_write_callback (const FLAC__SeekableStreamEncoder * UNUSED (encoder), const FLAC__byte buffer [], unsigned bytes, unsigned UNUSED (samples), unsigned UNUSED (current_frame), void *client_data)
{	SF_PRIVATE *psf = (SF_PRIVATE*) client_data ;

	if (psf_fwrite (buffer, 1, bytes, psf) == bytes && psf->error == 0)
		return FLAC__STREAM_ENCODER_WRITE_STATUS_OK ;

	return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR ;
} /* sf_flac_enc_write_callback */

/*------------------------------------------------------------------------------
** Public function.
*/

int
flac_open	(SF_PRIVATE *psf)
{	int		subformat ;
	int		error = 0 ;

	FLAC_PRIVATE* pflac = calloc (1, sizeof (FLAC_PRIVATE)) ;
	psf->fdata = pflac ;

	if (psf->mode == SFM_RDWR)
		return SFE_UNIMPLEMENTED ;

	if (psf->mode == SFM_READ)
	{	if ((error = flac_read_header (psf)))
			return error ;
		} ;

	subformat = psf->sf.format & SF_FORMAT_SUBMASK ;

	if (psf->mode == SFM_WRITE)
	{	if ((psf->sf.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_FLAC)
			return	SFE_BAD_OPEN_FORMAT ;

		psf->endian = SF_ENDIAN_BIG ;

		if ((error = flac_enc_init (psf)))
			return error ;
		} ;

	psf->datalength = psf->filelength ;
	psf->dataoffset = 0 ;
	psf->blockwidth = 0 ;
	psf->bytewidth = 1 ;

	psf->container_close = flac_close ;
	psf->seek = flac_seek ;
	psf->command = flac_command ;

	psf->blockwidth = psf->bytewidth * psf->sf.channels ;

	switch (subformat)
	{	case SF_FORMAT_PCM_S8 :		/* 8-bit FLAC.  */
		case SF_FORMAT_PCM_16 :	/* 16-bit FLAC. */
		case SF_FORMAT_PCM_24 :	/* 24-bit FLAC. */
			error = flac_init (psf) ;
			break ;

		default : return SFE_UNIMPLEMENTED ;
		} ;

	return error ;
} /* flac_open */

/*------------------------------------------------------------------------------
*/

static int
flac_close	(SF_PRIVATE *psf)
{	FLAC_PRIVATE* pflac ;
	int k ;

	if ((pflac = (FLAC_PRIVATE*) psf->fdata) == NULL)
		return 0 ;

	if (psf->mode == SFM_WRITE)
	{	FLAC__seekable_stream_encoder_finish (pflac->fse) ;
		FLAC__seekable_stream_encoder_delete (pflac->fse) ;
		if (pflac->encbuffer)
			free (pflac->encbuffer) ;
		} ;

	if (psf->mode == SFM_READ)
	{	FLAC__seekable_stream_decoder_finish (pflac->fsd) ;
		FLAC__seekable_stream_decoder_delete (pflac->fsd) ;
		} ;

	for (k = 0 ; k < ARRAY_LEN (pflac->rbuffer) ; k++)
		free (pflac->rbuffer [k]) ;

	free (pflac) ;
	psf->fdata = NULL ;

	return 0 ;
} /* flac_close */

static int
flac_enc_init (SF_PRIVATE *psf)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	unsigned bps ;
	int k, found ;

	found = 0 ;
	for (k = 0 ; k < ARRAY_LEN (legal_sample_rates) ; k++)
		if (psf->sf.samplerate == legal_sample_rates [k])
		{	found = 1 ;
			break ;
			} ;

	if (found == 0)
		return SFE_FLAC_BAD_SAMPLE_RATE ;

	psf_fseek (psf, 0, SEEK_SET) ;
	if ((pflac->fse = FLAC__seekable_stream_encoder_new ()) == NULL)
		return SFE_FLAC_NEW_DECODER ;
	FLAC__seekable_stream_encoder_set_write_callback (pflac->fse, sf_flac_enc_write_callback) ;
	FLAC__seekable_stream_encoder_set_seek_callback (pflac->fse, sf_flac_enc_seek_callback) ;

#ifdef HAVE_FLAC_1_1_1
	FLAC__seekable_stream_encoder_set_tell_callback (pflac->fse, sf_flac_enc_tell_callback) ;
#endif
	FLAC__seekable_stream_encoder_set_client_data (pflac->fse, psf) ;
	FLAC__seekable_stream_encoder_set_channels (pflac->fse, psf->sf.channels) ;
	FLAC__seekable_stream_encoder_set_sample_rate (pflac->fse, psf->sf.samplerate) ;

	switch (psf->sf.format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_PCM_S8 :
			bps = 8 ;
			break ;
		case SF_FORMAT_PCM_16 :
			bps = 16 ;
			break ;
		case SF_FORMAT_PCM_24 :
			bps = 24 ;
			break ;

		default :
			bps = 0 ;
			break ;
		} ;

	FLAC__seekable_stream_encoder_set_bits_per_sample (pflac->fse, bps) ;

	if ((bps = FLAC__seekable_stream_encoder_init (pflac->fse)) != FLAC__SEEKABLE_STREAM_DECODER_OK)
	{	psf_log_printf (psf, "Error : FLAC encoder init returned error : %s\n", FLAC__seekable_stream_encoder_get_resolved_state_string (pflac->fse)) ;
		return SFE_FLAC_INIT_DECODER ;
		} ;

	if (psf->error == 0)
		psf->dataoffset = psf_ftell (psf) ;
	pflac->encbuffer = calloc (ENC_BUFFER_SIZE, sizeof (FLAC__int32)) ;

	return psf->error ;
} /* flac_enc_init */

static int
flac_read_header (SF_PRIVATE *psf)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;

	psf_fseek (psf, 0, SEEK_SET) ;
	if ((pflac->fsd = FLAC__seekable_stream_decoder_new ()) == NULL)
		return SFE_FLAC_NEW_DECODER ;

	FLAC__seekable_stream_decoder_set_read_callback (pflac->fsd, sf_flac_read_callback) ;
	FLAC__seekable_stream_decoder_set_seek_callback (pflac->fsd, sf_flac_seek_callback) ;
	FLAC__seekable_stream_decoder_set_tell_callback (pflac->fsd, sf_flac_tell_callback) ;
	FLAC__seekable_stream_decoder_set_length_callback (pflac->fsd, sf_flac_length_callback) ;
	FLAC__seekable_stream_decoder_set_eof_callback (pflac->fsd, sf_flac_eof_callback) ;
	FLAC__seekable_stream_decoder_set_write_callback (pflac->fsd, sf_flac_write_callback) ;
	FLAC__seekable_stream_decoder_set_metadata_callback (pflac->fsd, sf_flac_meta_callback) ;
	FLAC__seekable_stream_decoder_set_error_callback (pflac->fsd, sf_flac_error_callback) ;
	FLAC__seekable_stream_decoder_set_client_data (pflac->fsd, psf) ;

	if (FLAC__seekable_stream_decoder_init (pflac->fsd) != FLAC__SEEKABLE_STREAM_DECODER_OK)
		return SFE_FLAC_INIT_DECODER ;

	FLAC__seekable_stream_decoder_process_until_end_of_metadata (pflac->fsd) ;
	if (psf->error == 0)
	{	FLAC__uint64 position ;
		FLAC__seekable_stream_decoder_get_decode_position (pflac->fsd, &position) ;
		psf->dataoffset = position ;
		} ;

	return psf->error ;
} /* flac_read_header */

static int
flac_command (SF_PRIVATE *psf, int command, void *data, int datasize)
{
	/* Avoid compiler warnings. */
	psf = psf ;
	data = data ;
	datasize = datasize ;

	switch (command)
	{	default : break ;
		} ;

	return 0 ;
} /* flac_command */

int
flac_init (SF_PRIVATE *psf)
{
	if (psf->mode == SFM_RDWR)
		return SFE_BAD_MODE_RW ;

	if (psf->mode == SFM_READ)
	{	psf->read_short		= flac_read_flac2s ;
		psf->read_int		= flac_read_flac2i ;
		psf->read_float		= flac_read_flac2f ;
		psf->read_double	= flac_read_flac2d ;
		} ;

	if (psf->mode == SFM_WRITE)
	{	psf->write_short	= flac_write_s2flac ;
		psf->write_int		= flac_write_i2flac ;
		psf->write_float	= flac_write_f2flac ;
		psf->write_double	= flac_write_d2flac ;
		} ;

	psf->bytewidth = 1 ;
	psf->blockwidth = psf->sf.channels ;

	if (psf->filelength > psf->dataoffset)
		psf->datalength = (psf->dataend) ? psf->dataend - psf->dataoffset : psf->filelength - psf->dataoffset ;
	else
		psf->datalength = 0 ;

	return 0 ;
} /* flac_init */

static unsigned
flac_read_loop (SF_PRIVATE *psf, unsigned len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;

	pflac->pos = 0 ;
	pflac->len = len ;
	pflac->remain = len ;
	if (pflac->frame != NULL && pflac->bufferpos < pflac->frame->header.blocksize)
		flac_buffer_copy (psf) ;

	while (pflac->pos < pflac->len)
	{	if (FLAC__seekable_stream_decoder_process_single (pflac->fsd) == 0)
			break ;
		if (FLAC__seekable_stream_decoder_get_state (pflac->fsd) != FLAC__SEEKABLE_STREAM_DECODER_OK)
			break ;
		} ;

	pflac->ptr = NULL ;

	return pflac->pos ;
} /* flac_read_loop */

static sf_count_t
flac_read_flac2s (SF_PRIVATE *psf, short *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	sf_count_t total = 0, current ;
	unsigned readlen ;

	pflac->pcmtype = PFLAC_PCM_SHORT ;

	while (total < len)
	{	pflac->ptr = ptr + total ;
		readlen = (len - total > 0x1000000) ? 0x1000000 : (unsigned) (len - total) ;
		current = flac_read_loop (psf, readlen) ;
		if (current == 0)
			break ;
		total += current ;
		} ;

	return total ;
} /* flac_read_flac2s */

static sf_count_t
flac_read_flac2i (SF_PRIVATE *psf, int *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	sf_count_t total = 0, current ;
	unsigned readlen ;

	pflac->pcmtype = PFLAC_PCM_INT ;

	while (total < len)
	{	pflac->ptr = ptr + total ;
		readlen = (len - total > 0x1000000) ? 0x1000000 : (unsigned) (len - total) ;
		current = flac_read_loop (psf, readlen) ;
		if (current == 0)
			break ;
		total += current ;
		} ;

	return total ;
} /* flac_read_flac2i */

static sf_count_t
flac_read_flac2f (SF_PRIVATE *psf, float *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	sf_count_t total = 0, current ;
	unsigned readlen ;

	pflac->pcmtype = PFLAC_PCM_FLOAT ;

	while (total < len)
	{	pflac->ptr = ptr + total ;
		readlen = (len - total > 0x1000000) ? 0x1000000 : (unsigned) (len - total) ;
		current = flac_read_loop (psf, readlen) ;
		if (current == 0)
			break ;
		total += current ;
		} ;

	return total ;
} /* flac_read_flac2f */

static sf_count_t
flac_read_flac2d (SF_PRIVATE *psf, double *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	sf_count_t total = 0, current ;
	unsigned readlen ;

	pflac->pcmtype = PFLAC_PCM_DOUBLE ;

	while (total < len)
	{	pflac->ptr = ptr + total ;
		readlen = (len - total > 0x1000000) ? 0x1000000 : (unsigned) (len - total) ;
		current = flac_read_loop (psf, readlen) ;
		if (current == 0)
			break ;
		total += current ;
		} ;

	return total ;
} /* flac_read_flac2d */

static sf_count_t
flac_write_s2flac (SF_PRIVATE *psf, const short *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	void (*convert) (const short *, FLAC__int32 *, int) ;
	int bufferlen, writecount, thiswrite ;
	sf_count_t	total = 0 ;
	FLAC__int32* buffer = pflac->encbuffer ;

	switch (psf->sf.format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_PCM_S8 :
			convert = s2flac8_array ;
			break ;
		case SF_FORMAT_PCM_16 :
			convert = s2flac16_array ;
			break ;
			case SF_FORMAT_PCM_24 :
			convert = s2flac24_array ;
			break ;
		default :
			return -1 ;
		} ;

	bufferlen = ENC_BUFFER_SIZE / (sizeof (FLAC__int32) * psf->sf.channels) ;
	bufferlen *= psf->sf.channels ;

	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		convert (ptr + total, buffer, writecount) ;
		if (FLAC__seekable_stream_encoder_process_interleaved (pflac->fse, buffer, writecount/psf->sf.channels))
			thiswrite = writecount ;
		else
			break ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;

		len -= thiswrite ;
		} ;

	return total ;
} /* flac_write_s2flac */

static sf_count_t
flac_write_i2flac (SF_PRIVATE *psf, const int *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	void (*convert) (const int *, FLAC__int32 *, int) ;
	int bufferlen, writecount, thiswrite ;
	sf_count_t	total = 0 ;
	FLAC__int32* buffer = pflac->encbuffer ;

	switch (psf->sf.format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_PCM_S8 :
			convert = i2flac8_array ;
			break ;
		case SF_FORMAT_PCM_16 :
			convert = i2flac16_array ;
			break ;
		case SF_FORMAT_PCM_24 :
			convert = i2flac24_array ;
			break ;
		default :
			return -1 ;
		} ;

	bufferlen = ENC_BUFFER_SIZE / (sizeof (FLAC__int32) * psf->sf.channels) ;
	bufferlen *= psf->sf.channels ;

	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		convert (ptr + total, buffer, writecount) ;
		if (FLAC__seekable_stream_encoder_process_interleaved (pflac->fse, buffer, writecount/psf->sf.channels))
			thiswrite = writecount ;
		else
			break ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;

		len -= thiswrite ;
		} ;

	return total ;
} /* flac_write_i2flac */

static sf_count_t
flac_write_f2flac (SF_PRIVATE *psf, const float *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	void (*convert) (const float *, FLAC__int32 *, int, int) ;
	int bufferlen, writecount, thiswrite ;
	sf_count_t	total = 0 ;
	FLAC__int32* buffer = pflac->encbuffer ;

	switch (psf->sf.format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_PCM_S8 :
			convert = (psf->add_clipping) ? f2flac8_clip_array : f2flac8_array ;
			break ;
		case SF_FORMAT_PCM_16 :
			convert = (psf->add_clipping) ? f2flac16_clip_array : f2flac16_array ;
			break ;
		case SF_FORMAT_PCM_24 :
			convert = (psf->add_clipping) ? f2flac24_clip_array : f2flac24_array ;
			break ;
		default :
			return -1 ;
		} ;

	bufferlen = ENC_BUFFER_SIZE / (sizeof (FLAC__int32) * psf->sf.channels) ;
	bufferlen *= psf->sf.channels ;

	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		convert (ptr + total, buffer, writecount, psf->norm_float) ;
		if (FLAC__seekable_stream_encoder_process_interleaved (pflac->fse, buffer, writecount/psf->sf.channels))
			thiswrite = writecount ;
		else
			break ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;

		len -= thiswrite ;
		} ;

	return total ;
} /* flac_write_f2flac */

static void
f2flac8_clip_array (const float *src, FLAC__int32 *dest, int count, int normalize)
{	float normfact, scaled_value ;

	normfact = normalize ? (8.0 * 0x10) : 1.0 ;

	while (--count >= 0)
	{	scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7F))
		{	dest [count] = 0x7F ;
			continue ;
			} ;
		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x10))
		{	dest [count] = 0x80 ;
			continue ;
			} ;
		dest [count] = lrintf (scaled_value) ;
		} ;

	return ;
} /* f2flac8_clip_array */

static void
f2flac16_clip_array (const float *src, FLAC__int32 *dest, int count, int normalize)
{
  float normfact, scaled_value ;

  normfact = normalize ? (8.0 * 0x1000) : 1.0 ;

  while (--count >= 0) {
    scaled_value = src [count] * normfact ;
    if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7FFF)) {
      dest [count] = 0x7FFF ;
      continue ;
    }
    if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x1000)) {
      dest [count] = 0x8000 ;
      continue ;
    }
    dest [count] = lrintf (scaled_value) ;
  }
} /* f2flac16_clip_array */

static void
f2flac24_clip_array (const float *src, FLAC__int32 *dest, int count, int normalize)
{	float normfact, scaled_value ;

	normfact = normalize ? (8.0 * 0x100000) : 1.0 ;

	while (--count >= 0)
	{	scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7FFFFF))
		{	dest [count] = 0x7FFFFF ;
			continue ;
			} ;

		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x100000))
		{	dest [count] = 0x800000 ;
			continue ;
			}
		dest [count] = lrintf (scaled_value) ;
		} ;

	return ;
} /* f2flac24_clip_array */

static void
f2flac8_array (const float *src, FLAC__int32 *dest, int count, int normalize)
{	float normfact = normalize ? (1.0 * 0x7F) : 1.0 ;

	while (--count >= 0)
		dest [count] = lrintf (src [count] * normfact) ;
} /* f2flac8_array */

static void
f2flac16_array (const float *src, FLAC__int32 *dest, int count, int normalize)
{	float normfact = normalize ? (1.0 * 0x7FFF) : 1.0 ;

	while (--count >= 0)
		dest [count] = lrintf (src [count] * normfact) ;
} /* f2flac16_array */

static void
f2flac24_array (const float *src, FLAC__int32 *dest, int count, int normalize)
{	float normfact = normalize ? (1.0 * 0x7FFFFF) : 1.0 ;

	while (--count >= 0)
		dest [count] = lrintf (src [count] * normfact) ;
} /* f2flac24_array */

static sf_count_t
flac_write_d2flac (SF_PRIVATE *psf, const double *ptr, sf_count_t len)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;
	void (*convert) (const double *, FLAC__int32 *, int, int) ;
	int bufferlen, writecount, thiswrite ;
	sf_count_t	total = 0 ;
	FLAC__int32* buffer = pflac->encbuffer ;

	switch (psf->sf.format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_PCM_S8 :
			convert = (psf->add_clipping) ? d2flac8_clip_array : d2flac8_array ;
			break ;
		case SF_FORMAT_PCM_16 :
			convert = (psf->add_clipping) ? d2flac16_clip_array : d2flac16_array ;
			break ;
		case SF_FORMAT_PCM_24 :
			convert = (psf->add_clipping) ? d2flac24_clip_array : d2flac24_array ;
			break ;
		default :
			return -1 ;
		} ;

	bufferlen = ENC_BUFFER_SIZE / (sizeof (FLAC__int32) * psf->sf.channels) ;
	bufferlen *= psf->sf.channels ;

	while (len > 0)
	{	writecount = (len >= bufferlen) ? bufferlen : (int) len ;
		convert (ptr + total, buffer, writecount, psf->norm_double) ;
		if (FLAC__seekable_stream_encoder_process_interleaved (pflac->fse, buffer, writecount/psf->sf.channels))
			thiswrite = writecount ;
		else
			break ;
		total += thiswrite ;
		if (thiswrite < writecount)
			break ;

		len -= thiswrite ;
		} ;

	return total ;
} /* flac_write_d2flac */

static void
d2flac8_clip_array (const double *src, FLAC__int32 *dest, int count, int normalize)
{	double normfact, scaled_value ;

	normfact = normalize ? (8.0 * 0x10) : 1.0 ;

	while (--count >= 0)
	{	scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7F))
		{	dest [count] = 0x7F ;
			continue ;
			} ;
		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x10))
		{	dest [count] = 0x80 ;
			continue ;
			} ;
		dest [count] = lrint (scaled_value) ;
		} ;

	return ;
} /* d2flac8_clip_array */

static void
d2flac16_clip_array (const double *src, FLAC__int32 *dest, int count, int normalize)
{	double normfact, scaled_value ;

	normfact = normalize ? (8.0 * 0x1000) : 1.0 ;

	while (--count >= 0)
	{	scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7FFF))
		{	dest [count] = 0x7FFF ;
			continue ;
			} ;
		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x1000))
		{	dest [count] = 0x8000 ;
			continue ;
			} ;
		dest [count] = lrint (scaled_value) ;
		} ;

	return ;
} /* d2flac16_clip_array */

static void
d2flac24_clip_array (const double *src, FLAC__int32 *dest, int count, int normalize)
{	double normfact, scaled_value ;

	normfact = normalize ? (8.0 * 0x100000) : 1.0 ;

	while (--count >= 0)
	{	scaled_value = src [count] * normfact ;
		if (CPU_CLIPS_POSITIVE == 0 && scaled_value >= (1.0 * 0x7FFFFF))
		{	dest [count] = 0x7FFFFF ;
			continue ;
			} ;
		if (CPU_CLIPS_NEGATIVE == 0 && scaled_value <= (-8.0 * 0x100000))
		{	dest [count] = 0x800000 ;
			continue ;
			} ;
		dest [count] = lrint (scaled_value) ;
		} ;

	return ;
} /* d2flac24_clip_array */

static void
d2flac8_array (const double *src, FLAC__int32 *dest, int count, int normalize)
{	double normfact = normalize ? (1.0 * 0x7F) : 1.0 ;

	while (--count >= 0)
		dest [count] = lrint (src [count] * normfact) ;
} /* d2flac8_array */

static void
d2flac16_array (const double *src, FLAC__int32 *dest, int count, int normalize)
{	double normfact = normalize ? (1.0 * 0x7FFF) : 1.0 ;

	while (--count >= 0)
		dest [count] = lrint (src [count] * normfact) ;
} /* d2flac16_array */

static void
d2flac24_array (const double *src, FLAC__int32 *dest, int count, int normalize)
{	double normfact = normalize ? (1.0 * 0x7FFFFF) : 1.0 ;

	while (--count >= 0)
		dest [count] = lrint (src [count] * normfact) ;
} /* d2flac24_array */

static sf_count_t
flac_seek (SF_PRIVATE *psf, int UNUSED (mode), sf_count_t offset)
{	FLAC_PRIVATE* pflac = (FLAC_PRIVATE*) psf->fdata ;

	if (pflac == NULL)
		return 0 ;

	if (psf->dataoffset < 0)
	{	psf->error = SFE_BAD_SEEK ;
		return ((sf_count_t) -1) ;
		} ;

	pflac->frame = NULL ;

	if (psf->mode == SFM_READ)
	{	FLAC__uint64 position ;
		if (FLAC__seekable_stream_decoder_seek_absolute (pflac->fsd, offset))
		{	FLAC__seekable_stream_decoder_get_decode_position (pflac->fsd, &position) ;
			return offset ;
			} ;

		return ((sf_count_t) -1) ;
		} ;

	/* Seeking in write mode not yet supported. */
	psf->error = SFE_BAD_SEEK ;

	return ((sf_count_t) -1) ;
} /* flac_seek */

#endif

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 46d49617-ebff-42b4-8f66-a0e428147360
*/
