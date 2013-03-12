/**
   @file ltc.h
   @brief libltc - en+decode linear timecode

   Linear (or Longitudinal) Timecode (LTC) is an encoding of
   timecode data as a Manchester-Biphase encoded audio signal.
   The audio signal is commonly recorded on a VTR track or other
   storage media.

   libltc facilitates decoding and encoding of LTC from/to
   timecode, including SMPTE date support.

   @author Robin Gareus <robin@gareus.org>
   @copyright

   Copyright (C) 2006-2012 Robin Gareus <robin@gareus.org>

   Copyright (C) 2008-2009 Jan Weiß <jan@geheimwerk.de>

   Inspired by SMPTE Decoder - Maarten de Boer <mdeboer@iua.upf.es>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.
   If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef LTC_H
#define LTC_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* size_t */

#ifndef DOXYGEN_IGNORE
/* libltc version */
#define LIBLTC_VERSION "1.1.1"
#define LIBLTC_VERSION_MAJOR  1
#define LIBLTC_VERSION_MINOR  1
#define LIBLTC_VERSION_MICRO  1

/* interface revision number
 * http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
 */
#define LIBLTC_CUR 11
#define LIBLTC_REV  1
#define LIBLTC_AGE  0
#endif /* end DOXYGEN_IGNORE */

/**
 * default audio sample type: 8bit unsigned (mono)
 */
typedef unsigned char ltcsnd_sample_t;

/**
 * sample-count offset - 64bit wide
 */
typedef long long int ltc_off_t;

#define LTC_FRAME_BIT_COUNT	80

