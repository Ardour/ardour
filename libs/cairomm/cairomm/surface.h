/* Copyright (C) 2005 The cairomm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __CAIROMM_SURFACE_H
#define __CAIROMM_SURFACE_H

#include <string>
#include <vector>
#include <cairomm/enums.h>
#include <cairomm/exception.h>
#include <cairomm/fontoptions.h>
#include <cairomm/refptr.h>

//See xlib_surface.h for XlibSurface.
//See win32_surface.h for Win32Surface.

#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif // CAIRO_HAS_PDF_SURFACE
#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif // CAIRO_HAS_PS_SURFACE
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif // CAIRO_HAS_SVG_SURFACE

// Experimental surfaces
#ifdef CAIRO_HAS_GLITZ_SURFACE
#include <cairo-glitz.h>
#endif // CAIRO_HAS_GLITZ_SURFACE


namespace Cairo
{

/** A cairo surface represents an image, either as the destination of a drawing
 * operation or as source when drawing onto another surface. There are
 * different subtypes of cairo surface for different drawing backends.  This
 * class is a base class for all subtypes and should not be used directly
 *
 * Surfaces are reference-counted objects that should be used via Cairo::RefPtr. 
 */
class Surface
{
public:
  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit Surface(cairo_surface_t* cobject, bool has_reference = false);

  virtual ~Surface();

  /** Retrieves the default font rendering options for the surface. This allows
   * display surfaces to report the correct subpixel order for rendering on
   * them, print surfaces to disable hinting of metrics and so forth. The
   * result can then be used with cairo_scaled_font_create().
   *
   * @param options 	a FontOptions object into which to store the retrieved
   * options. All existing values are overwritten
   */
  void get_font_options(FontOptions& options) const;

  /** This function finishes the surface and drops all references to external
   * resources. For example, for the Xlib backend it means that cairo will no
   * longer access the drawable, which can be freed. After calling
   * finish() the only valid operations on a surface are getting and setting
   * user data and referencing and destroying it. Further drawing to the
   * surface will not affect the surface but will instead trigger a
   * CAIRO_STATUS_SURFACE_FINISHED error.
   *
   * When the Surface is destroyed, cairo will call finish() if it hasn't been
   * called already, before freeing the resources associated with the Surface.
   */
  void finish();

  /** Do any pending drawing for the surface and also restore any temporary
   * modifications cairo has made to the surface's state. This function must
   * be called before switching from drawing on the surface with cairo to
   * drawing on it directly with native APIs. If the surface doesn't support
   * direct access, then this function does nothing.
   */
  void flush();

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
  void mark_dirty();

  /** Marks a rectangular area of the given surface dirty.
   *
   * @param x 	 X coordinate of dirty rectangle
   * @param y 	Y coordinate of dirty rectangle
   * @param width 	width of dirty rectangle
   * @param height 	height of dirty rectangle
   */
  void mark_dirty(int x, int y, int width, int height);

  /** Sets an offset that is added to the device coordinates determined by the
   * CTM when drawing to surface. One use case for this function is when we
   * want to create a Surface that redirects drawing for a portion of
   * an onscreen surface to an offscreen surface in a way that is completely
   * invisible to the user of the cairo API. Setting a transformation via
   * cairo_translate() isn't sufficient to do this, since functions like
   * Cairo::Context::device_to_user() will expose the hidden offset.
   *
   * Note that the offset only affects drawing to the surface, not using the
   * surface in a surface pattern.
   *
   * @param x_offset 	the offset in the X direction, in device units
   * @param y_offset 	the offset in the Y direction, in device units
   */
  void set_device_offset(double x_offset, double y_offset);

  /** Returns a previous device offset set by set_device_offset().
   */
  void get_device_offset(double& x_offset, double& y_offset) const;

  /** Sets the fallback resolution of the image in dots per inch
   *
   * @param x_pixels_per_inch   Pixels per inch in the x direction
   * @param y_pixels_per_inch   Pixels per inch in the y direction
   */
  void set_fallback_resolution(double x_pixels_per_inch, double y_pixels_per_inch);

  SurfaceType get_type() const;

#ifdef CAIRO_HAS_PNG_FUNCTIONS

  /** Writes the contents of surface to a new file filename as a PNG image.
   *
   * @note For this function to be available, cairo must have been compiled
   * with PNG support
   *
   * @param filename	the name of a file to write to
   */
  void write_to_png(const std::string& filename);

  /** Writes the Surface to the write function.
   *
   * @note For this function to be available, cairo must have been compiled
   * with PNG support
   *
   * @param write_func  The function to be called when the backend needs to
   * write data to an output stream
   * @param closure	closure data for the write function
   */
  void write_to_png(cairo_write_func_t write_func, void *closure); //TODO: Use a sigc::slot?

#endif // CAIRO_HAS_PNG_FUNCTIONS


