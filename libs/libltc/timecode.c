/*
   libltc - en+decode linear timecode

   Copyright (C) 2006-2012 Robin Gareus <robin@gareus.org>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ltc/ltc.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/**
 * SMPTE Timezones
 */
struct SMPTETimeZonesStruct {
	unsigned char code; //actually 6 bit!
	char timezone[6];
};

/**
 * SMPTE Timezone codes as per http://www.barney-wol.net/time/timecode.html
 */
static const struct SMPTETimeZonesStruct smpte_time_zones[] =
{
    /*  code,   timezone (UTC+)     //Standard time                 //Daylight saving   */
    {   0x00,   "+0000"             /* Greenwich */                 /* - */             },
    {   0x00,   "-0000"             /* Greenwich */                 /* - */             },
    {   0x01,   "-0100"             /* Azores */                    /* - */             },
    {   0x02,   "-0200"             /* Mid-Atlantic */              /* - */             },
    {   0x03,   "-0300"             /* Buenos Aires */              /* Halifax */       },
    {   0x04,   "-0400"             /* Halifax */                   /* New York */      },
    {   0x05,   "-0500"             /* New York */                  /* Chicago */       },
    {   0x06,   "-0600"             /* Chicago Denver */            /* - */             },
    {   0x07,   "-0700"             /* Denver */                    /* Los Angeles */   },
    {   0x08,   "-0800"             /* Los Angeles */               /* - */             },
    {   0x09,   "-0900"             /* Alaska */                    /* - */             },
    {   0x10,   "-1000"             /* Hawaii */                    /* - */             },
    {   0x11,   "-1100"             /* Midway Island */             /* - */             },
    {   0x12,   "-1200"             /* Kwaialein */                 /* - */             },
    {   0x13,   "+1300"             /* - */                         /* New Zealand */   },
    {   0x14,   "+1200"             /* New Zealand */               /* - */             },
    {   0x15,   "+1100"             /* Solomon Islands */           /* - */             },
    {   0x16,   "+1000"             /* Guam */                      /* - */             },
    {   0x17,   "+0900"             /* Tokyo */                     /* - */             },
    {   0x18,   "+0800"             /* Beijing */                   /* - */             },
    {   0x19,   "+0700"             /* Bangkok */                   /* - */             },
    {   0x20,   "+0600"             /* Dhaka */                     /* - */             },
    {   0x21,   "+0500"             /* Islamabad */                 /* - */             },
    {   0x22,   "+0400"             /* Abu Dhabi */                 /* - */             },
    {   0x23,   "+0300"             /* Moscow */                    /* - */             },
    {   0x24,   "+0200"             /* Eastern Europe */            /* - */             },
    {   0x25,   "+0100"             /* Central Europe */            /* - */             },
/*  {   0x26,   "Undefined"         Reserved; do not use                                },*/
/*  {   0x27,   "Undefined"         Reserved; do not use                                },*/
    {   0x28,   "TP-03"             /* Time precision class 3 */    /* - */             },
    {   0x29,   "TP-02"             /* Time precision class 2 */    /* - */             },
    {   0x30,   "TP-01"             /* Time precision class 1 */    /* - */             },
    {   0x31,   "TP-00"             /* Time precision class 0 */    /* - */             },
    {   0x0A,   "-0030"             /* - */                         /* - */             },
    {   0x0B,   "-0130"             /* - */                         /* - */             },
    {   0x0C,   "-0230"             /* - */                         /* Newfoundland */  },
    {   0x0D,   "-0330"             /* Newfoundland */              /* - */             },
    {   0x0E,   "-0430"             /* - */                         /* - */             },
    {   0x0F,   "-0530"             /* - */                         /* - */             },
    {   0x1A,   "-0630"             /* - */                         /* - */             },
    {   0x1B,   "-0730"             /* - */                         /* - */             },
    {   0x1C,   "-0830"             /* - */                         /* - */             },
    {   0x1D,   "-0930"             /* Marquesa Islands */          /* - */             },
    {   0x1E,   "-1030"             /* - */                         /* - */             },
    {   0x1F,   "-1130"             /* - */                         /* - */             },
    {   0x2A,   "+1130"             /* Norfolk Island */            /* - */             },
    {   0x2B,   "+1030"             /* Lord Howe Is. */             /* - */             },
    {   0x2C,   "+0930"             /* Darwin */                    /* - */             },
    {   0x2D,   "+0830"             /* - */                         /* - */             },
    {   0x2E,   "+0730"             /* - */                         /* - */             },
    {   0x2F,   "+0630"             /* Rangoon */                   /* - */             },
    {   0x3A,   "+0530"             /* Bombay */                    /* - */             },
    {   0x3B,   "+0430"             /* Kabul */                     /* - */             },
    {   0x3C,   "+0330"             /* Tehran */                    /* - */             },
    {   0x3D,   "+0230"             /* - */                         /* - */             },
    {   0x3E,   "+0130"             /* - */                         /* - */             },
    {   0x3F,   "+0030"             /* - */                         /* - */             },
    {   0x32,   "+1245"             /* Chatham Island */            /* - */             },
/*  {   0x33,   "Undefined"         Reserved; do not use                                },*/
/*  {   0x34,   "Undefined"         Reserved; do not use                                },*/
/*  {   0x35,   "Undefined"         Reserved; do not use                                },*/
/*  {   0x36,   "Undefined"         Reserved; do not use                                },*/
/*  {   0x37,   "Undefined"         Reserved; do not use                                },*/
    {   0x38,   "+XXXX"             /* User defined time offset */  /* - */             },
/*  {   0x39,   "Undefined"         Unknown                         Unknown             },*/
/*  {   0x39,   "Undefined"         Unknown                         Unknown             },*/

    {   0xFF,   ""                  /* The End */                                       }
};

