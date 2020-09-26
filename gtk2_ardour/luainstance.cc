/*
 * Copyright (C) 2016-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2017-2018 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <pango/pangocairo.h>

#include "pbd/file_utils.h"
#include "pbd/strsplit.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/gui_thread.h"

#include "ardour/audioengine.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/filesystem_paths.h"
#include "ardour/plugin_manager.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/system_exec.h"

#include "LuaBridge/LuaBridge.h"

#include "ardour_http.h"
#include "ardour_ui.h"
#include "public_editor.h"
#include "region_selection.h"
#include "luadialog.h"
#include "luainstance.h"
#include "luasignal.h"
#include "marker.h"
#include "mixer_ui.h"
#include "region_view.h"
#include "processor_box.h"
#include "time_axis_view.h"
#include "time_axis_view_item.h"
#include "selection.h"
#include "script_selector.h"
#include "timers.h"
#include "ui_config.h"
#include "utils_videotl.h"

#include "pbd/i18n.h"

static const char* ui_scripts_file_name = "ui_scripts";

namespace LuaCairo {
/** wrap RefPtr< Cairo::ImageSurface >
 *
 * Image surfaces provide the ability to render to memory buffers either
 * allocated by cairo or by the calling code. The supported image formats are
 * those defined in Cairo::Format.
 */
class ImageSurface {
	public:
		/**
		 * Creates an image surface of the specified format and dimensions. Initially
		 * the surface contents are all 0. (Specifically, within each pixel, each
		 * color or alpha channel belonging to format will be 0. The contents of bits
		 * within a pixel, but not belonging to the given format are undefined).
		 *
		 * @param format  format of pixels in the surface to create
		 * @param width   width of the surface, in pixels
		 * @param height  height of the surface, in pixels
		 */
		ImageSurface (Cairo::Format format, int width, int height)
			: _surface (Cairo::ImageSurface::create (format, width, height))
			, _ctx (Cairo::Context::create (_surface))
			, ctx (_ctx->cobj ()) {}

		~ImageSurface () {}

		/**
		 * Set this surface as source for another context.
		 * This allows to draw this surface
		 */
		void set_as_source (Cairo::Context* c, int x, int y) {
			_surface->flush ();
			c->set_source (_surface, x, y);
		}

		/**
		 * Returns a context object to perform operations on the surface
		 */
		Cairo::Context* context () {
			return (Cairo::Context *)&ctx;
		}

		/**
		 * Returns the stride of the image surface in bytes (or 0 if surface is not
		 * an image surface). The stride is the distance in bytes from the beginning
		 * of one row of the image data to the beginning of the next row.
		 */
		int get_stride () const {
			return _surface->get_stride ();
		}

		/** Gets the width of the ImageSurface in pixels */
		int get_width () const {
			return _surface->get_width ();
		}

		/** Gets the height of the ImageSurface in pixels */
		int get_height () const {
			return _surface->get_height ();
		}

		/**
		 * Get a pointer to the data of the image surface, for direct
		 * inspection or modification.
		 *
		 * Return value: a pointer to the image data of this surface or NULL
		 * if @surface is not an image surface.
		 *
		 */
		unsigned char* get_data () {
			return _surface->get_data ();
		}

		/** Tells cairo to consider the data buffer dirty.
		 *
		 * In particular, if you've created an ImageSurface with a data buffer that
		 * you've allocated yourself and you draw to that data buffer using means
		 * other than cairo, you must call mark_dirty() before doing any additional
		 * drawing to that surface with cairo.
		 *
		 * Note that if you do draw to the Surface outside of cairo, you must call
		 * flush() before doing the drawing.
		 */
		void mark_dirty () {
			_surface->mark_dirty ();
		}

		/** Marks a rectangular area of the given surface dirty.
		 *
		 * @param x X coordinate of dirty rectangle
		 * @param y Y coordinate of dirty rectangle
		 * @param width width of dirty rectangle
		 * @param height height of dirty rectangle
		 */
		void mark_dirty (int x, int y, int width, int height) {
			_surface->mark_dirty (x, y, width, height);
		}

	private:
		Cairo::RefPtr<Cairo::ImageSurface> _surface;
		Cairo::RefPtr<Cairo::Context> _ctx;
		Cairo::Context ctx;
};

class PangoLayout {
	public:
		/** Create a new PangoLayout Text Display
		 * @param c CairoContext for the layout
		 * @param font_name a font-description e.g. "Mono 8px"
		 */
		PangoLayout (Cairo::Context* c, std::string font_name) {
			::PangoLayout* pl = pango_cairo_create_layout (c->cobj ());
			_layout = Glib::wrap (pl);
			Pango::FontDescription fd (font_name);
			_layout->set_font_description (fd);
		}

		~PangoLayout () {}

		/** Gets the text in the layout. The returned text should not
		 * be freed or modified.
		 *
		 * @return The text in the @a layout.
		 */
		std::string get_text () const {
			return _layout->get_text ();
		}
		/** Set the text of the layout.
		 * @param text The text for the layout.
		 */
		void set_text (const std::string& text) {
			_layout->set_text (text);
		}

		/** Sets the layout text and attribute list from marked-up text (see markup format).
		 * Replaces the current text and attribute list.
		 * @param markup Some marked-up text.
		 */
		void set_markup (const std::string& markup) {
			_layout->set_markup (markup);
		}

		/** Sets the width to which the lines of the Pango::Layout should wrap or
		 * ellipsized.  The default value is -1: no width set.
		 *
		 * @param width The desired width in Pango units, or -1 to indicate that no
		 * wrapping or ellipsization should be performed.
		 */
		void set_width (float width) {
			_layout->set_width (width * PANGO_SCALE);
		}

		/** Gets the width to which the lines of the Pango::Layout should wrap.
		 *
		 * @return The width in Pango units, or -1 if no width set.
		 */
		int get_width () const {
			return _layout->get_width () / PANGO_SCALE;
		}

		/** Sets the type of ellipsization being performed for @a layout.
		 * Depending on the ellipsization mode @a ellipsize text is
		 * removed from the start, middle, or end of text so they
		 * fit within the width and height of layout set with
		 * set_width() and set_height().
		 *
		 * If the layout contains characters such as newlines that
		 * force it to be layed out in multiple paragraphs, then whether
		 * each paragraph is ellipsized separately or the entire layout
		 * is ellipsized as a whole depends on the set height of the layout.
		 * See set_height() for details.
		 *
		 * @param ellipsize The new ellipsization mode for @a layout.
		 */
		void set_ellipsize (Pango::EllipsizeMode ellipsize) {
			_layout->set_ellipsize (ellipsize);
		}

		/** Gets the type of ellipsization being performed for @a layout.
		 * See set_ellipsize()
		 *
		 * @return The current ellipsization mode for @a layout.
		 *
		 * Use is_ellipsized() to query whether any paragraphs
		 * were actually ellipsized.
		 */
		Pango::EllipsizeMode get_ellipsize () const {
			return _layout->get_ellipsize ();
		}

		/** Queries whether the layout had to ellipsize any paragraphs.
		 *
		 * This returns <tt>true</tt> if the ellipsization mode for @a layout
		 * is not Pango::ELLIPSIZE_NONE, a positive width is set on @a layout,
		 * and there are paragraphs exceeding that width that have to be
		 * ellipsized.
		 *
		 * @return <tt>true</tt> if any paragraphs had to be ellipsized, <tt>false</tt>
		 * otherwise.
		 */
		bool is_ellipsized () const {
			return _layout->is_ellipsized ();
		}

		/** Sets the alignment for the layout: how partial lines are
		 * positioned within the horizontal space available.
		 * @param alignment The alignment.
		 */
		void set_alignment(Pango::Alignment alignment) {
			_layout->set_alignment (alignment);
		}

		/** Gets the alignment for the layout: how partial lines are
		 * positioned within the horizontal space available.
		 * @return The alignment.
		 */
		Pango::Alignment get_alignment() const {
			return _layout->get_alignment ();
		}

		/** Sets the wrap mode; the wrap mode only has effect if a width
		 * is set on the layout with set_width().
		 * To turn off wrapping, set the width to -1.
		 *
		 * @param wrap The wrap mode.
		 */
		void set_wrap (Pango::WrapMode wrap) {
			_layout->set_width (wrap);
		}

		/** Gets the wrap mode for the layout.
		 *
		 * Use is_wrapped() to query whether any paragraphs
		 * were actually wrapped.
		 *
		 * @return Active wrap mode.
		 */
		Pango::WrapMode get_wrap () const {
			return _layout->get_wrap ();
		}

		/** Queries whether the layout had to wrap any paragraphs.
		 *
		 * This returns <tt>true</tt> if a positive width is set on @a layout,
		 * ellipsization mode of @a layout is set to Pango::ELLIPSIZE_NONE,
		 * and there are paragraphs exceeding the layout width that have
		 * to be wrapped.
		 *
		 * @return <tt>true</tt> if any paragraphs had to be wrapped, <tt>false</tt>
		 * otherwise.
		 */
		bool is_wrapped () const {
			return _layout->is_wrapped ();
		}

		/** Determines the logical width and height of a Pango::Layout
		 * in device units.
		 */
		int get_pixel_size (lua_State *L) {
			int width, height;
			_layout->get_pixel_size (width, height);
			luabridge::Stack<int>::push (L, width);
			luabridge::Stack<int>::push (L, height);
			return 2;
		}

		/** Draws a Layout in the specified Cairo @a context. The top-left
		 *  corner of the Layout will be drawn at the current point of the
		 *  cairo context.
		 *
		 * @param context A Cairo context.
		 */
		void show_in_cairo_context (Cairo::Context* c) {
			pango_cairo_update_layout (c->cobj (), _layout->gobj());
			pango_cairo_show_layout (c->cobj (), _layout->gobj());
		}

		void layout_cairo_path (Cairo::Context* c) {
			pango_cairo_update_layout (c->cobj (), _layout->gobj());
			pango_cairo_layout_path (c->cobj (), _layout->gobj());
		}

	private:
		Glib::RefPtr<Pango::Layout> _layout;
};

}; // namespace

////////////////////////////////////////////////////////////////////////////////