/**
 * Raw 80 bit LTC frame
 *
 * The datastream for each video frame of Longitudinal Timecode consists of eighty bit-periods.
 *
 * At a frame-rate of 30 fps, the bit-rate corresponds to 30 [fps] * 80 [bits/f] = 2400 bits per second.
 * The frequency for a stream of zeros would be 1.2 kHz and for a stream of ones it would be 2.4 kHz.
 * \image html smptefmt.png
 * With all commonly used video-frame-rates and audio-sample-rates,  LTC timecode can be recorded
 * easily into a audio-track.
 *
 * In each frame, 26 of the eighty bits carry the SMPTE time in binary coded decimal (BCD).
 *
 * These Bits are FRAME-UNITS, FRAME-TENS, SECS-UNITS, SECS-TENS, MINS-UNITS, MINS-TENS, HOURS-UNITS and HOURS-TENS.
 * The BCD digits are loaded 'least significant bit first' (libltc takes care of the architecture specific alignment).
 *
 * 32 bits are assigned as eight groups of four USER-BITS (also sometimes called the "Binary Groups").
 * This capacity is generally used to carry extra info such as reel number and/or date.
 * The User Bits may be allocated howsoever one wishes as long as both Binary Group Flag Bits are cleared.
 *
 * The function \ref ltc_frame_to_time can interpret the user-bits as SMPTE Date+Timezone according to SMPTE 309M-1999.
 * similarly \ref ltc_time_to_frame will do the reverse.
 *
 * The last 16 Bits make up the SYNC WORD. These bits indicate the frame boundary, the tape direction, and the bit-rate of the sync tone.
 * The values of these Bits are fixed as 0011 1111 1111 1101
 *
 * The Bi-Phase Mark Phase Correction Bit (Bit 27 or 59) may be set or cleared so that that every 80-bit word
 * contains an even number of zeroes. This means that the phase of the pulse train in every Sync Word will be the same.
 *
 * Bit 10 indicates drop-frame timecode.
 * The Colour Frame Flag col.frm is Bit 11; if the timecode intentionally synchronized to a colour TV field sequence, this bit is set.
 *
 * Bit 58 is not required for the BCD count for HOURS-TENS (which has a maximum value of two)
 * and has not been given any other special purpose so remains unassigned.
 * This Bit has been RESERVED for future assignment.
 *
 * The Binary Group Flag Bits (bits 43 and 59) are two bits indicate the format of the User Bits data.
 * SMPTE 12M-1999 defines the previously reserved bit 58 to signals that the time is locked to wall-clock
 * within a tolerance of ± 0.5 seconds.
 *
 * SMPTE 12M-1999 also changes the numbering schema of the BGF. (BGF1 was renamed to BGF2 and bit 58 becomes BGFB1)
 *
 * To further complicate matters, the BGFB assignment as well as the biphase_mark_phase_correction (aka parity)
 * bit depends on the timecode-format used.
 *
 * <pre>
 *          25 fps   24, 30 fps
 *  BGF0      27        43
 *  BGF1      58        58
 *  BGF2      43        59
 *  Parity    59        27
 * </pre>
 *
 * The variable naming chosen for the LTCFrame struct is based on the 24,30 fps standard.
 *
 * The Binary Group Flag Bits should be used only as shown in the truth table below.
 * The Unassigned entries in the table should not be used, as they may be allocated specific meanings in the future.
 *
 * <pre>
 *                                                 BGF0      BGF1    BGF2
 *       user-bits                     timecode    Bit 43   Bit 58  Bit 59 (30fps, 24 fps)
 *                                    |        |   Bit 27   Bit 58  Bit 43 (25fps)
 *  No User Bits format specified     |   ?    |     0       0        0
 *  Eight-bit character set (1)       |   ?    |     1       0        0
 *  Date and Timezone set             |   ?    |     0       0        1
 *  Page/Line multiplex (2)           |   ?    |     1       0        1
 *  Character set not specified       |  clk   |     0       1        0
 *  Reserved                          |   ?    |     1       1        0
 *  Date and Timezone set             |  clk   |     0       1        1
 *  Page/Line multiplex (2)           |  clk   |     1       1        1
 *
 * </pre>
 *
 * (1) ISO/IEC 646 or ISO/IEC 2022 character set.
 * If the seven-bit ISO codes are being used, they shall be converted to
 * eight-bit codes by setting the eighth bit to zero. 4 ISO codes can be encoded,
 * user7 and user8 are to be used for the first code with LSB 7 and MSB in 8.
 * the remaining ISO codes are to be distributed in the same manner to
 * user5/6 user3/4 and user1/2 accordingly.
 *
 * (2) The Page/Line indicates ANSI/SMPTE-262M is used for the user-bits. It is multiplex system that
 * can be used to encode large amounts of data in the binary groups through the use of time multiplexing.
 *
 * libltc does not use any of the BGF - except for the Parity bit which can be calculated and set with
 * \ref ltc_frame_set_parity. Setting and interpreting the BGF is left to the application using libltc.
 * However libltc provides functionality to parse or set date and timezoe according to SMPTE 309M-1999.
 *
 * further information: http://www.philrees.co.uk/articles/timecode.htm
 * and http://www.barney-wol.net/time/timecode.html
 */
#if (defined __BIG_ENDIAN__ && !defined DOXYGEN_IGNORE)
// Big Endian version, bytes are "upside down"
struct LTCFrame {
	unsigned int user1:4;
	unsigned int frame_units:4;

	unsigned int user2:4;
	unsigned int col_frame:1;
	unsigned int dfbit:1;
	unsigned int frame_tens:2;

	unsigned int user3:4;
	unsigned int secs_units:4;

	unsigned int user4:4;
	unsigned int biphase_mark_phase_correction:1;
	unsigned int secs_tens:3;

	unsigned int user5:4;
	unsigned int mins_units:4;

	unsigned int user6:4;
	unsigned int binary_group_flag_bit0:1;
	unsigned int mins_tens:3;

	unsigned int user7:4;
	unsigned int hours_units:4;

	unsigned int user8:4;
	unsigned int binary_group_flag_bit2:1;
	unsigned int binary_group_flag_bit1:1;
	unsigned int hours_tens:2;

	unsigned int sync_word:16;
};
#else
/* Little Endian version -- and doxygen doc */
struct LTCFrame {
	unsigned int frame_units:4; ///< SMPTE framenumber BCD unit 0..9
	unsigned int user1:4;

	unsigned int frame_tens:2; ///< SMPTE framenumber BCD tens 0..3
	unsigned int dfbit:1; ///< indicated drop-frame timecode
	unsigned int col_frame:1; ///< colour-frame: timecode intentionally synchronized to a colour TV field sequence
	unsigned int user2:4;

