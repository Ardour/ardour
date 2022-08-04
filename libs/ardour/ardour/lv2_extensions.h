/*
  Copyright 2016 Robin Gareus <robin@gareus.org>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef _ardour_lv2_extensions_h_
#define _ardour_lv2_extensions_h_

#include "lv2/core/lv2.h"

/**
   @defgroup lv2inlinedisplay Inline-Display

   Support for displaying a miniaturized generic view
	 directly in the host's Mixer Window.

   @{
*/

#define LV2_INLINEDISPLAY_URI "http://harrisonconsoles.com/lv2/inlinedisplay"
#define LV2_INLINEDISPLAY_PREFIX LV2_INLINEDISPLAY_URI "#"
#define LV2_INLINEDISPLAY__interface LV2_INLINEDISPLAY_PREFIX "interface"
#define LV2_INLINEDISPLAY__queue_draw LV2_INLINEDISPLAY_PREFIX "queue_draw"
#define LV2_INLINEDISPLAY__in_gui LV2_INLINEDISPLAY_PREFIX "in_gui"

/** Opaque handle for LV2_Inline_Display::queue_draw() */
typedef void* LV2_Inline_Display_Handle;

/** raw image pixmap format is ARGB32,
 * the data pointer is owned by the plugin and must be valid
 * from the first call to render until cleanup.
 */
typedef struct {
	unsigned char *data;
	int width;
	int height;
	int stride;
} LV2_Inline_Display_Image_Surface;

/** a LV2 Feature provided by the Host to the plugin */
typedef struct {
	/** Opaque host data */
	LV2_Inline_Display_Handle handle;
	/** Request from run() that the host should call render() at a later time
	 * to update the inline display */
	void (*queue_draw)(LV2_Inline_Display_Handle handle);
} LV2_Inline_Display;

/**
 * Plugin Inline-Display Interface.
 */
typedef struct {
	/**
	 * The render method. This is called by the host in a non-realtime context,
	 * usually the main GUI thread.
	 * The data pointer is owned by the plugin and must be valid
	 * from the first call to render until cleanup.
	 *
	 * @param instance The LV2 instance
	 * @param w the max available width
	 * @param h the max available height
	 * @return pointer to a LV2_Inline_Display_Image_Surface or NULL
	 */
	LV2_Inline_Display_Image_Surface* (*render)(LV2_Handle instance, uint32_t w, uint32_t h);
} LV2_Inline_Display_Interface;

/**
   @}
*/

/**
   @defgroup lv2automate Self-Automation

   Support for plugins to write automation data via Atom Events

   @{
*/

#define LV2_AUTOMATE_URI "http://ardour.org/lv2/automate"
#define LV2_AUTOMATE_URI_PREFIX LV2_AUTOMATE_URI "#"
/** an lv2:optionalFeature */
#define LV2_AUTOMATE_URI__can_write LV2_AUTOMATE_URI_PREFIX "canWriteAutomatation"
/** atom:supports */
#define LV2_AUTOMATE_URI__control LV2_AUTOMATE_URI_PREFIX "automationControl"
/** lv2:portProperty */
#define LV2_AUTOMATE_URI__controlled LV2_AUTOMATE_URI_PREFIX "automationControlled"
#define LV2_AUTOMATE_URI__controller LV2_AUTOMATE_URI_PREFIX "automationController"

/** atom messages */
#define LV2_AUTOMATE_URI__event LV2_AUTOMATE_URI_PREFIX "event"
#define LV2_AUTOMATE_URI__setup LV2_AUTOMATE_URI_PREFIX "setup"
#define LV2_AUTOMATE_URI__finalize LV2_AUTOMATE_URI_PREFIX "finalize"
#define LV2_AUTOMATE_URI__start LV2_AUTOMATE_URI_PREFIX "start"
#define LV2_AUTOMATE_URI__end LV2_AUTOMATE_URI_PREFIX "end"
#define LV2_AUTOMATE_URI__parameter LV2_AUTOMATE_URI_PREFIX "parameter"
#define LV2_AUTOMATE_URI__value LV2_AUTOMATE_URI_PREFIX "value"

/**
   @}
*/

/**
   @defgroup lv2license License-Report

   Allow for commercial LV2 to report their
	 licensing status.

   @{
*/

#define LV2_PLUGINLICENSE_URI "http://harrisonconsoles.com/lv2/license"
#define LV2_PLUGINLICENSE_PREFIX LV2_PLUGINLICENSE_URI "#"
#define LV2_PLUGINLICENSE__interface LV2_PLUGINLICENSE_PREFIX "interface"
#define LV2_PLUGINLICENSE__interface2 LV2_PLUGINLICENSE_PREFIX "interface2"