namespace LuaSignal {

#define STATIC(name,c,p) else if (!strcmp(type, #name)) {return name;}
#define SESSION(name,c,p) else if (!strcmp(type, #name)) {return name;}
#define ENGINE(name,c,p) else if (!strcmp(type, #name)) {return name;}

LuaSignal
str2luasignal (const std::string &str) {
	const char* type = str.c_str();
	if (0) { }
#	include "luasignal_syms.h"
	else {
		PBD::fatal << string_compose (_("programming error: %1: %2"), "Impossible LuaSignal type", str) << endmsg;
		abort(); /*NOTREACHED*/
	}
}
#undef STATIC
#undef SESSION
#undef ENGINE

#define STATIC(name,c,p) N_(#name),
#define SESSION(name,c,p) N_(#name),
#define ENGINE(name,c,p) N_(#name),
const char *luasignalstr[] = {
#	include "luasignal_syms.h"
	0
};

#undef STATIC
#undef SESSION
#undef ENGINE
}; // namespace


static std::string http_get_unlogged (const std::string& url) { return ArdourCurl::http_get (url, false); }

/** special cases for Ardour's Mixer UI */
namespace LuaMixer {

	ProcessorBox::ProcSelection
	processor_selection () {
		return ProcessorBox::current_processor_selection ();
	}

};

static void mixer_screenshot (const std::string& fn) {
	Mixer_UI::instance()->screenshot (fn);
}

/** Access libardour global configuration */
static UIConfiguration* _ui_config () {
	return &UIConfiguration::instance();
}


////////////////////////////////////////////////////////////////////////////////

static PBD::ScopedConnectionList _luaexecs;

static void reaper (ARDOUR::SystemExec* x)
{
	delete x;
}

static int
lua_forkexec (lua_State *L)
{
	int argc = lua_gettop (L);
	if (argc == 0) {
		return luaL_argerror (L, 1, "invalid number of arguments, forkexec (command, ...)");
	}
	// args are free()ed in ~SystemExec
	char** args = (char**) malloc ((argc + 1) * sizeof(char*));
	for (int i = 0; i < argc; ++i) {
		args[i] = strdup (luaL_checkstring (L, i + 1));
	}
	args[argc] = 0;

	ARDOUR::SystemExec* x = new ARDOUR::SystemExec (args[0], args);
	x->Terminated.connect (_luaexecs, MISSING_INVALIDATOR, boost::bind (&reaper, x), gui_context());

	if (x->start()) {
		reaper (x);
		luabridge::Stack<bool>::push (L, false);
		return -1;
	} else {
		luabridge::Stack<bool>::push (L, false);
	}
	return 1;
}