	unsigned int secs_units:4; ///< SMPTE seconds BCD unit 0..9
	unsigned int user3:4;

	unsigned int secs_tens:3; ///< SMPTE seconds BCD tens 0..6
	unsigned int biphase_mark_phase_correction:1; ///< see note on Bit 27 in description and \ref ltc_frame_set_parity .
	unsigned int user4:4;

	unsigned int mins_units:4; ///< SMPTE minutes BCD unit 0..9
	unsigned int user5:4;

	unsigned int mins_tens:3; ///< SMPTE minutes BCD tens 0..6
	unsigned int binary_group_flag_bit0:1; ///< indicate user-data char encoding, see table above - bit 43
	unsigned int user6:4;

	unsigned int hours_units:4; ///< SMPTE hours BCD unit 0..9
	unsigned int user7:4;

	unsigned int hours_tens:2; ///< SMPTE hours BCD tens 0..2
	unsigned int binary_group_flag_bit1:1; ///< indicate timecode is local time wall-clock, see table above - bit 58
	unsigned int binary_group_flag_bit2:1; ///< indicate user-data char encoding (or parity with 25fps), see table above - bit 59
	unsigned int user8:4;

	unsigned int sync_word:16;
};
#endif

/** the standard defines the assignment of the binary-group-flag bits
 * basically only 25fps is different, but other standards defined in
 * the SMPTE spec have been included for completeness.
 */
enum LTC_TV_STANDARD {
	LTC_TV_525_60, ///< 30fps
	LTC_TV_625_50, ///< 25fps
	LTC_TV_1125_60,///< 30fps
	LTC_TV_FILM_24 ///< 24fps
};

/** encoder and LTCframe <> timecode operation flags */
enum LTC_BG_FLAGS {
	LTC_USE_DATE  = 1, ///< LTCFrame <> SMPTETimecode converter and LTCFrame increment/decrement use date, also set BGF2 to '1' when encoder is initialized or re-initialized (unless LTC_BGF_DONT_TOUCH is given)
	LTC_TC_CLOCK  = 2,///< the Timecode is wall-clock aka freerun. This also sets BGF1 (unless LTC_BGF_DONT_TOUCH is given)
	LTC_BGF_DONT_TOUCH = 4, ///< encoder init or re-init does not touch the BGF bits (initial values after initialization is zero)
	LTC_NO_PARITY = 8 ///< parity bit is left untouched when setting or in/decrementing the encoder frame-number
};

/**
 * see LTCFrame
 */
typedef struct LTCFrame LTCFrame;

/**
 * Extended LTC frame - includes audio-sample position offsets, volume, etc
 *
 * Note: For TV systems, the sample in the LTC audio data stream where the LTC Frame starts is not neccesarily at the same time
 * as the video-frame which is described by the LTC Frame.
 *
 * \ref off_start denotes the time of the first transition of bit 0 in the LTC frame.
 *
 * For 525/60 Television systems, the first transition shall occur at the beginning of line 5 of the frame with which it is
 * associated. The tolerance is ± 1.5 lines.
 *
 * For 625/50 systems, the first transition shall occur at the beginning of line 2  ± 1.5 lines of the frame with which it is associated.
 *
 * Only for 1125/60 systems, the first transition occurs exactly at the vertical sync timing reference of the frame. ± 1 line.
 *
 */
struct LTCFrameExt {
	LTCFrame ltc; ///< the actual LTC frame. see \ref LTCFrame
	ltc_off_t off_start; ///< \anchor off_start the approximate sample in the stream corresponding to the start of the LTC frame.
	ltc_off_t off_end; ///< \anchor off_end the sample in the stream corresponding to the end of the LTC frame.
	int reverse; ///< if non-zero, a reverse played LTC frame was detected. Since the frame was reversed, it started at off_end and finishes as off_start (off_end > off_start). (Note: in reverse playback the (reversed) sync-word of the next/previous frame is detected, this offset is corrected).
	float biphase_tics[LTC_FRAME_BIT_COUNT]; ///< detailed timing info: phase of the LTC signal; the time between each bit in the LTC-frame in audio-frames. Summing all 80 values in the array will yield audio-frames/LTC-frame = (\ref off_end - \ref off_start + 1).
	ltcsnd_sample_t sample_min; ///< the minimum input sample signal for this frame (0..255)
	ltcsnd_sample_t sample_max; ///< the maximum input sample signal for this frame (0..255)
	double volume; ///< the volume of the input signal in dbFS
};