static void smpte_set_timezone_string(LTCFrame *frame, SMPTETimecode *stime) {
	int i = 0;

	const unsigned char code = frame->user7 + (frame->user8 << 4);

	char timezone[6] = "+0000";

	for (i = 0 ; smpte_time_zones[i].code != 0xFF ; i++) {
		if ( smpte_time_zones[i].code == code ) {
			strcpy(timezone, smpte_time_zones[i].timezone);
			break;
		}
	}
	strcpy(stime->timezone, timezone);
}

static void smpte_set_timezone_code(SMPTETimecode *stime, LTCFrame *frame) {
	int i = 0;
	unsigned char code = 0x00;

	// Find code for timezone string
	// Primitive search
	for (i=0; smpte_time_zones[i].code != 0xFF; i++) {
		if ( (strcmp(smpte_time_zones[i].timezone, stime->timezone)) == 0 ) {
			code = smpte_time_zones[i].code;
			break;
		}
	}

	frame->user7 = code & 0x0F;
	frame->user8 = (code & 0xF0) >> 4;
}

/** Drop-frame support function
 * We skip the first two frame numbers (0 and 1) at the beginning of each minute,
 * except for minutes 0, 10, 20, 30, 40, and 50
 * (i.e. we skip frame numbers at the beginning of minutes for which mins_units is not 0).
 */
static void skip_drop_frames(LTCFrame* frame) {
	if ((frame->mins_units != 0)
		&& (frame->secs_units == 0)
		&& (frame->secs_tens == 0)
		&& (frame->frame_units == 0)
		&& (frame->frame_tens == 0)
		) {
		frame->frame_units += 2;
	}
}

void ltc_frame_to_time(SMPTETimecode *stime, LTCFrame *frame, int flags) {
	if (!stime) return;

	if (flags & LTC_USE_DATE) {
		smpte_set_timezone_string(frame, stime);

		stime->years  = frame->user5 + frame->user6*10;
		stime->months = frame->user3 + frame->user4*10;
		stime->days   = frame->user1 + frame->user2*10;
	} else {
		stime->years  = 0;
		stime->months = 0;
		stime->days   = 0;
		sprintf(stime->timezone,"+0000");
	}

	stime->hours = frame->hours_units + frame->hours_tens*10;
	stime->mins  = frame->mins_units  + frame->mins_tens*10;
	stime->secs  = frame->secs_units  + frame->secs_tens*10;
	stime->frame = frame->frame_units + frame->frame_tens*10;
}