#ifndef PLATFORM_WINDOWS
static int
lua_exec (std::string cmd)
{
	// args are free()ed in ~SystemExec
	char** args = (char**) malloc (4 * sizeof(char*));
	args[0] = strdup ("/bin/sh");
	args[1] = strdup ("-c");
	args[2] = strdup (cmd.c_str());
	args[3] = 0;
	ARDOUR::SystemExec x ("/bin/sh", args);
	if (x.start()) {
		return -1;
	}
	x.wait ();
	return 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////

static int
lua_actionlist (lua_State *L)
{
	using namespace std;

	vector<string> paths;
	vector<string> labels;
	vector<string> tooltips;
	vector<string> keys;
	vector<Glib::RefPtr<Gtk::Action> > actions;
	ActionManager::get_all_actions (paths, labels, tooltips, keys, actions);

	vector<string>::iterator p;
	vector<string>::iterator l;

	luabridge::LuaRef action_tbl (luabridge::newTable (L));

	for (l = labels.begin(), p = paths.begin(); l != labels.end(); ++p, ++l) {
		if (l->empty ()) {
			continue;
		}

		vector<string> parts;
		split (*p, parts, '/');

		if (parts.empty()) {
			continue;
		}

		//kinda kludgy way to avoid displaying menu items as mappable
		if (parts[1] == _("Main_menu"))
			continue;
		if (parts[1] == _("JACK"))
			continue;
		if (parts[1] == _("redirectmenu"))
			continue;
		if (parts[1] == _("RegionList"))
			continue;
		if (parts[1] == _("ProcessorMenu"))
			continue;

		if (!action_tbl[parts[1]].isTable()) {
			action_tbl[parts[1]] = luabridge::newTable (L);
		}
		assert (action_tbl[parts[1]].isTable());
		luabridge::LuaRef tbl (action_tbl[parts[1]]);
		assert (tbl.isTable());
		tbl[*l] = *p;
	}

	luabridge::push (L, action_tbl);
	return 1;
}

////////////////////////////////////////////////////////////////////////////////

// ARDOUR_UI and instance() are not exposed.
ARDOUR::PresentationInfo::order_t
lua_translate_order (RouteDialogs::InsertAt place)
{
	return ARDOUR_UI::instance()->translate_order (place);
}

////////////////////////////////////////////////////////////////////////////////

#define xstr(s) stringify(s)
#define stringify(s) #s

using namespace ARDOUR;

PBD::Signal0<void> LuaInstance::LuaTimerS;
PBD::Signal0<void> LuaInstance::LuaTimerDS;
PBD::Signal0<void> LuaInstance::SetSession;

void
LuaInstance::register_hooks (lua_State* L)
{

#define ENGINE(name,c,p) .addConst (stringify(name), (LuaSignal::LuaSignal)LuaSignal::name)
#define STATIC(name,c,p) .addConst (stringify(name), (LuaSignal::LuaSignal)LuaSignal::name)
#define SESSION(name,c,p) .addConst (stringify(name), (LuaSignal::LuaSignal)LuaSignal::name)
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("LuaSignal")
#		include "luasignal_syms.h"
		.endNamespace ();
#undef ENGINE
#undef SESSION
#undef STATIC

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("LuaSignal")
		.beginStdBitSet <LuaSignal::LAST_SIGNAL> ("Set")
		.endClass()
		.endNamespace ();

#if 0 // Dump size -> libs/ardour/luabindings.cc
	printf ("LuaInstance: registered %d signals\n", LuaSignal::LAST_SIGNAL);
#endif
}

void
LuaInstance::bind_cairo (lua_State* L)
{
	/* std::vector<double> for set_dash()
	 * for Windows (DLL, .exe) this needs to be bound in the same memory context as "Cairo".
	 *
	 * The std::vector<> argument in set_dash() has a fixed address in ardour.exe, while
	 * the address of the one in libardour.dll is mapped when loading the .dll
	 *
	 * see LuaBindings::set_session() for a detailed explanation
	 */
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("C")
		.registerArray <double> ("DoubleArray")
		.beginStdVector <double> ("DoubleVector")
		.endClass ()
		.endNamespace ();

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Cairo")
		.beginClass <Cairo::Context> ("Context")
		.addFunction ("save", &Cairo::Context::save)
		.addFunction ("restore", &Cairo::Context::restore)
		.addFunction ("set_operator", &Cairo::Context::set_operator)
		//.addFunction ("set_source", &Cairo::Context::set_operator) // needs RefPtr
		.addFunction ("set_source_rgb", &Cairo::Context::set_source_rgb)
		.addFunction ("set_source_rgba", &Cairo::Context::set_source_rgba)
		.addFunction ("set_line_width", &Cairo::Context::set_line_width)
		.addFunction ("set_line_cap", &Cairo::Context::set_line_cap)
		.addFunction ("set_line_join", &Cairo::Context::set_line_join)
		.addFunction ("set_dash", (void (Cairo::Context::*)(const std::vector<double>&, double))&Cairo::Context::set_dash)
		.addFunction ("unset_dash", &Cairo::Context::unset_dash)
		.addFunction ("translate", &Cairo::Context::translate)
		.addFunction ("scale", &Cairo::Context::scale)
		.addFunction ("rotate", &Cairo::Context::rotate)
		.addFunction ("begin_new_path", &Cairo::Context::begin_new_path)
		.addFunction ("begin_new_sub_path", &Cairo::Context::begin_new_sub_path)
		.addFunction ("move_to", &Cairo::Context::move_to)
		.addFunction ("line_to", &Cairo::Context::line_to)
		.addFunction ("curve_to", &Cairo::Context::curve_to)
		.addFunction ("arc", &Cairo::Context::arc)
		.addFunction ("arc_negative", &Cairo::Context::arc_negative)
		.addFunction ("rel_move_to", &Cairo::Context::rel_move_to)
		.addFunction ("rel_line_to", &Cairo::Context::rel_line_to)
		.addFunction ("rel_curve_to", &Cairo::Context::rel_curve_to)
		.addFunction ("rectangle", (void (Cairo::Context::*)(double, double, double, double))&Cairo::Context::rectangle)
		.addFunction ("close_path", &Cairo::Context::close_path)
		.addFunction ("paint", &Cairo::Context::paint)
		.addFunction ("paint_with_alpha", &Cairo::Context::paint_with_alpha)
		.addFunction ("stroke", &Cairo::Context::stroke)
		.addFunction ("stroke_preserve", &Cairo::Context::stroke_preserve)
		.addFunction ("fill", &Cairo::Context::fill)
		.addFunction ("fill_preserve", &Cairo::Context::fill_preserve)
		.addFunction ("reset_clip", &Cairo::Context::reset_clip)
		.addFunction ("clip", &Cairo::Context::clip)
		.addFunction ("clip_preserve", &Cairo::Context::clip_preserve)
		.addFunction ("set_font_size", &Cairo::Context::set_font_size)
		.addFunction ("show_text", &Cairo::Context::show_text)
		.endClass ()
		/* enums */
		// LineCap, LineJoin, Operator
		.beginNamespace ("LineCap")
		.addConst ("Butt", CAIRO_LINE_CAP_BUTT)
		.addConst ("Round", CAIRO_LINE_CAP_ROUND)
		.addConst ("Square", CAIRO_LINE_CAP_SQUARE)
		.endNamespace ()

		.beginNamespace ("LineJoin")
		.addConst ("Miter", CAIRO_LINE_JOIN_MITER)
		.addConst ("Round", CAIRO_LINE_JOIN_ROUND)
		.addConst ("Bevel", CAIRO_LINE_JOIN_BEVEL)
		.endNamespace ()

		.beginNamespace ("Operator")
		.addConst ("Clear", CAIRO_OPERATOR_CLEAR)
		.addConst ("Source", CAIRO_OPERATOR_SOURCE)
		.addConst ("Over", CAIRO_OPERATOR_OVER)
		.addConst ("Add", CAIRO_OPERATOR_ADD)
		.endNamespace ()

		.beginNamespace ("Format")
		.addConst ("ARGB32", CAIRO_FORMAT_ARGB32)
		.addConst ("RGB24", CAIRO_FORMAT_RGB24)
		.endNamespace ()

		.beginClass <LuaCairo::ImageSurface> ("ImageSurface")
		.addConstructor <void (*) (Cairo::Format, int, int)> ()
		.addFunction ("set_as_source", &LuaCairo::ImageSurface::set_as_source)
		.addFunction ("context", &LuaCairo::ImageSurface::context)
		.addFunction ("get_stride", &LuaCairo::ImageSurface::get_stride)
		.addFunction ("get_width", &LuaCairo::ImageSurface::get_width)
		.addFunction ("get_height", &LuaCairo::ImageSurface::get_height)
		//.addFunction ("get_data", &LuaCairo::ImageSurface::get_data) // uint8_t* array is n/a
		.endClass ()

		.beginClass <LuaCairo::PangoLayout> ("PangoLayout")
		.addConstructor <void (*) (Cairo::Context*, std::string)> ()
		.addCFunction ("get_pixel_size", &LuaCairo::PangoLayout::get_pixel_size)
		.addFunction ("get_text", &LuaCairo::PangoLayout::get_text)
		.addFunction ("set_text", &LuaCairo::PangoLayout::set_text)
		.addFunction ("show_in_cairo_context", &LuaCairo::PangoLayout::show_in_cairo_context)
		.addFunction ("layout_cairo_path", &LuaCairo::PangoLayout::layout_cairo_path)
		.addFunction ("set_markup", &LuaCairo::PangoLayout::set_markup)
		.addFunction ("set_width", &LuaCairo::PangoLayout::set_width)
		.addFunction ("set_ellipsize", &LuaCairo::PangoLayout::set_ellipsize)
		.addFunction ("get_ellipsize", &LuaCairo::PangoLayout::get_ellipsize)
		.addFunction ("is_ellipsized", &LuaCairo::PangoLayout::is_ellipsized)
		.addFunction ("set_alignment", &LuaCairo::PangoLayout::set_alignment)
		.addFunction ("get_alignment", &LuaCairo::PangoLayout::get_alignment)
		.addFunction ("set_wrap", &LuaCairo::PangoLayout::set_wrap)
		.addFunction ("get_wrap", &LuaCairo::PangoLayout::get_wrap)
		.addFunction ("is_wrapped", &LuaCairo::PangoLayout::is_wrapped)
		.endClass ()

		/* enums */
		.beginNamespace ("EllipsizeMode")
		.addConst ("None", Pango::ELLIPSIZE_NONE)
		.addConst ("Start", Pango::ELLIPSIZE_START)
		.addConst ("Middle", Pango::ELLIPSIZE_MIDDLE)
		.addConst ("End", Pango::ELLIPSIZE_END)
		.endNamespace ()

		.beginNamespace ("Alignment")
		.addConst ("Left", Pango::ALIGN_LEFT)
		.addConst ("Center", Pango::ALIGN_CENTER)
		.addConst ("Right", Pango::ALIGN_RIGHT)
		.endNamespace ()

		.beginNamespace ("WrapMode")
		.addConst ("Word", Pango::WRAP_WORD)
		.addConst ("Char", Pango::WRAP_CHAR)
		.addConst ("WordChar", Pango::WRAP_WORD_CHAR)
		.endNamespace ()

		.endNamespace ();

/* Lua/cairo bindings operate on Cairo::Context, there is no Cairo::RefPtr wrapper [yet].
  one can work around this as follows:

  LuaState lua;
  LuaInstance::register_classes (lua.getState());
  lua.do_command (
      "function render (ctx)"
      "  ctx:rectangle (0, 0, 100, 100)"
      "  ctx:set_source_rgba (0.1, 1.0, 0.1, 1.0)"
      "  ctx:fill ()"
      " end"
      );
  {
		Cairo::RefPtr<Cairo::Context> context = get_window ()->create_cairo_context ();
    Cairo::Context ctx (context->cobj ());

    luabridge::LuaRef lua_render = luabridge::getGlobal (lua.getState(), "render");
    lua_render ((Cairo::Context *)&ctx);
  }
*/

}

void
LuaInstance::bind_dialog (lua_State* L)
{
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("LuaDialog")

		.beginClass <LuaDialog::Message> ("Message")
		.addConstructor <void (*) (std::string const&, std::string const&, LuaDialog::Message::MessageType, LuaDialog::Message::ButtonType)> ()
		.addFunction ("run", &LuaDialog::Message::run)
		.endClass ()

		.beginClass <LuaDialog::Dialog> ("Dialog")
		.addConstructor <void (*) (std::string const&, luabridge::LuaRef)> ()
		.addCFunction ("run", &LuaDialog::Dialog::run)
		.endClass ()

		/* enums */
		.beginNamespace ("MessageType")
		.addConst ("Info", LuaDialog::Message::Info)
		.addConst ("Warning", LuaDialog::Message::Warning)
		.addConst ("Question", LuaDialog::Message::Question)
		.addConst ("Error", LuaDialog::Message::Error)
		.endNamespace ()

		.beginNamespace ("ButtonType")
		.addConst ("OK", LuaDialog::Message::OK)
		.addConst ("Close", LuaDialog::Message::Close)
		.addConst ("Cancel", LuaDialog::Message::Cancel)
		.addConst ("Yes_No", LuaDialog::Message::Yes_No)
		.addConst ("OK_Cancel", LuaDialog::Message::OK_Cancel)
		.endNamespace ()

		.beginNamespace ("Response")
		.addConst ("OK", 0)
		.addConst ("Cancel", 1)
		.addConst ("Close", 2)
		.addConst ("Yes", 3)
		.addConst ("No", 4)
		.addConst ("None", -1)
		.endNamespace ()

		.beginClass <LuaDialog::ProgressWindow> ("ProgressWindow")
		.addConstructor <void (*) (std::string const&, bool)> ()
		.addFunction ("progress", &LuaDialog::ProgressWindow::progress)
		.addFunction ("done", &LuaDialog::ProgressWindow::done)
		.addFunction ("canceled", &LuaDialog::ProgressWindow::canceled)
		.endClass ()

		.endNamespace ();
}

void
LuaInstance::register_classes (lua_State* L)
{
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::session (L);
	LuaBindings::osc (L);

	bind_cairo (L);
	bind_dialog (L);

	luabridge::getGlobalNamespace (L)
		.beginNamespace ("ArdourUI")

		.addFunction ("http_get", &http_get_unlogged)

		.addFunction ("mixer_screenshot", &mixer_screenshot)

		.addFunction ("processor_selection", &LuaMixer::processor_selection)

		.beginStdCPtrList <ArdourMarker> ("ArdourMarkerList")
		.endClass ()

		.beginClass <ArdourMarker> ("ArdourMarker")
		.addFunction ("name", &ArdourMarker::name)
		.addFunction ("position", &ArdourMarker::position)
		.addFunction ("_type", &ArdourMarker::type)
		.endClass ()

		.beginClass <AxisView> ("AxisView")
		.endClass ()

		.deriveClass <TimeAxisView, AxisView> ("TimeAxisView")
		.addFunction ("order", &TimeAxisView::order)
		.addFunction ("y_position", &TimeAxisView::y_position)
		.addFunction ("effective_height", &TimeAxisView::effective_height)
		.addFunction ("current_height", &TimeAxisView::current_height)
		.addFunction ("set_height", &TimeAxisView::set_height)
		.endClass ()

		.deriveClass <StripableTimeAxisView, TimeAxisView> ("StripableTimeAxisView")
		.endClass ()

		.beginClass <Selectable> ("Selectable")
		.endClass ()

		.deriveClass <TimeAxisViewItem, Selectable> ("TimeAxisViewItem")
		.endClass ()

		.deriveClass <RegionView, TimeAxisViewItem> ("RegionView")
		.endClass ()

		.deriveClass <RouteUI, Selectable> ("RouteUI")
		.endClass ()

		.deriveClass <RouteTimeAxisView, RouteUI> ("RouteTimeAxisView")
		.addCast<StripableTimeAxisView> ("to_stripabletimeaxisview")
		.addCast<TimeAxisView> ("to_timeaxisview") // deprecated
		.endClass ()

		// std::list<Selectable*>
		.beginStdCPtrList <Selectable> ("SelectionList")
		.endClass ()

		// std::list<TimeAxisView*>
		.beginConstStdCPtrList <TimeAxisView> ("TrackViewStdList")
		.endClass ()


		.beginClass <RegionSelection> ("RegionSelection")
		.addFunction ("start", &RegionSelection::start)
		.addFunction ("end_sample", &RegionSelection::end_sample)
		.addFunction ("n_midi_regions", &RegionSelection::n_midi_regions)
		.addFunction ("regionlist", &RegionSelection::regionlist) // XXX check windows binding (libardour)
		.endClass ()

		.deriveClass <TimeSelection, std::list<ARDOUR::TimelineRange> > ("TimeSelection")
		.addFunction ("start", &TimeSelection::start)
		.addFunction ("end_sample", &TimeSelection::end_sample)
		.addFunction ("length", &TimeSelection::length)
		.endClass ()

		.deriveClass <MarkerSelection, std::list<ArdourMarker*> > ("MarkerSelection")
		.endClass ()

		.beginClass <TrackViewList> ("TrackViewList")
		.addCast<std::list<TimeAxisView*> > ("to_tav_list")
		.addFunction ("contains", &TrackViewList::contains)
		.addFunction ("routelist", &TrackViewList::routelist)
		.endClass ()

		.deriveClass <TrackSelection, TrackViewList> ("TrackSelection")
		.endClass ()

		.beginClass <Selection> ("Selection")
		.addFunction ("clear", &Selection::clear)
		.addFunction ("clear_all", &Selection::clear_all)
		.addFunction ("empty", &Selection::empty)
		.addData ("tracks", &Selection::tracks)
		.addData ("regions", &Selection::regions)
		.addData ("time", &Selection::time)
		.addData ("markers", &Selection::markers)
#if 0
		.addData ("lines", &Selection::lines)
		.addData ("playlists", &Selection::playlists)
		.addData ("points", &Selection::points)
		.addData ("midi_regions", &Selection::midi_regions)
		.addData ("midi_notes", &Selection::midi_notes) // cut buffer only
#endif
		.endClass ()

		.beginClass <PublicEditor> ("Editor")
		.addFunction ("grid_type", &PublicEditor::grid_type)
		.addFunction ("snap_mode", &PublicEditor::snap_mode)
		.addFunction ("set_snap_mode", &PublicEditor::set_snap_mode)

		.addFunction ("undo", &PublicEditor::undo)
		.addFunction ("redo", &PublicEditor::redo)

		.addFunction ("set_mouse_mode", &PublicEditor::set_mouse_mode)
		.addFunction ("current_mouse_mode", &PublicEditor::current_mouse_mode)

		.addFunction ("consider_auditioning", &PublicEditor::consider_auditioning)

		.addFunction ("new_region_from_selection", &PublicEditor::new_region_from_selection)
		.addFunction ("separate_region_from_selection", &PublicEditor::separate_region_from_selection)
		.addFunction ("pixel_to_sample", &PublicEditor::pixel_to_sample)
		.addFunction ("sample_to_pixel", &PublicEditor::sample_to_pixel)

		.addFunction ("get_selection", &PublicEditor::get_selection)
		.addFunction ("get_cut_buffer", &PublicEditor::get_cut_buffer)
		.addRefFunction ("get_selection_extents", &PublicEditor::get_selection_extents)

		.addFunction ("set_selection", &PublicEditor::set_selection)

		.addFunction ("play_selection", &PublicEditor::play_selection)
		.addFunction ("play_with_preroll", &PublicEditor::play_with_preroll)
		.addFunction ("maybe_locate_with_edit_preroll", &PublicEditor::maybe_locate_with_edit_preroll)
		.addFunction ("goto_nth_marker", &PublicEditor::goto_nth_marker)

		.addFunction ("add_location_from_playhead_cursor", &PublicEditor::add_location_from_playhead_cursor)
		.addFunction ("remove_location_at_playhead_cursor", &PublicEditor::remove_location_at_playhead_cursor)
		.addFunction ("add_location_mark", &PublicEditor::add_location_mark)

		.addFunction ("update_grid", &PublicEditor::update_grid)
		.addFunction ("remove_tracks", &PublicEditor::remove_tracks)

		.addFunction ("set_loop_range", &PublicEditor::set_loop_range)
		.addFunction ("set_punch_range", &PublicEditor::set_punch_range)

		.addFunction ("effective_mouse_mode", &PublicEditor::effective_mouse_mode)

		.addRefFunction ("do_import", &PublicEditor::do_import)
		.addRefFunction ("do_embed", &PublicEditor::do_embed)

		.addFunction ("export_audio", &PublicEditor::export_audio)
		.addFunction ("stem_export", &PublicEditor::stem_export)
		.addFunction ("export_selection", &PublicEditor::export_selection)
		.addFunction ("export_range", &PublicEditor::export_range)

		.addFunction ("set_zoom_focus", &PublicEditor::set_zoom_focus)
		.addFunction ("get_zoom_focus", &PublicEditor::get_zoom_focus)
		.addFunction ("get_current_zoom", &PublicEditor::get_current_zoom)
		.addFunction ("reset_zoom", &PublicEditor::reset_zoom)

		.addFunction ("clear_playlist", &PublicEditor::clear_playlist)
		.addFunction ("clear_grouped_playlists", &PublicEditor::clear_grouped_playlists)

		.addFunction ("new_playlists_for_grouped_tracks", &PublicEditor::new_playlists_for_grouped_tracks)
		.addFunction ("new_playlists_for_all_tracks", &PublicEditor::new_playlists_for_all_tracks)
		.addFunction ("new_playlists_for_armed_tracks", &PublicEditor::new_playlists_for_armed_tracks)
		.addFunction ("new_playlists_for_selected_tracks", &PublicEditor::new_playlists_for_selected_tracks)

		.addFunction ("select_all_visible_lanes", &PublicEditor::select_all_visible_lanes)
		.addFunction ("select_all_tracks", &PublicEditor::select_all_tracks)
		.addFunction ("deselect_all", &PublicEditor::deselect_all)

#if 0 // TimeAxisView&  can't be bound (pure virtual fn)
		.addFunction ("set_selected_track", &PublicEditor::set_selected_track)
		.addFunction ("set_selected_mixer_strip", &PublicEditor::set_selected_mixer_strip)
		.addFunction ("ensure_time_axis_view_is_visible", &PublicEditor::ensure_time_axis_view_is_visible)
#endif
		.addFunction ("hide_track_in_display", &PublicEditor::hide_track_in_display)
		.addFunction ("show_track_in_display", &PublicEditor::show_track_in_display)
		.addFunction ("set_visible_track_count", &PublicEditor::set_visible_track_count)
		.addFunction ("fit_selection", &PublicEditor::fit_selection)

		.addFunction ("regionview_from_region", &PublicEditor::regionview_from_region)
		.addFunction ("set_stationary_playhead", &PublicEditor::set_stationary_playhead)
		.addFunction ("stationary_playhead", &PublicEditor::stationary_playhead)
		.addFunction ("set_follow_playhead", &PublicEditor::set_follow_playhead)
		.addFunction ("follow_playhead", &PublicEditor::follow_playhead)

		.addFunction ("dragging_playhead", &PublicEditor::dragging_playhead)
		.addFunction ("leftmost_sample", &PublicEditor::leftmost_sample)
		.addFunction ("current_page_samples", &PublicEditor::current_page_samples)
		.addFunction ("visible_canvas_height", &PublicEditor::visible_canvas_height)
		.addFunction ("temporal_zoom_step", &PublicEditor::temporal_zoom_step)
		.addFunction ("override_visible_track_count", &PublicEditor::override_visible_track_count)

		.addFunction ("scroll_tracks_down_line", &PublicEditor::scroll_tracks_down_line)
		.addFunction ("scroll_tracks_up_line", &PublicEditor::scroll_tracks_up_line)
		.addFunction ("scroll_down_one_track", &PublicEditor::scroll_down_one_track)
		.addFunction ("scroll_up_one_track", &PublicEditor::scroll_up_one_track)

		.addFunction ("reset_x_origin", &PublicEditor::reset_x_origin)
		.addFunction ("get_y_origin", &PublicEditor::get_y_origin)
		.addFunction ("reset_y_origin", &PublicEditor::reset_y_origin)

		.addFunction ("remove_last_capture", &PublicEditor::remove_last_capture)

		.addFunction ("maximise_editing_space", &PublicEditor::maximise_editing_space)
		.addFunction ("restore_editing_space", &PublicEditor::restore_editing_space)
		.addFunction ("toggle_meter_updating", &PublicEditor::toggle_meter_updating)

		//.addFunction ("get_preferred_edit_position", &PublicEditor::get_preferred_edit_position)
		//.addFunction ("split_regions_at", &PublicEditor::split_regions_at)

		.addRefFunction ("get_nudge_distance", &PublicEditor::get_nudge_distance)
		.addFunction ("get_paste_offset", &PublicEditor::get_paste_offset)
		.addFunction ("get_grid_beat_divisions", &PublicEditor::get_grid_beat_divisions)
		.addRefFunction ("get_grid_type_as_beats", &PublicEditor::get_grid_type_as_beats)

		.addFunction ("toggle_ruler_video", &PublicEditor::toggle_ruler_video)
		.addFunction ("toggle_xjadeo_proc", &PublicEditor::toggle_xjadeo_proc)
		.addFunction ("get_videotl_bar_height", &PublicEditor::get_videotl_bar_height)
		.addFunction ("set_video_timeline_height", &PublicEditor::set_video_timeline_height)

#if 0
		.addFunction ("get_equivalent_regions", &PublicEditor::get_equivalent_regions)
		.addFunction ("drags", &PublicEditor::drags)
#endif

		.addFunction ("get_stripable_time_axis_by_id", &PublicEditor::get_stripable_time_axis_by_id)
		.addFunction ("get_track_views", &PublicEditor::get_track_views)
		.addFunction ("rtav_from_route", &PublicEditor::rtav_from_route)
		.addFunction ("axis_views_from_routes", &PublicEditor::axis_views_from_routes)

		.addFunction ("center_screen", &PublicEditor::center_screen)

		.addFunction ("get_smart_mode", &PublicEditor::get_smart_mode)
		.addRefFunction ("get_pointer_position", &PublicEditor::get_pointer_position)

		.addRefFunction ("find_location_from_marker", &PublicEditor::find_location_from_marker)
		.addFunction ("find_marker_from_location_id", &PublicEditor::find_marker_from_location_id)
		.addFunction ("mouse_add_new_marker", &PublicEditor::mouse_add_new_marker)
#if 0
		.addFunction ("get_regions_at", &PublicEditor::get_regions_at)
		.addFunction ("get_regions_after", &PublicEditor::get_regions_after)
		.addFunction ("get_regions_from_selection_and_mouse", &PublicEditor::get_regions_from_selection_and_mouse)
		.addFunction ("get_regionviews_by_id", &PublicEditor::get_regionviews_by_id)
		.addFunction ("get_per_region_note_selection", &PublicEditor::get_per_region_note_selection)
#endif

#if 0
		.addFunction ("mouse_add_new_tempo_event", &PublicEditor::mouse_add_new_tempo_event)
		.addFunction ("mouse_add_new_meter_event", &PublicEditor::mouse_add_new_meter_event)
		.addFunction ("edit_tempo_section", &PublicEditor::edit_tempo_section)
		.addFunction ("edit_meter_section", &PublicEditor::edit_meter_section)
#endif

		.addFunction ("access_action", &PublicEditor::access_action)
		.addFunction ("set_toggleaction", &PublicEditor::set_toggleaction)
		.endClass ()

		.addFunction ("translate_order", &lua_translate_order)

		/* ArdourUI enums */
		.beginNamespace ("InsertAt")
		.addConst ("BeforeSelection", RouteDialogs::InsertAt(RouteDialogs::BeforeSelection))
		.addConst ("AfterSelection", RouteDialogs::InsertAt(RouteDialogs::AfterSelection))
		.addConst ("First", RouteDialogs::InsertAt(RouteDialogs::First))
		.addConst ("Last", RouteDialogs::InsertAt(RouteDialogs::Last))
		.endNamespace ()

		.beginNamespace ("MarkerType")
		.addConst ("Mark", ArdourMarker::Type(ArdourMarker::Mark))
		.addConst ("Tempo", ArdourMarker::Type(ArdourMarker::Tempo))
		.addConst ("Meter", ArdourMarker::Type(ArdourMarker::Meter))
		.addConst ("SessionStart", ArdourMarker::Type(ArdourMarker::SessionStart))
		.addConst ("SessionEnd", ArdourMarker::Type(ArdourMarker::SessionEnd))
		.addConst ("RangeStart", ArdourMarker::Type(ArdourMarker::RangeStart))
		.addConst ("RangeEnd", ArdourMarker::Type(ArdourMarker::RangeEnd))
		.addConst ("LoopStart", ArdourMarker::Type(ArdourMarker::LoopStart))
		.addConst ("LoopEnd", ArdourMarker::Type(ArdourMarker::LoopEnd))
		.addConst ("PunchIn", ArdourMarker::Type(ArdourMarker::PunchIn))
		.addConst ("PunchOut", ArdourMarker::Type(ArdourMarker::PunchOut))
		.endNamespace ()

		.beginNamespace ("SelectionOp")
		.addConst ("Toggle", Selection::Operation(Selection::Toggle))
		.addConst ("Set", Selection::Operation(Selection::Set))
		.addConst ("Extend", Selection::Operation(Selection::Extend))
		.addConst ("Add", Selection::Operation(Selection::Add))
		.endNamespace ()

		.beginNamespace ("TrackHeightMode")
		.addConst ("OnlySelf", TimeAxisView::TrackHeightMode(TimeAxisView::OnlySelf))
		.addConst ("TotalHeight", TimeAxisView::TrackHeightMode(TimeAxisView::TotalHeight))
		.addConst ("HeightPerLane,", TimeAxisView::TrackHeightMode(TimeAxisView::HeightPerLane))
		.endNamespace ()

		.addCFunction ("actionlist", &lua_actionlist)


		.beginClass <UIConfiguration> ("UIConfiguration")
#undef  UI_CONFIG_VARIABLE
#define UI_CONFIG_VARIABLE(Type,var,name,value) \
		.addFunction ("get_" # var, &UIConfiguration::get_##var) \
		.addFunction ("set_" # var, &UIConfiguration::set_##var) \
		.addProperty (#var, &UIConfiguration::get_##var, &UIConfiguration::set_##var)

#include "ui_config_vars.h"

#undef UI_CONFIG_VARIABLE
		.endClass()

		.addFunction ("config", &_ui_config)

		.endNamespace () // end ArdourUI

		.beginNamespace ("os")
#ifndef PLATFORM_WINDOWS
		.addFunction ("execute", &lua_exec)
#endif
		.addCFunction ("forkexec", &lua_forkexec)
		.endNamespace ();

	// Editing Symbols

#undef ZOOMFOCUS
#undef GRIDTYPE
#undef SNAPMODE
#undef MOUSEMODE
#undef DISPLAYCONTROL
#undef IMPORTMODE
#undef IMPORTPOSITION
#undef IMPORTDISPOSITION

#define ZOOMFOCUS(NAME) .addConst (stringify(NAME), (Editing::ZoomFocus)Editing::NAME)
#define GRIDTYPE(NAME) .addConst (stringify(NAME), (Editing::GridType)Editing::NAME)
#define SNAPMODE(NAME) .addConst (stringify(NAME), (Editing::SnapMode)Editing::NAME)
#define MOUSEMODE(NAME) .addConst (stringify(NAME), (Editing::MouseMode)Editing::NAME)
#define DISPLAYCONTROL(NAME) .addConst (stringify(NAME), (Editing::DisplayControl)Editing::NAME)
#define IMPORTMODE(NAME) .addConst (stringify(NAME), (Editing::ImportMode)Editing::NAME)
#define IMPORTPOSITION(NAME) .addConst (stringify(NAME), (Editing::ImportPosition)Editing::NAME)
#define IMPORTDISPOSITION(NAME) .addConst (stringify(NAME), (Editing::ImportDisposition)Editing::NAME)
	luabridge::getGlobalNamespace (L)
		.beginNamespace ("Editing")
#		include "editing_syms.h"
		.endNamespace ();
}

#undef xstr
#undef stringify

////////////////////////////////////////////////////////////////////////////////

using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace std;

static void _lua_print (std::string s) {
#ifndef NDEBUG
	std::cout << "LuaInstance: " << s << "\n";
#endif
	PBD::info << "LuaInstance: " << s << endmsg;
}

LuaInstance* LuaInstance::_instance = 0;

LuaInstance*
LuaInstance::instance ()
{
	if (!_instance) {
		_instance  = new LuaInstance;
	}

	return _instance;
}

void
LuaInstance::destroy_instance ()
{
	delete _instance;
	_instance = 0;
}

LuaInstance::LuaInstance ()
{
	lua.Print.connect (&_lua_print);
	init ();
}

LuaInstance::~LuaInstance ()
{
	delete _lua_call_action;
	delete _lua_render_icon;
	delete _lua_add_action;
	delete _lua_del_action;
	delete _lua_get_action;

	delete _lua_load;
	delete _lua_save;
	delete _lua_clear;
	_callbacks.clear();
}

void
LuaInstance::init ()
{
	lua.sandbox (false);
	lua.do_command (
			"function ScriptManager ()"
			"  local self = { scripts = {}, instances = {}, icons = {} }"
			""
			"  local remove = function (id)"
			"   self.scripts[id] = nil"
			"   self.instances[id] = nil"
			"   self.icons[id] = nil"
			"  end"
			""
			"  local addinternal = function (i, n, s, f, c, a)"
			"   assert(type(i) == 'number', 'id must be numeric')"
			"   assert(type(n) == 'string', 'Name must be string')"
			"   assert(type(s) == 'string', 'Script must be string')"
			"   assert(type(f) == 'function', 'Factory is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   self.scripts[i] = { ['n'] = n, ['s'] = s, ['f'] = f, ['a'] = a, ['c'] = c }"
			"   local env = _ENV; env.f = nil"
			"   self.instances[i] = load (string.dump(f, true), nil, nil, env)(a)"
			"   if type(c) == 'function' then"
			"     self.icons[i] = load (string.dump(c, true), nil, nil, env)(a)"
			"   else"
			"     self.icons[i] = nil"
			"   end"
			"  end"
			""
			"  local call = function (id)"
			"   if type(self.instances[id]) == 'function' then"
			"     local status, err = pcall (self.instances[id])"
			"     if not status then"
			"       print ('action \"'.. id .. '\": ', err)" // error out
			"       remove (id)"
			"     end"
			"   end"
			"   collectgarbage()"
			"  end"
			""
			"  local icon = function (id, ...)"
			"   if type(self.icons[id]) == 'function' then"
			"     pcall (self.icons[id], ...)"
			"   end"
			"   collectgarbage()"
			"  end"
			""
			"  local add = function (i, n, s, b, c, a)"
			"   assert(type(b) == 'string', 'ByteCode must be string')"
			"   f = nil load (b)()" // assigns f
			"   icn = nil load (c)()" // may assign "icn"
			"   assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"   addinternal (i, n, s, load(f), type(icn) ~= \"string\" or icn == '' or load(icn), a)"
			"  end"
			""
			"  local get = function (id)"
			"   if type(self.scripts[id]) == 'table' then"
			"    return { ['name'] = self.scripts[id]['n'],"
			"             ['script'] = self.scripts[id]['s'],"
			"             ['icon'] = type(self.scripts[id]['c']) == 'function',"
			"             ['args'] = self.scripts[id]['a'] }"
			"   end"
			"   return nil"
			"  end"
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value, true))"
			"   elseif type(value) == \"boolean\" then"
			"     return rv .. tostring (value)"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			""
			"  local save = function ()"
			"   return (serialize('scripts', self.scripts))"
			"  end"
			""
			"  local clear = function ()"
			"   self.scripts = {}"
			"   self.instances = {}"
			"   self.icons = {}"
			"   collectgarbage()"
			"  end"
			""
			"  local restore = function (state)"
			"   clear()"
			"   load (state)()"
			"   for i, s in pairs (scripts) do"
			"    addinternal (i, s['n'], s['s'], load(s['f']), type (s['c']) ~= \"string\" or s['c'] == '' or load (s['c']), s['a'])"
			"   end"
			"   collectgarbage()"
			"  end"
			""
			" return { call = call, add = add, remove = remove, get = get,"
			"          restore = restore, save = save, clear = clear, icon = icon}"
			" end"
			" "
			" manager = ScriptManager ()"
			" ScriptManager = nil"
			);
	lua_State* L = lua.getState();

	try {
		luabridge::LuaRef lua_mgr = luabridge::getGlobal (L, "manager");
		lua.do_command ("manager = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		_lua_add_action = new luabridge::LuaRef(lua_mgr["add"]);
		_lua_del_action = new luabridge::LuaRef(lua_mgr["remove"]);
		_lua_get_action = new luabridge::LuaRef(lua_mgr["get"]);
		_lua_call_action = new luabridge::LuaRef(lua_mgr["call"]);
		_lua_render_icon = new luabridge::LuaRef(lua_mgr["icon"]);
		_lua_save = new luabridge::LuaRef(lua_mgr["save"]);
		_lua_load = new luabridge::LuaRef(lua_mgr["restore"]);
		_lua_clear = new luabridge::LuaRef(lua_mgr["clear"]);

	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				std::string ("Failed to setup Lua action interpreter") + e.what ())
			<< endmsg;
		abort(); /*NOTREACHED*/
	} catch (...) {
		fatal << string_compose (_("programming error: %1"),
				X_("Failed to setup Lua action interpreter"))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}

	register_classes (L);
	register_hooks (L);

	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");
}

int
LuaInstance::load_state ()
{
	std::string uiscripts;
	if (!find_file (ardour_config_search_path(), ui_scripts_file_name, uiscripts)) {
		pre_seed_scripts ();
		return -1;
	}
	XMLTree tree;

	info << string_compose (_("Loading user ui scripts file %1"), uiscripts) << endmsg;

	if (!tree.read (uiscripts)) {
		error << string_compose(_("cannot read ui scripts file \"%1\""), uiscripts) << endmsg;
		return -1;
	}

	if (set_state (*tree.root())) {
		error << string_compose(_("user ui scripts file \"%1\" not loaded successfully."), uiscripts) << endmsg;
		return -1;
	}

	return 0;
}

int
LuaInstance::save_state ()
{
	if (!_session) {
		/* action scripts are un-registered with the session */
		return -1;
	}

	std::string uiscripts = Glib::build_filename (user_config_directory(), ui_scripts_file_name);

	XMLNode* node = new XMLNode (X_("UIScripts"));
	node->add_child_nocopy (get_action_state ());
	node->add_child_nocopy (get_hook_state ());

	XMLTree tree;
	tree.set_root (node);

	if (!tree.write (uiscripts.c_str())){
		error << string_compose (_("UI script file %1 not saved"), uiscripts) << endmsg;
		return -1;
	}
	return 0;
}

void
LuaInstance::set_dirty ()
{
	if (!_session || _session->deletion_in_progress()) {
		return;
	}
	save_state ();
	_session->set_dirty (); // XXX is this reasonable?
}

void LuaInstance::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	if (!_session) {
		return;
	}

	load_state ();

	lua_State* L = lua.getState();
	LuaBindings::set_session (L, _session);

	for (LuaCallbackMap::iterator i = _callbacks.begin(); i != _callbacks.end(); ++i) {
		i->second->set_session (s);
	}
	second_connection = Timers::rapid_connect (sigc::mem_fun(*this, & LuaInstance::every_second));
	point_one_second_connection = Timers::rapid_connect (sigc::mem_fun(*this, & LuaInstance::every_point_one_seconds));
	SetSession (); /* EMIT SIGNAL */
}

void
LuaInstance::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &LuaInstance::session_going_away);
	second_connection.disconnect ();
	point_one_second_connection.disconnect ();

	(*_lua_clear)();
	for (int i = 0; i < MAX_LUA_ACTION_SCRIPTS; ++i) {
		ActionChanged (i, ""); /* EMIT SIGNAL */
	}
	SessionHandlePtr::session_going_away ();
	_session = 0;

	lua_State* L = lua.getState();
	LuaBindings::set_session (L, _session);
	lua.collect_garbage ();
}