/**
 * see \ref LTCFrameExt
 */
typedef struct LTCFrameExt LTCFrameExt;

/**
 * Human readable time representation, decimal values.
 */
struct SMPTETimecode {
	char timezone[6];   ///< the timezone 6bytes: "+HHMM" textual representation
	unsigned char years; ///< LTC-date uses 2-digit year 00.99
	unsigned char months; ///< valid months are 1..12
	unsigned char days; ///< day of month 1..31

	unsigned char hours; ///< hour 0..23
	unsigned char mins; ///< minute 0..60
	unsigned char secs; ///< second 0..60
	unsigned char frame; ///< sub-second frame 0..(FPS - 1)
};

/**
 * see \ref SMPTETimecode
 */
typedef struct SMPTETimecode SMPTETimecode;


/**
 * Opaque structure
 * see: \ref ltc_decoder_create, \ref ltc_decoder_free
 */
typedef struct LTCDecoder LTCDecoder;

/**
 * Opaque structure
 * see: \ref ltc_encoder_create, \ref ltc_encoder_free
 */
typedef struct LTCEncoder LTCEncoder;

/**
 * Convert binary LTCFrame into SMPTETimecode struct
 *
 * @param stime output
 * @param frame input
 * @param flags binary combination of \ref LTC_BG_FLAGS - here only LTC_USE_DATE is relevant.
 * if LTC_USE_DATE is set, the user-fields in LTCFrame will be parsed into the date variable of SMPTETimecode.
 * otherwise the date information in the SMPTETimecode is set to zero.
 */
void ltc_frame_to_time(SMPTETimecode* stime, LTCFrame* frame, int flags);

/**
 * Translate SMPTETimecode struct into its binary LTC representation
 * and set the LTC frame's parity bit accordingly (see \ref ltc_frame_set_parity)
 *
 * @param frame output - the frame to be set
 * @param stime input - timecode input
 * @param standard the TV standard to use for parity bit assignment
 * @param flags binary combination of \ref LTC_BG_FLAGS - here only LTC_USE_DATE and LTC_NO_PARITY are relevant.
 * if LTC_USE_DATE is given, user-fields in LTCFrame will be set from the date in SMPTETimecode,
 * otherwise the user-bits are not modified. All non-timecode fields remain untouched - except for the parity bit
 * unless LTC_NO_PARITY is given.
 */
void ltc_time_to_frame(LTCFrame* frame, SMPTETimecode* stime, enum LTC_TV_STANDARD standard, int flags);

/**
 * Reset all values of a LTC FRAME to zero, except for the sync-word (0x3FFD) at the end.
 * The sync word is set according to architecture (big/little endian).
 * Also set the Frame's parity bit accordingly (see \ref ltc_frame_set_parity)
 * @param frame the LTCFrame to reset
 */
void ltc_frame_reset(LTCFrame* frame);

/**
 * Increment the timecode by one Frame (1/framerate seconds)
 * and set the Frame's parity bit accordingly (see \ref ltc_frame_set_parity)
 *
 * @param frame the LTC-timecode to increment
 * @param fps integer framerate (for drop-frame-timecode set frame->dfbit and round-up the fps).
 * @param standard the TV standard to use for parity bit assignment
 * if set to 1 the 25fps standard is enabled and LTC Frame bit 59 instead of 27 is used for the parity. It only has only has effect flag bit 4 (LTC_NO_PARITY) is cleared.
 * @param flags binary combination of \ref LTC_BG_FLAGS - here only LTC_USE_DATE and LTC_NO_PARITY are relevant.
 * If the bit 0 (1) is set (1) interpret user-data as date and increment date if timecode wraps after 24h.
 * (Note: leap-years are taken into account, but since the year is two-digit only, the 100,400yr rules are ignored.
 * "00" is assumed to be year 2000 which was a leap year.)
 * @return 1 if timecode was wrapped around after 23:59:59:ff, 0 otherwise
 */
