/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * ALTHOUGH THIS SOFTWARE IS MADE OF WIN AND SCIENCE, IT IS PROVIDED BY THE
 * AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SMF_PRIVATE_H
#define SMF_PRIVATE_H

#include <stdint.h>
#include <sys/types.h>

//#include "config.h"
//#define SMF_VERSION PACKAGE_VERSION

/**
 * \file
 *
 * Private header.  Applications using libsmf should use smf.h.
 *
 */

#if defined(__GNUC__)
#define ATTRIBUTE_PACKED  __attribute__((__packed__))
#else
#define ATTRIBUTE_PACKED
#pragma pack(1)
#endif

/** SMF chunk header, used only by smf_load.c and smf_save.c. */
struct chunk_header_struct {
	char		id[4];
	uint32_t	length; 
} ATTRIBUTE_PACKED;

/** SMF chunk, used only by smf_load.c and smf_save.c. */
struct mthd_chunk_struct {
	struct chunk_header_struct	mthd_header;
	uint16_t			format;
	uint16_t			number_of_tracks;
	uint16_t			division;
} ATTRIBUTE_PACKED;

#if (!defined __GNUC__)
#pragma pack()
#endif

void smf_track_add_event(smf_track_t *track, smf_event_t *event);
void smf_init_tempo(smf_t *smf);
void smf_fini_tempo(smf_t *smf);
void smf_create_tempo_map_and_compute_seconds(smf_t *smf);
void maybe_add_to_tempo_map(smf_event_t *event);
void remove_last_tempo_with_pulses(smf_t *smf, size_t pulses);
int smf_event_is_tempo_change_or_time_signature(const smf_event_t *event) WARN_UNUSED_RESULT;
int smf_event_length_is_valid(const smf_event_t *event) WARN_UNUSED_RESULT;
int is_status_byte(const unsigned char status) WARN_UNUSED_RESULT;
smf_track_t* smf_find_track_with_next_event (smf_t *smf);

#endif /* SMF_PRIVATE_H */