  /** The underlying C cairo surface type
   */
  typedef cairo_surface_t cobject;
  /** Provides acces to the underlying C cairo surface
   */
  inline cobject* cobj() { return m_cobject; }
  /** Provides acces to the underlying C cairo surface
   */
  inline const cobject* cobj() const { return m_cobject; }

  #ifndef DOXYGEN_IGNORE_THIS
  ///For use only by the cairomm implementation.
  inline ErrorStatus get_status() const
  { return cairo_surface_status(const_cast<cairo_surface_t*>(cobj())); }

  void reference() const;
  void unreference() const;
  #endif //DOXYGEN_IGNORE_THIS

  /** Create a new surface that is as compatible as possible with an existing
   * surface. The new surface will use the same backend as other unless that is
   * not possible for some reason.
   *
   * @param other 	an existing surface used to select the backend of the new surface
   * @param content 	the content for the new surface
   * @param width 	width of the new surface, (in device-space units)
   * @param height 	height of the new surface (in device-space units)
   * @return 	a RefPtr to the newly allocated surface.
   */
  static RefPtr<Surface> create(const RefPtr<Surface> other, Content content, int width, int height);

protected:
  /** The underlying C cairo surface type that is wrapped by this Surface
   */
  cobject* m_cobject;
};


/** Image surfaces provide the ability to render to memory buffers either
 * allocated by cairo or by the calling code. The supported image formats are
 * those defined in Cairo::Format
 *
 * An ImageSurface is the most generic type of Surface and the only one that is
 * available by default.  You can either create an ImageSurface whose data is
 * managed by Cairo, or you can create an ImageSurface with a data buffer that
 * you allocated yourself so that you can have full access to the data.  
 *
 * When you create an ImageSurface with your own data buffer, you are free to
 * examine the results at any point and do whatever you want with it.  Note that
 * if you modify anything and later want to continue to draw to the surface
 * with cairo, you must let cairo know via Cairo::Surface::mark_dirty() 
 *
 * Note that like all surfaces, an ImageSurface is a reference-counted object that should be used via Cairo::RefPtr.
 */
class ImageSurface : public Surface
{
protected:
  //TODO?: Surface(cairo_surface_t *target);

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   * @param cobject The C instance.
   * @param has_reference Whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit ImageSurface(cairo_surface_t* cobject, bool has_reference = false);

  virtual ~ImageSurface();

  /** Gets the width of the ImageSurface in pixels
   */
  int get_width() const;

  /** Gets the height of the ImageSurface in pixels
   */
  int get_height() const;

  /**
   * Get a pointer to the data of the image surface, for direct
   * inspection or modification.
   *
   * Return value: a pointer to the image data of this surface or NULL
   * if @surface is not an image surface.
   */
  unsigned char* get_data();
  const unsigned char* get_data() const;

  /** gets the format of the surface
   */
  Format get_format() const;

  /**
   * Return value: the stride of the image surface in bytes (or 0 if
   * @surface is not an image surface). The stride is the distance in
   * bytes from the beginning of one row of the image data to the
   * beginning of the next row.
   */
  int get_stride() const;


  /** Creates an image surface of the specified format and dimensions. The
   * initial contents of the surface is undefined; you must explicitely clear
   * the buffer, using, for example, Cairo::Context::rectangle() and
   * Cairo::Context::fill() if you want it cleared.
   *
   * Use this function to create the surface if you don't need access to the
   * internal data and want cairo to manage it for you.  Since you don't have
   * access to the internal data, the resulting surface can only be saved to a
   * PNG image file (if cairo has been compiled with PNG support) or as a
   * source surface (see Cairo::SurfacePattern).
   *
   * @param format 	format of pixels in the surface to create
   * @param width 	width of the surface, in pixels
   * @param height 	height of the surface, in pixels
   * @return 	a RefPtr to the newly created surface.
   */
  static RefPtr<ImageSurface> create(Format format, int width, int height);

  /** Creates an image surface for the provided pixel data. The output buffer
   * must be kept around until the Surface is destroyed or finish() is called
   * on the surface. The initial contents of buffer will be used as the inital
   * image contents; you must explicitely clear the buffer, using, for example,
   * Cairo::Context::rectangle() and Cairo::Context::fill() if you want it
   * cleared.
   *
   * If you want to be able to manually manipulate or extract the data after
   * drawing to the surface with Cairo, you should use this function to create
   * the Surface.  Since you own the internal data, you can do anything you
   * want with it.
   *
   * @param data	a pointer to a buffer supplied by the application in which
   * to write contents.
   * @param format	the format of pixels in the buffer
   * @param width	the width of the image to be stored in the buffer
   * @param height	the height of the image to be stored in the buffer
   * @param stride	the number of bytes between the start of rows in the
   * buffer. Having this be specified separate from width allows for padding at
   * the end of rows, or for writing to a subportion of a larger image.
   * @return	a RefPtr to the newly created surface.
   */
  static RefPtr<ImageSurface> create(unsigned char* data, Format format, int width, int height, int stride);

#ifdef CAIRO_HAS_PNG_FUNCTIONS