void ltc_time_to_frame(LTCFrame* frame, SMPTETimecode* stime, enum LTC_TV_STANDARD standard, int flags) {
	if (flags & LTC_USE_DATE) {
		smpte_set_timezone_code(stime, frame);
		frame->user6 = stime->years/10;
		frame->user5 = stime->years - frame->user6*10;
		frame->user4 = stime->months/10;
		frame->user3 = stime->months - frame->user4*10;
		frame->user2 = stime->days/10;
		frame->user1 = stime->days - frame->user2*10;
	}

	frame->hours_tens  = stime->hours/10;
	frame->hours_units = stime->hours - frame->hours_tens*10;
	frame->mins_tens   = stime->mins/10;
	frame->mins_units  = stime->mins - frame->mins_tens*10;
	frame->secs_tens   = stime->secs/10;
	frame->secs_units  = stime->secs - frame->secs_tens*10;
	frame->frame_tens  = stime->frame/10;
	frame->frame_units = stime->frame - frame->frame_tens*10;

	// Prevent illegal SMPTE frames
	if (frame->dfbit) {
		skip_drop_frames(frame);
	}

	if ((flags & LTC_NO_PARITY) == 0) {
		ltc_frame_set_parity(frame, standard);
	}
}

void ltc_frame_reset(LTCFrame* frame) {
	memset(frame, 0, sizeof(LTCFrame));
	// syncword = 0x3FFD
#ifdef __BIG_ENDIAN__
	// mirrored BE bit order: FCBF
	frame->sync_word = 0xFCBF;
#else
	// mirrored LE bit order: BFFC
	frame->sync_word = 0xBFFC;
#endif
}

int ltc_frame_increment(LTCFrame* frame, int fps, enum LTC_TV_STANDARD standard, int flags) {
	int rv = 0;

	frame->frame_units++;

	if (frame->frame_units == 10)
	{
		frame->frame_units = 0;
		frame->frame_tens++;
	}
	if (fps == frame->frame_units+frame->frame_tens*10)
	{
		frame->frame_units = 0;
		frame->frame_tens = 0;
		frame->secs_units++;
		if (frame->secs_units == 10)
		{
			frame->secs_units = 0;
			frame->secs_tens++;
			if (frame->secs_tens == 6)
			{
				frame->secs_tens = 0;
				frame->mins_units++;
				if (frame->mins_units == 10)
				{
					frame->mins_units = 0;
					frame->mins_tens++;
					if (frame->mins_tens == 6)
					{
						frame->mins_tens = 0;
						frame->hours_units++;
						if (frame->hours_units == 10)
						{
							frame->hours_units = 0;
							frame->hours_tens++;
						}
						if (frame->hours_units == 4 && frame->hours_tens==2)
						{
							/* 24h wrap around */
							rv=1;
							frame->hours_tens=0;
							frame->hours_units = 0;

							if (flags&1)
							{
								/* wrap date */
								SMPTETimecode stime;
								stime.years  = frame->user5 + frame->user6*10;
								stime.months = frame->user3 + frame->user4*10;
								stime.days   = frame->user1 + frame->user2*10;

								if (stime.months > 0 && stime.months < 13)
								{
									unsigned char dpm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
									/* proper leap-year calc:
									 * ((stime.years%4)==0 && ( (stime.years%100) != 0 || (stime.years%400) == 0) )
									 * simplified since year is 0..99
									 */
									if ((stime.years%4)==0 /* && stime.years!=0 */ ) /* year 2000 was a leap-year */
										dpm[1]=29;
									stime.days++;
									if (stime.days > dpm[stime.months-1])
									{
										stime.days=1;
										stime.months++;
										if (stime.months > 12) {
											stime.months=1;
											stime.years=(stime.years+1)%100;
										}
									}
									frame->user6 = stime.years/10;
									frame->user5 = stime.years%10;
									frame->user4 = stime.months/10;
									frame->user3 = stime.months%10;
									frame->user2 = stime.days/10;
									frame->user1 = stime.days%10;
								} else {
									rv=-1;
								}
							}
						}
					}
				}
			}
		}
	}

	if (frame->dfbit) {
		skip_drop_frames(frame);
	}

	if ((flags & LTC_NO_PARITY) == 0) {
		ltc_frame_set_parity(frame, standard);
	}

	return rv;
}