int ltc_frame_increment(LTCFrame* frame, int fps, enum LTC_TV_STANDARD standard, int flags);

/**
 * Decrement the timecode by one Frame (1/framerate seconds)
 * and set the Frame's parity bit accordingly (see \ref ltc_frame_set_parity)
 *
 * @param frame the LTC-timecode to decrement
 * @param fps integer framerate (for drop-frame-timecode set frame->dfbit and round-up the fps).
 * @param standard the TV standard to use for parity bit assignment
 * if set to 1 the 25fps standard is enabled and LTC Frame bit 59 instead of 27 is used for the parity. It only has only has effect flag bit 4 (LTC_NO_PARITY) is cleared.
 * @param flags binary combination of \ref LTC_BG_FLAGS - here only LTC_USE_DATE and LTC_NO_PARITY are relevant.
 * if the bit 0 is set (1) interpret user-data as date and decrement date if timecode wraps at 24h.
 * (Note: leap-years are taken into account, but since the year is two-digit only, the 100,400yr rules are ignored.
 * "00" is assumed to be year 2000 which was a leap year.)
 * bit 3 (8) indicates that the parity bit should not be touched
 * @return 1 if timecode was wrapped around at 23:59:59:ff, 0 otherwise
 */
int ltc_frame_decrement(LTCFrame* frame, int fps, enum LTC_TV_STANDARD standard, int flags);

/**
 * Create a new LTC decoder.
 *
 * @param apv audio-frames per video frame. This is just used for initial settings, the speed is tracked dynamically. setting this in the right ballpark is needed to properly decode the first LTC frame in a sequence.
 * @param queue_size length of the internal queue to store decoded frames
 * to SMPTEDecoderWrite.
 * @return decoder handle or NULL if out-of-memory
 */
LTCDecoder * ltc_decoder_create(int apv, int queue_size);


/**
 * Release memory of decoder.
 * @param d decoder handle
 */
int ltc_decoder_free(LTCDecoder *d);

/**
 * Feed the LTC decoder with new audio samples.
 *
 * Parse raw audio for LTC timestamps. Once a complete LTC frame has been
 * decoded it is pushed into a queue (\ref ltc_decoder_read)
 *
 * @param d decoder handle
 * @param buf pointer to ltcsnd_sample_t - unsigned 8 bit mono audio data
 * @param size \anchor size number of samples to parse
 * @param posinfo (optional, recommended) sample-offset in the audio-stream. It is added to \ref off_start, \ref off_end in \ref LTCFrameExt and should be monotonic (ie incremented by \ref size for every call to ltc_decoder_write)
 */
void ltc_decoder_write(LTCDecoder *d,
		ltcsnd_sample_t *buf, size_t size,
		ltc_off_t posinfo);

/**
 * Wrapper around \ref ltc_decoder_write that accepts floating point
 * audio samples. Note: internally libltc uses 8 bit only.
 *
 * @param d decoder handle
 * @param buf pointer to audio sample data
 * @param size number of samples to parse
 * @param posinfo (optional, recommended) sample-offset in the audio-stream.
 */
void ltc_decoder_write_float(LTCDecoder *d, float *buf, size_t size, ltc_off_t posinfo);

/**
 * Wrapper around \ref ltc_decoder_write that accepts signed 16 bit
 * audio samples. Note: internally libltc uses 8 bit only.
 *
 * @param d decoder handle
 * @param buf pointer to audio sample data
 * @param size number of samples to parse
 * @param posinfo (optional, recommended) sample-offset in the audio-stream.
 */
void ltc_decoder_write_s16(LTCDecoder *d, short *buf, size_t size, ltc_off_t posinfo);

/**
 * Wrapper around \ref ltc_decoder_write that accepts unsigned 16 bit
 * audio samples. Note: internally libltc uses 8 bit only.
 *
 * @param d decoder handle
 * @param buf pointer to audio sample data
 * @param size number of samples to parse
 * @param posinfo (optional, recommended) sample-offset in the audio-stream.
 */
void ltc_decoder_write_u16(LTCDecoder *d, short *buf, size_t size, ltc_off_t posinfo);