  /** Creates a new image surface and initializes the contents to the given PNG
   * file.  
   *
   * @note For this function to be available, cairo must have been compiled
   * with PNG support.
   *
   * @param filename	name of PNG file to load
   * @return	a RefPtr to the new cairo_surface_t initialized with the
   * contents of the PNG image file.
   */
  static RefPtr<ImageSurface> create_from_png(std::string filename);

  /** Creates a new image surface from PNG data read incrementally via the
   * read_func function.  
   *
   * @note For this function to be available, cairo must have been compiled
   * with PNG support.
   *
   * @param read_func	function called to read the data of the file
   * @param closure	data to pass to read_func.
   * @return	a RefPtr to the new cairo_surface_t initialized with the
   * contents of the PNG image file.
   */
  static RefPtr<ImageSurface> create_from_png(cairo_read_func_t read_func, void *closure);

#endif // CAIRO_HAS_PNG_FUNCTIONS

};


#ifdef CAIRO_HAS_PDF_SURFACE

/** A PdfSurface provides a way to render PDF documents from cairo.  This
 * surface is not rendered to the screen but instead renders the drawing to a
 * PDF file on disk.
 *
 * @note For this Surface to be available, cairo must have been compiled with
 * PDF support
 */
class PdfSurface : public Surface
{
public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit PdfSurface(cairo_surface_t* cobject, bool has_reference = false);
  virtual ~PdfSurface();

  /** Creates a PdfSurface with a specified dimensions that will be saved as
   * the given filename
   *
   * @param filename    The name of the PDF file to save the surface to
   * @param width_in_points   The width of the PDF document in points
   * @param height_in_points   The height of the PDF document in points
   */
  static RefPtr<PdfSurface> create(std::string filename, double width_in_points, double height_in_points);

  /** Creates a PdfSurface with a specified dimensions that will be written to
   * the given write function instead of saved directly to disk
   *
   * @param write_func  The function to be called when the backend needs to
   * write data to an output stream
   * @param closure     closure data for the write function
   * @param width_in_points   The width of the PDF document in points
   * @param height_in_points   The height of the PDF document in points
   */
  static RefPtr<PdfSurface> create(cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points);

/**
 * Changes the size of a PDF surface for the current (and subsequent) pages.
 *
 * This function should only be called before any drawing operations have been
 * performed on the current page. The simplest way to do this is to call this
 * function immediately after creating the surface or immediately after
 * completing a page with either Context::show_page() or Context::copy_page().
 *
 * @param width_in_points new surface width, in points (1 point == 1/72.0 inch)
 * @param height_in_points new surface height, in points (1 point == 1/72.0 inch)
 **/
  void set_size(double width_in_points, double height_in_points);

};

#endif  // CAIRO_HAS_PDF_SURFACE


#ifdef CAIRO_HAS_PS_SURFACE

/** A PsSurface provides a way to render PostScript documents from cairo.  This
 * surface is not rendered to the screen but instead renders the drawing to a
 * PostScript file on disk.
 *
 * @note For this Surface to be available, cairo must have been compiled with
 * PostScript support
 */
class PsSurface : public Surface
{
public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit PsSurface(cairo_surface_t* cobject, bool has_reference = false);
  virtual ~PsSurface();

  /** Creates a PsSurface with a specified dimensions that will be saved as the
   * given filename
   *
   * @param filename    The name of the PostScript file to save the surface to
   * @param width_in_points   The width of the PostScript document in points
   * @param height_in_points   The height of the PostScript document in points
   */
  static RefPtr<PsSurface> create(std::string filename, double width_in_points, double height_in_points);

  /** Creates a PsSurface with a specified dimensions that will be written to
   * the given write function instead of saved directly to disk
   *
   * @param write_func  The function to be called when the backend needs to
   * write data to an output stream
   * @param closure     closure data for the write function
   * @param width_in_points   The width of the PostScript document in points
   * @param height_in_points   The height of the PostScript document in points
   */
  static RefPtr<PsSurface> create(cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points);

  /**
   * Changes the size of a PostScript surface for the current (and
   * subsequent) pages.
   *
   * This function should only be called before any drawing operations have been
   * performed on the current page. The simplest way to do this is to call this
   * function immediately after creating the surface or immediately after
   * completing a page with either Context::show_page() or Context::copy_page().
   *
   * @param width_in_points new surface width, in points (1 point == 1/72.0 inch)
   * @param height_in_points new surface height, in points (1 point == 1/72.0 inch)
   */
  void set_size(double width_in_points, double height_in_points);

