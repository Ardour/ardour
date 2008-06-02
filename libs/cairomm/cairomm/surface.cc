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

#include <cairomm/surface.h>
#include <cairomm/private.h>

namespace Cairo
{

Surface::Surface(cairo_surface_t* cobject, bool has_reference)
: m_cobject(0)
{
  if(has_reference)
    m_cobject = cobject;
  else
    m_cobject = cairo_surface_reference(cobject);
}

Surface::~Surface()
{
  if(m_cobject)
    cairo_surface_destroy(m_cobject);
}

void Surface::finish()
{
  cairo_surface_finish(m_cobject);
  check_object_status_and_throw_exception(*this);
}

void Surface::get_font_options(FontOptions& options) const
{
  cairo_font_options_t* cfontoptions = cairo_font_options_create();
  cairo_surface_get_font_options(m_cobject, cfontoptions);
  options = FontOptions(cfontoptions);
  cairo_font_options_destroy(cfontoptions);
  check_object_status_and_throw_exception(*this);
}

void Surface::flush()
{
  cairo_surface_flush(m_cobject);
  check_object_status_and_throw_exception(*this);
}

void Surface::mark_dirty()
{
  cairo_surface_mark_dirty(m_cobject);
  check_object_status_and_throw_exception(*this);
}

void Surface::mark_dirty(int x, int y, int width, int height)
{
  cairo_surface_mark_dirty_rectangle(m_cobject, x, y, width, height);
  check_object_status_and_throw_exception(*this);
}

void Surface::set_device_offset(double x_offset, double y_offset)
{
  cairo_surface_set_device_offset(m_cobject, x_offset, y_offset);
  check_object_status_and_throw_exception(*this);
}

void Surface::get_device_offset(double& x_offset, double& y_offset) const
{
  cairo_surface_get_device_offset(const_cast<cobject*>(m_cobject), &x_offset, &y_offset);
}

void Surface::set_fallback_resolution(double x_pixels_per_inch, double y_pixels_per_inch)
{
  cairo_surface_set_fallback_resolution(m_cobject, x_pixels_per_inch, y_pixels_per_inch);
  check_object_status_and_throw_exception(*this);
}

SurfaceType Surface::get_type() const
{
  cairo_surface_type_t surface_type = cairo_surface_get_type(m_cobject);
  check_object_status_and_throw_exception(*this);
  return static_cast<SurfaceType>(surface_type);
}

#ifdef CAIRO_HAS_PNG_FUNCTIONS
void Surface::write_to_png(const std::string& filename)
{
  ErrorStatus status = cairo_surface_write_to_png(m_cobject, filename.c_str());
  check_status_and_throw_exception(status);
}

void Surface::write_to_png(cairo_write_func_t write_func, void *closure)
{
  ErrorStatus status = cairo_surface_write_to_png_stream(m_cobject, write_func, closure);
  check_status_and_throw_exception(status);
}
#endif

void Surface::reference() const
{
  cairo_surface_reference(m_cobject);
}

void Surface::unreference() const
{
  cairo_surface_destroy(m_cobject);
}

RefPtr<Surface> Surface::create(const RefPtr<Surface> other, Content content, int width, int height)
{
  cairo_surface_t* cobject = cairo_surface_create_similar(other->m_cobject, (cairo_content_t)content, width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<Surface>(new Surface(cobject, true /* has reference */));
}



ImageSurface::ImageSurface(cairo_surface_t* cobject, bool has_reference)
: Surface(cobject, has_reference)
{ }

ImageSurface::~ImageSurface()
{
  // surface is destroyed in base class
}

RefPtr<ImageSurface> ImageSurface::create(Format format, int width, int height)
{
  cairo_surface_t* cobject = cairo_image_surface_create((cairo_format_t)format, width, height);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<ImageSurface>(new ImageSurface(cobject, true /* has reference */));
}

RefPtr<ImageSurface> ImageSurface::create(unsigned char* data, Format format, int width, int height, int stride)
{
  cairo_surface_t* cobject = cairo_image_surface_create_for_data(data, (cairo_format_t)format, width, height, stride);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<ImageSurface>(new ImageSurface(cobject, true /* has reference */));
}

#ifdef CAIRO_HAS_PNG_FUNCTIONS

RefPtr<ImageSurface> ImageSurface::create_from_png(std::string filename)
{
  cairo_surface_t* cobject = cairo_image_surface_create_from_png(filename.c_str());
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<ImageSurface>(new ImageSurface(cobject, true /* has reference */));
}

RefPtr<ImageSurface> ImageSurface::create_from_png(cairo_read_func_t read_func, void *closure)
{
  cairo_surface_t* cobject = cairo_image_surface_create_from_png_stream(read_func, closure);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<ImageSurface>(new ImageSurface(cobject, true /* has reference */));
}

#endif // CAIRO_HAS_PNG_FUNCTIONS

int ImageSurface::get_width() const
{
  const int result = cairo_image_surface_get_width(m_cobject);
  check_object_status_and_throw_exception(*this);
  return result;
}

int ImageSurface::get_height() const
{
  const int result = cairo_image_surface_get_height(m_cobject);
  check_object_status_and_throw_exception(*this);
  return result;
}

unsigned char* ImageSurface::get_data()
{
  return cairo_image_surface_get_data(m_cobject);
}

const unsigned char* ImageSurface::get_data() const
{
  return cairo_image_surface_get_data(m_cobject);
}

Format ImageSurface::get_format() const
{
  return static_cast<Format>(cairo_image_surface_get_format(m_cobject));
}

int ImageSurface::get_stride() const
{
  return cairo_image_surface_get_stride(m_cobject);
}


/*******************************************************************************
 * THE FOLLOWING SURFACE TYPES ARE EXPERIMENTAL AND NOT FULLY SUPPORTED
 ******************************************************************************/

#ifdef CAIRO_HAS_PDF_SURFACE

PdfSurface::PdfSurface(cairo_surface_t* cobject, bool has_reference) :
    Surface(cobject, has_reference)
{}

PdfSurface::~PdfSurface()
{
  // surface is destroyed in base class
}

RefPtr<PdfSurface> PdfSurface::create(std::string filename, double width_in_points, double height_in_points)
{
  cairo_surface_t* cobject = cairo_pdf_surface_create(filename.c_str(), width_in_points, height_in_points);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<PdfSurface>(new PdfSurface(cobject, true /* has reference */));
}

RefPtr<PdfSurface> PdfSurface::create(cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points)
{
  cairo_surface_t* cobject = cairo_pdf_surface_create_for_stream(write_func, closure, width_in_points, height_in_points);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<PdfSurface>(new PdfSurface(cobject, true /* has reference */));
}

void PdfSurface::set_size(double width_in_points, double height_in_points)
{
  cairo_pdf_surface_set_size(m_cobject, width_in_points, height_in_points);
  check_object_status_and_throw_exception(*this);
}

#endif // CAIRO_HAS_PDF_SURFACE




#ifdef CAIRO_HAS_PS_SURFACE

PsSurface::PsSurface(cairo_surface_t* cobject, bool has_reference) :
    Surface(cobject, has_reference)
{}

PsSurface::~PsSurface()
{
  // surface is destroyed in base class
}

RefPtr<PsSurface> PsSurface::create(std::string filename, double width_in_points, double height_in_points)
{
  cairo_surface_t* cobject = cairo_ps_surface_create(filename.c_str(), width_in_points, height_in_points);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<PsSurface>(new PsSurface(cobject, true /* has reference */));
}

RefPtr<PsSurface> PsSurface::create(cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points)
{
  cairo_surface_t* cobject = cairo_ps_surface_create_for_stream(write_func, closure, width_in_points, height_in_points);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<PsSurface>(new PsSurface(cobject, true /* has reference */));
}

void PsSurface::set_size(double width_in_points, double height_in_points)
{
  cairo_ps_surface_set_size(m_cobject, width_in_points, height_in_points);
  check_object_status_and_throw_exception(*this);
}


void PsSurface::dsc_comment(std::string comment)
{
  cairo_ps_surface_dsc_comment(m_cobject, comment.c_str());
  check_object_status_and_throw_exception(*this);
}

void PsSurface::dsc_begin_setup()
{
  cairo_ps_surface_dsc_begin_setup(m_cobject);
  check_object_status_and_throw_exception(*this);
}

void PsSurface::dsc_begin_page_setup()
{
  cairo_ps_surface_dsc_begin_page_setup(m_cobject);
  check_object_status_and_throw_exception(*this);
}


#endif // CAIRO_HAS_PS_SURFACE




#ifdef CAIRO_HAS_SVG_SURFACE

SvgSurface::SvgSurface(cairo_surface_t* cobject, bool has_reference) :
    Surface(cobject, has_reference)
{}

SvgSurface::~SvgSurface()
{
  // surface is destroyed in base class
}

RefPtr<SvgSurface> SvgSurface::create(std::string filename, double width_in_points, double height_in_points)
{
  cairo_surface_t* cobject = cairo_svg_surface_create(filename.c_str(), width_in_points, height_in_points);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<SvgSurface>(new SvgSurface(cobject, true /* has reference */));
}

RefPtr<SvgSurface> SvgSurface::create(cairo_write_func_t write_func, void *closure, double width_in_points, double height_in_points)
{
  cairo_surface_t* cobject = cairo_svg_surface_create_for_stream(write_func, closure, width_in_points, height_in_points);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<SvgSurface>(new SvgSurface(cobject, true /* has reference */));
}

void SvgSurface::restrict_to_version(SvgVersion version)
{
  cairo_svg_surface_restrict_to_version(m_cobject, static_cast<cairo_svg_version_t>(version));
  check_object_status_and_throw_exception(*this);
}

const std::vector<SvgVersion> SvgSurface::get_versions()
{
  cairo_svg_version_t const *versions;
  int num_versions;
  cairo_svg_get_versions(&versions, &num_versions);

  // Just copy the version array out into a std::vector.  This is a rarely used
  // function and the array of versions is going to be very small, so there's no
  // real performance hit.
  std::vector<SvgVersion> vec;
  for (int i = 0; i < num_versions; ++i)
  {
    vec.push_back(static_cast<SvgVersion>(versions[i]));
  }
  return vec;
}

std::string SvgSurface::version_to_string(SvgVersion version)
{
  return std::string(cairo_svg_version_to_string(static_cast<cairo_svg_version_t>(version)));
}

#endif // CAIRO_HAS_SVG_SURFACE




#ifdef CAIRO_HAS_GLITZ_SURFACE

GlitzSurface::GlitzSurface(cairo_surface_t* cobject, bool has_reference)
: Surface(cobject, has_reference)
{ }

GlitzSurface::~GlitzSurface()
{
  // surface is destroyed in base class
}

RefPtr<GlitzSurface> GlitzSurface::create(glitz_surface_t *surface)
{
  cairo_surface_t* cobject = cairo_glitz_surface_create(surface);
  check_status_and_throw_exception(cairo_surface_status(cobject));
  return RefPtr<GlitzSurface>(new GlitzSurface(cobject, true /* has reference */));
}

#endif // CAIRO_HAS_GLITZ_SURFACE

} //namespace Cairo

// vim: ts=2 sw=2 et