/**
 * Decoded LTC frames are placed in a queue. This function retrieves
 * a frame from the queue, and stores it at LTCFrameExt*
 *
 * @param d decoder handle
 * @param frame the decoded LTC frame is copied there
 * @return 1 on success or 0 when no frames queued.
 */
int ltc_decoder_read(LTCDecoder *d, LTCFrameExt *frame);

/**
 * Remove all LTC frames from the internal queue.
 * @param d decoder handle
 */
void ltc_decoder_queue_flush(LTCDecoder* d);

/**
 * Count number of LTC frames currently in the queue.
 * @param d decoder handle
 * @return number of queued frames
 */
int ltc_decoder_queue_length(LTCDecoder* d);



/**
 * Allocate and initialize LTC audio encoder.
 *
 * calls \ref ltc_encoder_reinit internally see, see notes there.
 *
 * @param sample_rate audio sample rate (eg. 48000)
 * @param fps video-frames per second (e.g. 25.0)
 * @param standard the TV standard to use for Binary Group Flag bit position
 * @param flags binary combination of \ref LTC_BG_FLAGS
 */
LTCEncoder* ltc_encoder_create(double sample_rate, double fps, enum LTC_TV_STANDARD standard, int flags);

/**
 * Release memory of the encoder.
 * @param e encoder handle
 */
void ltc_encoder_free(LTCEncoder *e);

/**
 * Set the encoder LTC-frame to the given SMPTETimecode.
 * The next call to \ref ltc_encoder_encode_byte or
 * \ref ltc_encoder_encode_frame will encode this time to LTC audio-samples.
 *
 * Internally this call uses \ref ltc_time_to_frame because
 * the LTCEncoder operates on LTCframes only.
 * see als \ref ltc_encoder_set_frame
 *
 * @param e encoder handle
 * @param t timecode to set.
 */
void ltc_encoder_set_timecode(LTCEncoder *e, SMPTETimecode *t);

/**
 * Query the current encoder timecode.
 *
 * Note: the decoder stores its internal state in an LTC-frame,
 * this function converts that LTC-Frame into SMPTETimecode on demand.
 * see also \ref ltc_encoder_get_frame.
 *
 * @param e encoder handle
 * @param t is set to current timecode
 */
void ltc_encoder_get_timecode(LTCEncoder *e, SMPTETimecode *t);

/**
 * Move the encoder to the next timecode frame.
 * uses \ref ltc_frame_increment() internally.
 */
int ltc_encoder_inc_timecode(LTCEncoder *e);

/**
 * Move the encoder to the previous timecode frame.
 * This is useful for encoding reverse LTC.
 * uses \ref ltc_frame_decrement() internally.
 */
int ltc_encoder_dec_timecode(LTCEncoder *e);

/**
 * Low-level access to the internal LTCFrame data.
 *
 * Note: be careful to about f->dfbit, the encoder sets this [only] upon
 * initialization.
 *
 * @param e encoder handle
 * @param f LTC frame data to use
 */
void ltc_encoder_set_frame(LTCEncoder *e, LTCFrame *f);

/**
 * Low-level access to the encoder internal LTCFrame data
 *
 * @param e encoder handle
 * @param f return LTC frame data
 */
void ltc_encoder_get_frame(LTCEncoder *e, LTCFrame *f);

/**
 * Copy the accumulated encoded audio to the given
 * sample-buffer and flush the internal buffer.
 *
 * @param e encoder handle
 * @param buf place to store the audio-samples, needs to be large enough
 * to hold \ref ltc_encoder_get_buffersize bytes
 * @return the number of bytes written to the memory area
 * pointed to by buf.
 */
int ltc_encoder_get_buffer(LTCEncoder *e, ltcsnd_sample_t *buf);


/**
 * Retrieve a pointer to the accumulated encoded audio-data.
 *
 * @param e encoder handle
 * @param size if set, the number of valid bytes in the buffer is stored there
 * @param flush call \ref ltc_encoder_buffer_flush - reset the buffer write-pointer
 * @return pointer to encoder-buffer
 */
ltcsnd_sample_t *ltc_encoder_get_bufptr(LTCEncoder *e, int *size, int flush);

/**
 * reset the write-pointer of the encoder-buffer
 * @param e encoder handle
 */