int ltc_frame_decrement(LTCFrame* frame, int fps, enum LTC_TV_STANDARD standard, int flags) {
	int rv = 0;

	int frames = frame->frame_units + frame->frame_tens * 10;
	if (frames > 0) {
		frames--;
	} else {
		frames = fps -1;
	}

	frame->frame_units = frames % 10;
	frame->frame_tens  = frames / 10;

	if (frames == fps -1) {
		int secs = frame->secs_units + frame->secs_tens * 10;
		if (secs > 0) {
			secs--;
		} else {
			secs = 59;
		}
		frame->secs_units = secs % 10;
		frame->secs_tens  = secs / 10;

		if (secs == 59) {
			int mins = frame->mins_units + frame->mins_tens * 10;
			if (mins > 0) {
				mins--;
			} else {
				mins = 59;
			}
			frame->mins_units = mins % 10;
			frame->mins_tens  = mins / 10;

			if (mins == 59) {
				int hours = frame->hours_units + frame->hours_tens * 10;
				if (hours > 0) {
					hours--;
				} else {
					hours = 23;
				}
				frame->hours_units = hours % 10;
				frame->hours_tens  = hours / 10;

				if (hours == 23) {
					/* 24h wrap around */
					rv=1;
					if (flags&LTC_USE_DATE)
					{
						/* wrap date */
						SMPTETimecode stime;
						stime.years  = frame->user5 + frame->user6*10;
						stime.months = frame->user3 + frame->user4*10;
						stime.days   = frame->user1 + frame->user2*10;

						if (stime.months > 0 && stime.months < 13)
						{
							unsigned char dpm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
							/* proper leap-year calc:
							 * ((stime.years%4)==0 && ( (stime.years%100) != 0 || (stime.years%400) == 0) )
							 * simplified since year is 0..99
							 */
							if ((stime.years%4)==0 /* && stime.years!=0 */ ) /* year 2000 was a leap-year */
								dpm[1]=29;
							//
							if (stime.days > 1) {
								stime.days--;
							} else {
								stime.months = 1 + (stime.months + 10)%12;
								stime.days = dpm[stime.months-1];
								if (stime.months == 12)  {
									stime.years=(stime.years+99)%100; // XXX
								}
							}

							frame->user6 = stime.years/10;
							frame->user5 = stime.years%10;
							frame->user4 = stime.months/10;
							frame->user3 = stime.months%10;
							frame->user2 = stime.days/10;
							frame->user1 = stime.days%10;
						} else {
							rv=-1;
						}
					}
				}
			}
		}
	}

	if (frame->dfbit && /* prevent endless recursion */ fps > 2) {
		if ((frame->mins_units != 0)
			&& (frame->secs_units == 0)
			&& (frame->secs_tens == 0)
			&& (frame->frame_units == 1)
			&& (frame->frame_tens == 0)
			) {
			ltc_frame_decrement(frame, fps, standard, flags&LTC_USE_DATE);
			ltc_frame_decrement(frame, fps, standard, flags&LTC_USE_DATE);
		}
	}

	if ((flags & LTC_NO_PARITY) == 0) {
		ltc_frame_set_parity(frame, standard);
	}

	return rv;
}

int parse_bcg_flags(LTCFrame *f, enum LTC_TV_STANDARD standard) {
	switch (standard) {
		case LTC_TV_625_50: /* 25 fps mode */
			return (
					  ((f->binary_group_flag_bit0)?4:0)
					| ((f->binary_group_flag_bit1)?2:0)
					| ((f->biphase_mark_phase_correction)?1:0)
					);
			break;
		default: /* 24,30 fps mode */
			return (
					  ((f->binary_group_flag_bit2)?4:0)
					| ((f->binary_group_flag_bit1)?2:0)
					| ((f->binary_group_flag_bit0)?1:0)
					);
			break;
	}
}