void
LuaInstance::every_second ()
{
	LuaTimerS (); // emit signal
}

void
LuaInstance::every_point_one_seconds ()
{
	LuaTimerDS (); // emit signal
}

int
LuaInstance::set_state (const XMLNode& node)
{
	XMLNode* child;

	if ((child = find_named_node (node, "ActionScript"))) {
		for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
			if (!(*n)->is_content ()) { continue; }
			gsize size;
			guchar* buf = g_base64_decode ((*n)->content ().c_str (), &size);
			try {
				(*_lua_load)(std::string ((const char*)buf, size));
			} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
				cerr << "LuaException:" << e.what () << endl;
#endif
				PBD::warning << "LuaException: " << e.what () << endmsg;
			} catch (...) { }
			for (int i = 0; i < MAX_LUA_ACTION_SCRIPTS; ++i) {
				std::string name;
				if (lua_action_name (i, name)) {
					ActionChanged (i, name); /* EMIT SIGNAL */
				}
			}
			g_free (buf);
		}
	}

	assert (_callbacks.empty());
	if ((child = find_named_node (node, "ActionHooks"))) {
		for (XMLNodeList::const_iterator n = child->children ().begin (); n != child->children ().end (); ++n) {
			try {
				LuaCallbackPtr p (new LuaCallback (_session, *(*n)));
				_callbacks.insert (std::make_pair(p->id(), p));
				p->drop_callback.connect (_slotcon, MISSING_INVALIDATOR, boost::bind (&LuaInstance::unregister_lua_slot, this, p->id()), gui_context());
				SlotChanged (p->id(), p->name(), p->signals()); /* EMIT SIGNAL */
			} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
				cerr << "LuaException:" << e.what () << endl;
#endif
				PBD::warning << "LuaException: " << e.what () << endmsg;
			} catch (...) { }
		}
	}

	return 0;
}