void ltc_encoder_buffer_flush(LTCEncoder *e);

/**
 * Query the length of the internal buffer. It is allocated
 * to hold audio-frames for exactly one LTC frame for the given
 * sample-rate and frame-rate.  ie. (1 + sample-rate / fps) bytes
 *
 * Note this returns the total size of the buffer, not the used/free
 * part. See also \ref ltc_encoder_get_bufptr
 *
 * @param e encoder handle
 * @return size of the allocated internal buffer.
 */
size_t ltc_encoder_get_buffersize(LTCEncoder *e);

/**
 * Change the encoder settings without re-allocating any
 * library internal data structure (realtime safe).
 * changing the fps and or sample-rate implies a buffer flush,
 * and biphase state reset.
 *
 * This call will fail if the internal buffer is too small
 * to hold one full LTC frame. Use \ref ltc_encoder_set_bufsize to
 * prepare an internal buffer large enough to accommodate all
 * sample_rate, fps combinations that you would like to re-init to.
 *
 * The LTC frame payload data is not modified by this call, however,
 * the flag-bits of the LTC-Frame are updated:
 * If fps equals to 29.97 or 30000.0/1001.0, the LTCFrame's 'dfbit' bit is set to 1
 * to indicate drop-frame timecode.
 *
 * Unless the LTC_BGF_DONT_TOUCH flag is set the BGF1 is set or cleared depending
 * on LTC_TC_CLOCK and BGF0,2 according to LTC_USE_DATE and the given standard.
 * col_frame is cleared  and the parity recomputed (unless LTC_NO_PARITY is given).
 *
 * @param e encoder handle
 * @param sample_rate audio sample rate (eg. 48000)
 * @param fps video-frames per second (e.g. 25.0)
 * @param standard the TV standard to use for Binary Group Flag bit position
 * @param flags binary combination of \ref LTC_BG_FLAGS
 */
int ltc_encoder_reinit(LTCEncoder *e, double sample_rate, double fps, enum LTC_TV_STANDARD standard, int flags);

/**
 * reset ecoder state.
 * flushes buffer, reset biphase state
 *
 * @param e encoder handle
 */
void ltc_encoder_reset(LTCEncoder *e);

/**
 * Configure a custom size for the internal buffer.
 *
 * This is needed if you are planning to call \ref ltc_encoder_reinit()
 * or if you want to keep more than one LTC frame's worth of data in
 * the library's internal buffer.
 *
 * The buffer-size is (1 + sample_rate / fps) bytes.
 * resizing the internal buffer will flush all existing data
 * in it - alike \ref ltc_encoder_buffer_flush.
 *
 * @param e encoder handle
 * @param sample_rate audio sample rate (eg. 48000)
 * @param fps video-frames per second (e.g. 25.0)
 * @return 0 on success, -1 if allocation fails (which makes the
 *   encoder unusable, call \ref ltc_encoder_free or realloc the buffer)
 */
int ltc_encoder_set_bufsize(LTCEncoder *e, double sample_rate, double fps);

/**
 * Set the volume of the generated LTC signal
 *
 * typically LTC is sent at 0dBu ; in EBU callibrated systems that
 * corresponds to -18dBFS. - by default libltc creates -3dBFS
 *
 * since libltc generated 8bit audio-data, the minium dBFS
 * is about -42dB which corresponds to 1 bit.
 *
 * 0dB corresponds to a signal range of 127
 * 1..255 with 128 at the center.
 *
 * @param e encoder handle
 * @param dBFS the volume in dB full-scale (<= 0.0)
 * @return 0 on success, -1 if the value was out of range
 */
int ltc_encoder_set_volume(LTCEncoder *e, double dBFS);

/**
 * Set encoder signal rise-time / signal filtering
 *
 * LTC signal should have a rise time of 40us +/- 10 us.
 * by default the encoder honors this and low-pass filters
 * the output depending on the sample-rate.
 *
 * If you want a perfect square wave, set 'rise_time' to 0.
 *
 * Note \ref ltc_encoder_reinit resets the filter-time-constant to use
 * the default 40us for the given sample-rate, overriding any value
 * previously set with \ref ltc_encoder_set_filter
 *
 * @param e encoder handle
 * @param rise_time the signal rise-time in us (10^(-6) sec), set to 0 for perfect square wave, default 40.0
 */