  /** Emit a comment into the PostScript output for the given surface.  See the
   * cairo reference documentation for more information.
   *
   * @param comment a comment string to be emitted into the PostScript output
   */
  void dsc_comment(std::string comment);

  /**
   * This function indicates that subsequent calls to dsc_comment() should direct
   * comments to the Setup section of the PostScript output.
   *
   * This function should be called at most once per surface, and must be called
   * before any call to dsc_begin_page_setup() and before any drawing is performed
   * to the surface.
   */
  void dsc_begin_setup();

  /** This function indicates that subsequent calls to dsc_comment() should
   * direct comments to the PageSetup section of the PostScript output.
   *
   * This function call is only needed for the first page of a surface. It
   * should be called after any call to dsc_begin_setup() and before any drawing
   * is performed to the surface.
   */
  void dsc_begin_page_setup();

};

#endif // CAIRO_HAS_PS_SURFACE


#ifdef CAIRO_HAS_SVG_SURFACE

typedef enum
{
  SVG_VERSION_1_1 = CAIRO_SVG_VERSION_1_1,
  SVG_VERSION_1_2 = CAIRO_SVG_VERSION_1_2
} SvgVersion;

/** A SvgSurface provides a way to render Scalable Vector Graphics (SVG) images
 * from cairo.  This surface is not rendered to the screen but instead renders
 * the drawing to an SVG file on disk.
 *
 * @note For this Surface to be available, cairo must have been compiled with
 * SVG support
 */
class SvgSurface : public Surface
{
public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit SvgSurface(cairo_surface_t* cobject, bool has_reference = false);
  virtual ~SvgSurface();


  /** Creates a SvgSurface with a specified dimensions that will be saved as the
   * given filename
   *
   * @param filename    The name of the SVG file to save the surface to
   * @param width_in_points   The width of the SVG document in points
   * @param height_in_points   The height of the SVG document in points
   */
  static RefPtr<SvgSurface> create(std::string filename, double width_in_points, double height_in_points);

  /** Creates a SvgSurface with a specified dimensions that will be written to
   * the given write function instead of saved directly to disk
   *
   * @param write_func  The function to be called when the backend needs to
   * write data to an output stream
   * @param closure     closure data for the write function
   * @param width_in_points   The width of the SVG document in points
   * @param height_in_points   The height of the SVG document in points
   */
  static RefPtr<SvgSurface> create(cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points);

  /** 
   * Restricts the generated SVG file to the given version. See get_versions()
   * for a list of available version values that can be used here.
   *
   * This function should only be called before any drawing operations have been
   * performed on the given surface. The simplest way to do this is to call this
   * function immediately after creating the surface.
   *
   * @since 1.2
   */
  void restrict_to_version(SvgVersion version);

  /** Retrieves the list of SVG versions supported by cairo. See
   * restrict_to_version().
   * 
   * @since 1.2
   */
  static const std::vector<SvgVersion> get_versions();

  /** Get the string representation of the given version id. The returned string
   * will be empty if version isn't valid. See get_versions() for a way to get
   * the list of valid version ids.
   *
   * Since: 1.2
   */
  static std::string version_to_string(SvgVersion version);
};

#endif // CAIRO_HAS_SVG_SURFACE


/*******************************************************************************
 * THE FOLLOWING SURFACE TYPES ARE EXPERIMENTAL AND NOT FULLY SUPPORTED
 ******************************************************************************/

#ifdef CAIRO_HAS_GLITZ_SURFACE

/** A GlitzSurface provides a way to render to the X Window System using Glitz.
 * This provides a way to use OpenGL-accelerated graphics from cairo.  If you
 * want to use hardware-accelerated graphics within the X Window system, you
 * should use this Surface type.
 *
 * @note For this Surface to be available, cairo must have been compiled with
 * Glitz support
 *
 * @warning This is an experimental surface.  It is not yet marked as a fully
 * supported surface by the cairo library
 */
class GlitzSurface : public Surface
{

public:

  /** Create a C++ wrapper for the C instance. This C++ instance should then be
   * given to a RefPtr.
   *
   * @param cobject The C instance.
   * @param has_reference whether we already have a reference. Otherwise, the
   * constructor will take an extra reference.
   */
  explicit GlitzSurface(cairo_surface_t* cobject, bool has_reference = false);

  virtual ~GlitzSurface();

  /** Creates a new GlitzSurface
   *
   * @param surface  a glitz surface type
   */
  static RefPtr<GlitzSurface> create(glitz_surface_t *surface);

};

#endif // CAIRO_HAS_GLITZ_SURFACE

} // namespace Cairo

#endif //__CAIROMM_SURFACE_H

// vim: ts=2 sw=2 et