void
LuaInstance::pre_seed_script (std::string const& name, int& id)
{
	LuaScriptInfoPtr spi = LuaScripting::instance ().by_name (name, LuaScriptInfo::EditorAction);
	if (spi) {
		try {
			std::string script = Glib::file_get_contents (spi->path);
			LuaState ls;
			register_classes (ls.getState ());
			LuaScriptParamList lsp = LuaScriptParams::script_params (ls, spi->path, "action_params");
			LuaScriptParamPtr lspp (new LuaScriptParam("x-script-origin", "", spi->path, false, true));
			lsp.push_back (lspp);
			set_lua_action (id++, name, script, lsp);
		} catch (...) { }
	}
}

void
LuaInstance::pre_seed_scripts ()
{
	int id = 0;
	pre_seed_script ("Mixer Screenshot", id);
	pre_seed_script ("List Plugins", id);
}

bool
LuaInstance::interactive_add (LuaScriptInfo::ScriptType type, int id)
{
	std::string title;
	std::string param_function = "action_params";
	std::vector<std::string> reg;

	switch (type) {
		case LuaScriptInfo::EditorAction:
			reg = lua_action_names ();
			title = _("Add Shortcut or Lua Script");
			break;
		case LuaScriptInfo::EditorHook:
			reg = lua_slot_names ();
			title = _("Add Lua Callback Hook");
			break;
		case LuaScriptInfo::Session:
			if (!_session) {
				return false;
			}
			reg = _session->registered_lua_functions ();
			title = _("Add Lua Session Script");
			param_function = "sess_params";
			break;
		default:
			return false;
	}

	LuaScriptInfoPtr spi;
	ScriptSelector ss (title, type);
	switch (ss.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			spi = ss.script();
			break;
		default:
			return false;
	}
	ss.hide ();

	std::string script = "";

	try {
		script = Glib::file_get_contents (spi->path);
	} catch (Glib::FileError const& e) {
		string msg = string_compose (_("Cannot read script '%1': %2"), spi->path, e.what());
		Gtk::MessageDialog am (msg);
		am.run ();
		return false;
	}

	LuaState ls;
	register_classes (ls.getState ());
	LuaScriptParamList lsp = LuaScriptParams::script_params (ls, spi->path, param_function);

	/* allow cancel */
	for (size_t i = 0; i < lsp.size(); ++i) {
		if (lsp[i]->preseeded && lsp[i]->name == "x-script-abort") {
			return false;
		}
	}

	ScriptParameterDialog spd (_("Set Script Parameters"), spi, reg, lsp);

	if (spd.need_interation ()) {
		switch (spd.run ()) {
			case Gtk::RESPONSE_ACCEPT:
				break;
			default:
				return false;
		}
	}

	LuaScriptParamPtr lspp (new LuaScriptParam("x-script-origin", "", spi->path, false, true));
	lsp.push_back (lspp);

	switch (type) {
		case LuaScriptInfo::EditorAction:
			return set_lua_action (id, spd.name(), script, lsp);
			break;
		case LuaScriptInfo::EditorHook:
			return register_lua_slot (spd.name(), script, lsp);
			break;
		case LuaScriptInfo::Session:
			try {
				_session->register_lua_function (spd.name(), script, lsp);
			} catch (luabridge::LuaException const& e) {
				string msg = string_compose (_("Session script '%1' instantiation failed: %2"), spd.name(), e.what ());
				Gtk::MessageDialog am (msg);
				am.run ();
			} catch (SessionException const& e) {
				string msg = string_compose (_("Loading Session script '%1' failed: %2"), spd.name(), e.what ());
				Gtk::MessageDialog am (msg);
				am.run ();
			} catch (...) {
				string msg = string_compose (_("Loading Session script '%1' failed: %2"), spd.name(), "Unknown Exception");
				Gtk::MessageDialog am (msg);
				am.run ();
			}
		default:
			break;
	}
	return false;
}