typedef struct _LV2_License_Interface {
	/* @return -1 if no license is needed; 0 if unlicensed, 1 if licensed */
	int   (*is_licensed)(LV2_Handle instance);
	/* @return a string copy of the licensee name if licensed, or NULL, the caller needs to free this */
	char* (*licensee)(LV2_Handle instance);
	/* @return a URI identifying the plugin-bundle or plugin for which a given license is valid */
	const char* (*product_uri)(LV2_Handle instance);
	/* @return human readable product name for the URI */
	const char* (*product_name)(LV2_Handle instance);
	/* @return link to website or webstore */
	const char* (*store_url)(LV2_Handle instance);
	/* interface2 ext: preferred location to install the license file, the caller needs to free this */
	char* (*preferred_license_file_path)(LV2_Handle instance);
	/* interface2 ext: currently used license file (if any, may be NULL), the caller needs to free this */
	char* (*current_license_file_path)(LV2_Handle instance);
	/* interface2 ext: free() allocated strings (licensee, license_file_paths) */
	void (*free)(char*);
} LV2_License_Interface;

/**
   @}
*/

/**
   @defgroup lv2bypass Plugin-provided bypass

	 A port with the designation "processing#enable" must
	 control a plugin's internal bypass mode.

	 If the port value is larger than zero the plugin processes
	 normally.

	 If the port value is zero, the plugin is expected to bypass
	 all signals unmodified.

	 The plugin is responsible for providing a click-free transition
	 between the states.

	 (values less than zero are reserved for future use:
	 e.g click-free insert/removal of latent plugins.
	 Generally values <= 0 are to be treated as bypassed.)

   lv2:designation <http://ardour.org/lv2/processing#enable> ;

   @{
*/

#define LV2_PROCESSING_URI "http://ardour.org/lv2/processing"
#define LV2_PROCESSING_URI_PREFIX LV2_PROCESSING_URI "#"
#define LV2_PROCESSING_URI__enable LV2_PROCESSING_URI_PREFIX "enable"

/**
   @}
*/


/**
   @defgroup lv2routing plugin port/routing control

   This is a "feature" to simplify per port meta-data of
   http://lv2plug.in/ns/ext/port-groups/port-groups.html#source

   Plugins using this feature provide a strong hint that the host
   should always connect all audio output-ports.

   This allows mono->stereo plugins to override strict_io rules.

   @{
*/

#define LV2_ROUTING_URI "http://harrisonconsoles.com/lv2/routing"
#define LV2_ROUTING_PREFIX LV2_ROUTING_URI "#"
#define LV2_ROUTING__connectAllOutputs LV2_ROUTING_PREFIX "connectAllOutputs"

/**
   @}
*/

/**
   @defgroup lv2midnam MIDI Naming

   @{
*/


#define LV2_MIDNAM_URI "http://ardour.org/lv2/midnam"
#define LV2_MIDNAM_PREFIX LV2_MIDNAM_URI "#"
#define LV2_MIDNAM__interface LV2_MIDNAM_PREFIX "interface"
#define LV2_MIDNAM__update LV2_MIDNAM_PREFIX "update"

typedef void* LV2_Midnam_Handle;

/** a LV2 Feature provided by the Host to the plugin */
typedef struct {
	/** Opaque host data */
	LV2_Midnam_Handle handle;
	/** Request from run() that the host should re-read the midnam */
	void (*update)(LV2_Midnam_Handle handle);
} LV2_Midnam;

typedef struct {
	/** Query midnam document. The plugin
	 * is expected to return a null-terminated XML
	 * text which is a valid midnam desciption
	 * (or NULL in case of error).
	 *
	 * The midnam \<Model\> must be unique and
	 * specific for the given plugin-instance.
	 */
	char* (*midnam)(LV2_Handle instance);

	/** The unique model id used ith the midnam,
	 * (or NULL).
	 */
	char* (*model)(LV2_Handle instance);

	/** free allocated strings. The host
	 * calls this for every value returned by
	 * \ref midnam and \ref model.
	 */
	void (*free)(char*);
} LV2_Midnam_Interface;

/**
   @}
*/

/**
   @defgroup lv2bankpatch MIDI Bank/Patch Notifications

	 LV2 extension to allow a synth to inform a host about the
	 currentl used MIDI bank/program.

   @{
*/


#define LV2_BANKPATCH_URI "http://ardour.org/lv2/bankpatch"
#define LV2_BANKPATCH_PREFIX LV2_BANKPATCH_URI "#"
#define LV2_BANKPATCH__notify LV2_BANKPATCH_PREFIX "notify"

typedef void* LV2_BankPatch_Handle;

/** a LV2 Feature provided by the Host to the plugin */
typedef struct {
	/** Opaque host data */
	LV2_BankPatch_Handle handle;
	/** Info from plugin's run(), notify host that bank/program changed */
	void (*notify)(LV2_BankPatch_Handle handle, uint8_t channel, uint32_t bank, uint8_t pgm);
} LV2_BankPatch;

/**
   @}
*/
#endif