void ltc_encoder_set_filter(LTCEncoder *e, double rise_time);

/**
 * Generate LTC audio for given byte of the LTC-frame and
 * place it into the internal buffer.
 *
 * see \ref ltc_encoder_get_buffer and  \ref ltc_encoder_get_bufptr
 *
 * LTC has 10 bytes per frame: 0 <= bytecnt < 10
 * use SMPTESetTime(..) to set the current frame before Encoding.
 * see tests/encoder.c for an example.
 *
 * The default output signal is @ -3dBFS (38..218 at 8 bit unsigned).
 * see also \ref ltc_encoder_set_volume
 *
 * if speed is < 0, the bits are encoded in reverse.
 * slowdown > 10.0 requires custom buffer sizes; see \ref ltc_encoder_set_bufsize
 *
 * @param e encoder handle
 * @param byte byte of the LTC-frame to encode 0..9
 * @param speed vari-speed, < 1.0 faster,  > 1.0 slower ; must be != 0
 *
 * @return 0 on success, -1 if byte is invalid or buffer overflow (speed > 10.0)
 */
int ltc_encoder_encode_byte(LTCEncoder *e, int byte, double speed);

/**
 * Encode a full LTC frame at fixed speed.
 * This is equivalent to calling \ref ltc_encoder_encode_byte 10 times for
 * bytes 0..9 with speed 1.0.
 *
 * Note: The internal buffer must be empty before calling this function.
 * Otherwise it may overflow. This is usually the case if it is read with
 * \ref ltc_encoder_get_buffer after calling this function.
 *
 * The default internal buffersize is exactly one full LTC frame at speed 1.0.
 *
 * @param e encoder handle
 */
void ltc_encoder_encode_frame(LTCEncoder *e);

/**
 * Set the parity of the LTC frame.
 *
 * Bi-Phase Mark Phase Correction bit (bit 27 - or 59) may be set or cleared so that
 * that every 80-bit word contains an even number of zeroes.
 * This means that the phase in every Sync Word will be the same.
 *
 * This is merely cosmetic; the motivation to keep the polarity of the waveform
 * constant is to make finding the Sync Word visibly (on a scope) easier.
 *
 * There is usually no need to call this function directly. The encoder utility
 * functions \ref ltc_time_to_frame, \ref ltc_frame_increment and
 * \ref ltc_frame_decrement include a call to it.
 *
 * @param frame the LTC to analyze and set or clear the biphase_mark_phase_correction bit.
 * @param standard If 1 (aka LTC_TV_625_50) , the 25fps mode (bit 59 - aka binary_group_flag_bit2) is used, otherwise the 30fps, 24fps mode (bit 27 -- biphase_mark_phase_correction) is set or cleared.
 */
void ltc_frame_set_parity(LTCFrame *frame, enum LTC_TV_STANDARD standard);

/**
 * Parse Binary Group Flags into standard independent format:
 * bit 0 (1) - BGF 0,
 * bit 1 (2) - BGF 1,
 * bit 2 (4) - BGF 2
 *
 * @param f LTC frame data analyze
 * @param standard the TV standard to use -- see \ref LTCFrame for BGF assignment
 * @return LTC Binary Group Flags
 */
int parse_bcg_flags(LTCFrame *f, enum LTC_TV_STANDARD standard);

/**
 * LTCFrame sample alignment offset.
 *
 * There is a relative offset of the LTC-Frame start and the TV-frame.
 * The first bit of a LTC frame corresponds to a specific line in the actual video
 * frame. When decoding this offset needs to be subtracted from the LTC-frame's
 * audio-sample-time to match the TV-frame's start position.
 *
 * For film frames or HDV the offset is zero.
 *
 * @param samples_per_frame audio-samples per timecode-frame (eg. 1920 = 48000/25)
 * @param standard the TV standard
 * @return offset in samples
 */
ltc_off_t ltc_frame_alignment(double samples_per_frame, enum LTC_TV_STANDARD standard);

#ifdef __cplusplus
}
#endif

#endif