XMLNode&
LuaInstance::get_action_state ()
{
	std::string saved;
	{
		luabridge::LuaRef savedstate ((*_lua_save)());
		saved = savedstate.cast<std::string>();
	}
	lua.collect_garbage ();

	gchar* b64 = g_base64_encode ((const guchar*)saved.c_str (), saved.size ());
	std::string b64s (b64);
	g_free (b64);

	XMLNode* script_node = new XMLNode (X_("ActionScript"));
	script_node->set_property (X_("lua"), LUA_VERSION);
	script_node->add_content (b64s);

	return *script_node;
}

XMLNode&
LuaInstance::get_hook_state ()
{
	XMLNode* script_node = new XMLNode (X_("ActionHooks"));
	for (LuaCallbackMap::const_iterator i = _callbacks.begin(); i != _callbacks.end(); ++i) {
		script_node->add_child_nocopy (i->second->get_state ());
	}
	return *script_node;
}

void
LuaInstance::call_action (const int id)
{
	try {
		(*_lua_call_action)(id + 1);
		lua.collect_garbage_step ();
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
}

void
LuaInstance::render_action_icon (cairo_t* cr, int w, int h, uint32_t c, void* i) {
	int ii = reinterpret_cast<uintptr_t> (i);
	instance()->render_icon (ii, cr, w, h, c);
}

void
LuaInstance::render_icon (int i, cairo_t* cr, int w, int h, uint32_t clr)
{
	 Cairo::Context ctx (cr);
	 try {
		 (*_lua_render_icon)(i + 1, (Cairo::Context *)&ctx, w, h, clr);
	 } catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		 cerr << "LuaException:" << e.what () << endl;
#endif
		 PBD::warning << "LuaException: " << e.what () << endmsg;
	 } catch (...) { }
}

