
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <glibmm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "convert.h"
#include "date.h"
#include "fileutils.h"
#include "iochannel.h"
#include "keyfile.h"
#include "markup.h"
#include "module.h"
#include "optioncontext.h"
#include "optionentry.h"
#include "optiongroup.h"
#include "shell.h"
#include "spawn.h"
#include "thread.h"
#include "unicode.h"

extern "C"
{

//Declarations of the *_get_type() functions:


//Declarations of the *_error_quark() functions:

GQuark g_convert_error_quark(void);
GQuark g_file_error_quark(void);
GQuark g_io_channel_error_quark(void);
GQuark g_key_file_error_quark(void);
GQuark g_markup_error_quark(void);
GQuark g_option_error_quark(void);
GQuark g_shell_error_quark(void);
GQuark g_spawn_error_quark(void);
GQuark g_thread_error_quark(void);
} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:


namespace Glib { 

void wrap_init()
{
  // Register Error domains:
  Glib::Error::register_domain(g_convert_error_quark(), &Glib::ConvertError::throw_func);
  Glib::Error::register_domain(g_file_error_quark(), &Glib::FileError::throw_func);
  Glib::Error::register_domain(g_io_channel_error_quark(), &Glib::IOChannelError::throw_func);
  Glib::Error::register_domain(g_key_file_error_quark(), &Glib::KeyFileError::throw_func);
  Glib::Error::register_domain(g_markup_error_quark(), &Glib::MarkupError::throw_func);
  Glib::Error::register_domain(g_option_error_quark(), &Glib::OptionError::throw_func);
  Glib::Error::register_domain(g_shell_error_quark(), &Glib::ShellError::throw_func);
  Glib::Error::register_domain(g_spawn_error_quark(), &Glib::SpawnError::throw_func);
  Glib::Error::register_domain(g_thread_error_quark(), &Glib::ThreadError::throw_func);

// Map gtypes to gtkmm wrapper-creation functions:

  // Register the gtkmm gtypes:

} // wrap_init()

} //Glib