bool
LuaInstance::set_lua_action (
		const int id,
		const std::string& name,
		const std::string& script,
		const LuaScriptParamList& args)
{
	try {
		lua_State* L = lua.getState();
		// get bytcode of factory-function in a sandbox
		// (don't allow scripts to interfere)
		const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
		const std::string& iconfunc = LuaScripting::get_factory_bytecode (script, "icon", "icn");
		luabridge::LuaRef tbl_arg (luabridge::newTable(L));
		for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
			if ((*i)->optional && !(*i)->is_set) { continue; }
			tbl_arg[(*i)->name] = (*i)->value;
		}
		(*_lua_add_action)(id + 1, name, script, bytecode, iconfunc, tbl_arg);
		ActionChanged (id, name); /* EMIT SIGNAL */
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
		return false;
	} catch (...) {
		return false;
	}
	set_dirty ();
	return true;
}

bool
LuaInstance::remove_lua_action (const int id)
{
	try {
		(*_lua_del_action)(id + 1);
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
		return false;
	} catch (...) {
		return false;
	}
	ActionChanged (id, ""); /* EMIT SIGNAL */
	set_dirty ();
	return true;
}

bool
LuaInstance::lua_action_name (const int id, std::string& rv)
{
	try {
		luabridge::LuaRef ref ((*_lua_get_action)(id + 1));
		if (ref.isNil()) {
			return false;
		}
		if (ref["name"].isString()) {
			rv = ref["name"].cast<std::string>();
			return true;
		}
		return true;
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
	return false;
}

std::vector<std::string>
LuaInstance::lua_action_names ()
{
	std::vector<std::string> rv;
	for (int i = 0; i < MAX_LUA_ACTION_SCRIPTS; ++i) {
		std::string name;
		if (lua_action_name (i, name)) {
			rv.push_back (name);
		}
	}
	return rv;
}

bool
LuaInstance::lua_action_has_icon (const int id)
{
	try {
		luabridge::LuaRef ref ((*_lua_get_action)(id + 1));
		if (ref.isNil()) {
			return false;
		}
		if (ref["icon"].isBoolean()) {
			return ref["icon"].cast<bool>();
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
	return false;
}

bool
LuaInstance::lua_action (const int id, std::string& name, std::string& script, LuaScriptParamList& args)
{
	try {
		luabridge::LuaRef ref ((*_lua_get_action)(id + 1));
		if (ref.isNil()) {
			return false;
		}
		if (!ref["name"].isString()) {
			return false;
		}
		if (!ref["script"].isString()) {
			return false;
		}
		if (!ref["args"].isTable()) {
			return false;
		}
		name = ref["name"].cast<std::string>();
		script = ref["script"].cast<std::string>();

		args.clear();
		LuaScriptInfoPtr lsi = LuaScripting::script_info (script);
		if (!lsi) {
			return false;
		}
		args = LuaScriptParams::script_params (lsi, "action_params");
		luabridge::LuaRef rargs (ref["args"]);
		LuaScriptParams::ref_to_params (args, &rargs);
		return true;
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
	return false;
}

bool
LuaInstance::register_lua_slot (const std::string& name, const std::string& script, const ARDOUR::LuaScriptParamList& args)
{
	/* parse script, get ActionHook(s) from script */
	ActionHook ah;
	try {
		LuaState l;
		l.Print.connect (&_lua_print);
		l.sandbox (true);
		lua_State* L = l.getState();
		register_hooks (L);
		l.do_command ("function ardour () end");
		l.do_command (script);
		luabridge::LuaRef signals = luabridge::getGlobal (L, "signals");
		if (signals.isFunction()) {
			ah = signals();
		}
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }

	if (ah.none ()) {
		cerr << "Script registered no hooks." << endl;
		return false;
	}

	/* register script w/args, get entry-point / ID */

	try {
		LuaCallbackPtr p (new LuaCallback (_session, name, script, ah, args));
		_callbacks.insert (std::make_pair(p->id(), p));
		p->drop_callback.connect (_slotcon, MISSING_INVALIDATOR, boost::bind (&LuaInstance::unregister_lua_slot, this, p->id()), gui_context());
		SlotChanged (p->id(), p->name(), p->signals()); /* EMIT SIGNAL */
		set_dirty ();
		return true;
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
	return false;
}

bool
LuaInstance::unregister_lua_slot (const PBD::ID& id)
{
	LuaCallbackMap::iterator i = _callbacks.find (id);
	if (i != _callbacks.end()) {
		SlotChanged (id, "", ActionHook()); /* EMIT SIGNAL */
		_callbacks.erase (i);
		set_dirty ();
		return true;
	}
	return false;
}

std::vector<PBD::ID>
LuaInstance::lua_slots () const
{
	std::vector<PBD::ID> rv;
	for (LuaCallbackMap::const_iterator i = _callbacks.begin(); i != _callbacks.end(); ++i) {
		rv.push_back (i->first);
	}
	return rv;
}

bool
LuaInstance::lua_slot_name (const PBD::ID& id, std::string& name) const
{
	LuaCallbackMap::const_iterator i = _callbacks.find (id);
	if (i != _callbacks.end()) {
		name = i->second->name();
		return true;
	}
	return false;
}

std::vector<std::string>
LuaInstance::lua_slot_names () const
{
	std::vector<std::string> rv;
	std::vector<PBD::ID> ids = lua_slots();
	for (std::vector<PBD::ID>::const_iterator i = ids.begin(); i != ids.end(); ++i) {
		std::string name;
		if (lua_slot_name (*i, name)) {
			rv.push_back (name);
		}
	}
	return rv;
}

bool
LuaInstance::lua_slot (const PBD::ID& id, std::string& name, std::string& script, ActionHook& ah, ARDOUR::LuaScriptParamList& args)
{
	LuaCallbackMap::const_iterator i = _callbacks.find (id);
	if (i == _callbacks.end()) {
		return false; // error
	}
	return i->second->lua_slot (name, script, ah, args);
}

///////////////////////////////////////////////////////////////////////////////

LuaCallback::LuaCallback (Session *s,
		const std::string& name,
		const std::string& script,
		const ActionHook& ah,
		const ARDOUR::LuaScriptParamList& args)
	: SessionHandlePtr (s)
	, _id ("0")
	, _name (name)
	, _signals (ah)
{
	// TODO: allow to reference object (e.g region)
	init ();

	lua_State* L = lua.getState();
	luabridge::LuaRef tbl_arg (luabridge::newTable(L));
	for (LuaScriptParamList::const_iterator i = args.begin(); i != args.end(); ++i) {
		if ((*i)->optional && !(*i)->is_set) { continue; }
		tbl_arg[(*i)->name] = (*i)->value;
	}

	try {
		const std::string& bytecode = LuaScripting::get_factory_bytecode (script);
		(*_lua_add)(name, script, bytecode, tbl_arg);
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
		throw failed_constructor ();
	} catch (...) {
		throw failed_constructor ();
	}

	_id.reset ();
	set_session (s);
}

LuaCallback::LuaCallback (Session *s, XMLNode & node)
	: SessionHandlePtr (s)
{
	XMLNode* child = NULL;
	if (node.name() != X_("LuaCallback")
			|| !node.property ("signals")
			|| !node.property ("id")
			|| !node.property ("name")) {
		throw failed_constructor ();
	}

	for (XMLNodeList::const_iterator n = node.children ().begin (); n != node.children ().end (); ++n) {
		if (!(*n)->is_content ()) { continue; }
		child = *n;
	}

	if (!child) {
		throw failed_constructor ();
	}

	init ();

	_id = PBD::ID (node.property ("id")->value ());
	_name = node.property ("name")->value ();
	_signals = ActionHook (node.property ("signals")->value ());

	gsize size;
	guchar* buf = g_base64_decode (child->content ().c_str (), &size);
	try {
		(*_lua_load)(std::string ((const char*)buf, size));
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
	} catch (...) { }
	g_free (buf);

	set_session (s);
}

LuaCallback::~LuaCallback ()
{
	delete _lua_add;
	delete _lua_get;
	delete _lua_call;
	delete _lua_load;
	delete _lua_save;
}

XMLNode&
LuaCallback::get_state (void)
{
	std::string saved;
	{
		luabridge::LuaRef savedstate ((*_lua_save)());
		saved = savedstate.cast<std::string>();
	}

	lua.collect_garbage (); // this may be expensive:
	/* Editor::instant_save() calls Editor::get_state() which
	 * calls LuaInstance::get_hook_state() which in turn calls
	 * this LuaCallback::get_state() for every registered hook.
	 *
	 * serialize in _lua_save() allocates many small strings
	 * on the lua-stack, collecting them all may take a ms.
	 */

	gchar* b64 = g_base64_encode ((const guchar*)saved.c_str (), saved.size ());
	std::string b64s (b64);
	g_free (b64);

	XMLNode* script_node = new XMLNode (X_("LuaCallback"));
	script_node->set_property (X_("lua"), LUA_VERSION);
	script_node->set_property (X_("id"), _id.to_s ());
	script_node->set_property (X_("name"), _name);
	script_node->set_property (X_("signals"), _signals.to_string ());
	script_node->add_content (b64s);
	return *script_node;
}

void
LuaCallback::init (void)
{
	lua.Print.connect (&_lua_print);
	lua.sandbox (false);

	lua.do_command (
			"function ScriptManager ()"
			"  local self = { script = {}, instance = {} }"
			""
			"  local addinternal = function (n, s, f, a)"
			"   assert(type(n) == 'string', 'Name must be string')"
			"   assert(type(s) == 'string', 'Script must be string')"
			"   assert(type(f) == 'function', 'Factory is a not a function')"
			"   assert(type(a) == 'table' or type(a) == 'nil', 'Given argument is invalid')"
			"   self.script = { ['n'] = n, ['s'] = s, ['f'] = f, ['a'] = a }"
			"   local env = _ENV; env.f = nil"
			"   self.instance = load (string.dump(f, true), nil, nil, env)(a)"
			"  end"
			""
			"  local call = function (...)"
			"   if type(self.instance) == 'function' then"
			"     local status, err = pcall (self.instance, ...)"
			"     if not status then"
			"       print ('callback \"'.. self.script['n'] .. '\": ', err)" // error out
			"       self.script = nil"
			"       self.instance = nil"
			"       return false"
			"     end"
			"   end"
			"   collectgarbage()"
			"   return true"
			"  end"
			""
			"  local add = function (n, s, b, a)"
			"   assert(type(b) == 'string', 'ByteCode must be string')"
			"   load (b)()" // assigns f
			"   assert(type(f) == 'string', 'Assigned ByteCode must be string')"
			"   addinternal (n, s, load(f), a)"
			"  end"
			""
			"  local get = function ()"
			"   if type(self.instance) == 'function' and type(self.script['n']) == 'string' then"
			"    return { ['name'] = self.script['n'],"
			"             ['script'] = self.script['s'],"
			"             ['args'] = self.script['a'] }"
			"   end"
			"   return nil"
			"  end"
			""
			// code dup
			""
			"  local function basic_serialize (o)"
			"    if type(o) == \"number\" then"
			"     return tostring(o)"
			"    else"
			"     return string.format(\"%q\", o)"
			"    end"
			"  end"
			""
			"  local function serialize (name, value)"
			"   local rv = name .. ' = '"
			"   if type(value) == \"number\" or type(value) == \"string\" or type(value) == \"nil\" then"
			"    return rv .. basic_serialize(value) .. ' '"
			"   elseif type(value) == \"table\" then"
			"    rv = rv .. '{} '"
			"    for k,v in pairs(value) do"
			"     local fieldname = string.format(\"%s[%s]\", name, basic_serialize(k))"
			"     rv = rv .. serialize(fieldname, v) .. ' '"
			"    end"
			"    return rv;"
			"   elseif type(value) == \"function\" then"
			"     return rv .. string.format(\"%q\", string.dump(value, true))"
			"   elseif type(value) == \"boolean\" then"
			"     return rv .. tostring (value)"
			"   else"
			"    error('cannot save a ' .. type(value))"
			"   end"
			"  end"
			""
			// end code dup
			""
			"  local save = function ()"
			"   return (serialize('s', self.script))"
			"  end"
			""
			"  local restore = function (state)"
			"   self.script = {}"
			"   load (state)()"
			"   addinternal (s['n'], s['s'], load(s['f']), s['a'])"
			"  end"
			""
			" return { call = call, add = add, get = get,"
			"          restore = restore, save = save}"
			" end"
			" "
			" manager = ScriptManager ()"
			" ScriptManager = nil"
			);

	lua_State* L = lua.getState();

	try {
		luabridge::LuaRef lua_mgr = luabridge::getGlobal (L, "manager");
		lua.do_command ("manager = nil"); // hide it.
		lua.do_command ("collectgarbage()");

		_lua_add = new luabridge::LuaRef(lua_mgr["add"]);
		_lua_get = new luabridge::LuaRef(lua_mgr["get"]);
		_lua_call = new luabridge::LuaRef(lua_mgr["call"]);
		_lua_save = new luabridge::LuaRef(lua_mgr["save"]);
		_lua_load = new luabridge::LuaRef(lua_mgr["restore"]);

	} catch (luabridge::LuaException const& e) {
		fatal << string_compose (_("programming error: %1"),
				std::string ("Failed to setup Lua callback interpreter: ") + e.what ())
			<< endmsg;
		abort(); /*NOTREACHED*/
	} catch (...) {
		fatal << string_compose (_("programming error: %1"),
				X_("Failed to setup Lua callback interpreter"))
			<< endmsg;
		abort(); /*NOTREACHED*/
	}

	LuaInstance::register_classes (L);
	LuaInstance::register_hooks (L);

	luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
	lua_setglobal (L, "Editor");
}

bool
LuaCallback::lua_slot (std::string& name, std::string& script, ActionHook& ah, ARDOUR::LuaScriptParamList& args)
{
	// TODO consolidate w/ LuaInstance::lua_action()
	try {
		luabridge::LuaRef ref = (*_lua_get)();
		if (ref.isNil()) {
			return false;
		}
		if (!ref["name"].isString()) {
			return false;
		}
		if (!ref["script"].isString()) {
			return false;
		}
		if (!ref["args"].isTable()) {
			return false;
		}

		ah = _signals;
		name = ref["name"].cast<std::string> ();
		script = ref["script"].cast<std::string> ();

		args.clear();
		LuaScriptInfoPtr lsi = LuaScripting::script_info (script);
		if (!lsi) {
			return false;
		}
		args = LuaScriptParams::script_params (lsi, "action_params");
		luabridge::LuaRef rargs (ref["args"]);
		LuaScriptParams::ref_to_params (args, &rargs);
		return true;
	} catch (luabridge::LuaException const& e) {
#ifndef NDEBUG
		cerr << "LuaException:" << e.what () << endl;
#endif
		PBD::warning << "LuaException: " << e.what () << endmsg;
		return false;
	} catch (...) { }
	return false;
}

void
LuaCallback::set_session (ARDOUR::Session *s)
{
	SessionHandlePtr::set_session (s);

	if (!_session) {
		return;
	}

	lua_State* L = lua.getState();
	LuaBindings::set_session (L, _session);

	reconnect();
}

void
LuaCallback::session_going_away ()
{
	ENSURE_GUI_THREAD (*this, &LuaCallback::session_going_away);
	lua.do_command ("collectgarbage();");

	SessionHandlePtr::session_going_away ();
	_session = 0;

	drop_callback (); /* EMIT SIGNAL */
}

void
LuaCallback::reconnect ()
{
	_connections.drop_connections ();
	if ((*_lua_get) ().isNil ()) {
		drop_callback (); /* EMIT SIGNAL */
		return;
	}

	// TODO pass object which emits the signal (e.g region)
	//
	// save/load bound objects will be tricky.
	// Best idea so far is to save/lookup the PBD::ID
	// (either use boost::any indirection or templates for bindable
	// object types or a switch statement..)
	//
	// _session->route_by_id ()
	// _session->track_by_diskstream_id ()
	// _session->source_by_id ()
	// _session->controllable_by_id ()
	// _session->processor_by_id ()
	// RegionFactory::region_by_id ()
	//
	// TODO loop over objects (if any)

	reconnect_object ((void*)0);
}

template <class T> void
LuaCallback::reconnect_object (T obj)
{
	for (uint32_t i = 0; i < LuaSignal::LAST_SIGNAL; ++i) {
		if (_signals[i]) {
#define ENGINE(n,c,p) else if (i == LuaSignal::n) { connect_ ## p (LuaSignal::n, AudioEngine::instance(), &(AudioEngine::instance()->c)); }
#define SESSION(n,c,p) else if (i == LuaSignal::n) { if (_session) { connect_ ## p (LuaSignal::n, _session, &(_session->c)); } }
#define STATIC(n,c,p) else if (i == LuaSignal::n) { connect_ ## p (LuaSignal::n, obj, c); }
			if (0) {}
#			include "luasignal_syms.h"
			else {
				PBD::fatal << string_compose (_("programming error: %1: %2"), "Impossible LuaSignal type", i) << endmsg;
				abort(); /*NOTREACHED*/
			}
#undef ENGINE
#undef SESSION
#undef STATIC
		}
	}
}

template <typename T, typename S> void
LuaCallback::connect_0 (enum LuaSignal::LuaSignal ls, T ref, S *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_0<T>, this, ls, ref),
			gui_context());
}

template <typename T, typename C1> void
LuaCallback::connect_1 (enum LuaSignal::LuaSignal ls, T ref, PBD::Signal1<void, C1> *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_1<T, C1>, this, ls, ref, _1),
			gui_context());
}

template <typename T, typename C1, typename C2> void
LuaCallback::connect_2 (enum LuaSignal::LuaSignal ls, T ref, PBD::Signal2<void, C1, C2> *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_2<T, C1, C2>, this, ls, ref, _1, _2),
			gui_context());
}

template <typename T, typename C1, typename C2, typename C3> void
LuaCallback::connect_3 (enum LuaSignal::LuaSignal ls, T ref, PBD::Signal3<void, C1, C2, C3> *signal) {
	signal->connect (
			_connections, invalidator (*this),
			boost::bind (&LuaCallback::proxy_3<T, C1, C2, C3>, this, ls, ref, _1, _2, _3),
			gui_context());
}

template <typename T> void
LuaCallback::proxy_0 (enum LuaSignal::LuaSignal ls, T ref) {
	bool ok = true;
	{
		const luabridge::LuaRef& rv ((*_lua_call)((int)ls, ref));
		if (! rv.cast<bool> ()) {
			ok = false;
		}
	}
	/* destroy LuaRef ^^ first before calling drop_callback() */
	if (!ok) {
		drop_callback (); /* EMIT SIGNAL */
	}
}

template <typename T, typename C1> void
LuaCallback::proxy_1 (enum LuaSignal::LuaSignal ls, T ref, C1 a1) {
	bool ok = true;
	{
		const luabridge::LuaRef& rv ((*_lua_call)((int)ls, ref, a1));
		if (! rv.cast<bool> ()) {
			ok = false;
		}
	}
	if (!ok) {
		drop_callback (); /* EMIT SIGNAL */
	}
}

template <typename T, typename C1, typename C2> void
LuaCallback::proxy_2 (enum LuaSignal::LuaSignal ls, T ref, C1 a1, C2 a2) {
	bool ok = true;
	{
		const luabridge::LuaRef& rv ((*_lua_call)((int)ls, ref, a1, a2));
		if (! rv.cast<bool> ()) {
			ok = false;
		}
	}
	if (!ok) {
		drop_callback (); /* EMIT SIGNAL */
	}
}

template <typename T, typename C1, typename C2, typename C3> void
LuaCallback::proxy_3 (enum LuaSignal::LuaSignal ls, T ref, C1 a1, C2 a2, C3 a3) {
	bool ok = true;
	{
		const luabridge::LuaRef& rv ((*_lua_call)((int)ls, ref, a1, a2, a3));
		if (! rv.cast<bool> ()) {
			ok = false;
		}
	}
	if (!ok) {
		drop_callback (); /* EMIT SIGNAL */
	}
}
