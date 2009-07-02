
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <giomm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "unixinputstream.h"
#include "unixoutputstream.h"
#include "desktopappinfo.h"
#include "appinfo.h"
#include "asyncresult.h"
#include "cancellable.h"
#include "drive.h"
#include "error.h"
#include "file.h"
#include "fileattributeinfo.h"
#include "fileattributeinfolist.h"
#include "fileenumerator.h"
#include "fileicon.h"
#include "fileinfo.h"
#include "fileinputstream.h"
#include "fileoutputstream.h"
#include "filemonitor.h"
#include "filterinputstream.h"
#include "filteroutputstream.h"
#include "filenamecompleter.h"
#include "icon.h"
#include "inputstream.h"
#include "loadableicon.h"
#include "mount.h"
#include "mountoperation.h"
#include "outputstream.h"
#include "seekable.h"
#include "volume.h"
#include "volumemonitor.h"
#include "bufferedinputstream.h"
#include "bufferedoutputstream.h"
#include "datainputstream.h"
#include "dataoutputstream.h"
#include "enums.h"
#include "memoryinputstream.h"
#include "themedicon.h"

extern "C"
{

//Declarations of the *_get_type() functions:

GType g_app_launch_context_get_type(void);
GType g_buffered_input_stream_get_type(void);
GType g_buffered_output_stream_get_type(void);
GType g_cancellable_get_type(void);
GType g_data_input_stream_get_type(void);
GType g_data_output_stream_get_type(void);
#ifndef G_OS_WIN32
GType g_desktop_app_info_get_type(void);
#endif //G_OS_WIN32
GType g_file_enumerator_get_type(void);
GType g_file_icon_get_type(void);
GType g_file_info_get_type(void);
GType g_file_input_stream_get_type(void);
GType g_file_monitor_get_type(void);
GType g_file_output_stream_get_type(void);
GType g_filename_completer_get_type(void);
GType g_filter_input_stream_get_type(void);
GType g_filter_output_stream_get_type(void);
GType g_input_stream_get_type(void);
GType g_memory_input_stream_get_type(void);
GType g_mount_operation_get_type(void);
GType g_output_stream_get_type(void);
GType g_themed_icon_get_type(void);
#ifndef G_OS_WIN32
GType g_unix_input_stream_get_type(void);
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
GType g_unix_output_stream_get_type(void);
#endif //G_OS_WIN32
GType g_volume_monitor_get_type(void);

//Declarations of the *_error_quark() functions:

GQuark g_io_error_quark(void);
} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Gio {  class AppLaunchContext_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class BufferedInputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class BufferedOutputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class Cancellable_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class DataInputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class DataOutputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gio {  class DesktopAppInfo_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gio {  class FileEnumerator_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FileIcon_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FileInfo_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FileInputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FileMonitor_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FileOutputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FilenameCompleter_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FilterInputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class FilterOutputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class InputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class MemoryInputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class MountOperation_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class OutputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gio {  class ThemedIcon_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gio {  class UnixInputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
namespace Gio {  class UnixOutputStream_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gio {  class VolumeMonitor_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }

namespace Gio { 

void wrap_init()
{
  // Register Error domains:
  Glib::Error::register_domain(g_io_error_quark(), &Gio::Error::throw_func);

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(g_app_launch_context_get_type(), &Gio::AppLaunchContext_Class::wrap_new);
  Glib::wrap_register(g_buffered_input_stream_get_type(), &Gio::BufferedInputStream_Class::wrap_new);
  Glib::wrap_register(g_buffered_output_stream_get_type(), &Gio::BufferedOutputStream_Class::wrap_new);
  Glib::wrap_register(g_cancellable_get_type(), &Gio::Cancellable_Class::wrap_new);
  Glib::wrap_register(g_data_input_stream_get_type(), &Gio::DataInputStream_Class::wrap_new);
  Glib::wrap_register(g_data_output_stream_get_type(), &Gio::DataOutputStream_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(g_desktop_app_info_get_type(), &Gio::DesktopAppInfo_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(g_file_enumerator_get_type(), &Gio::FileEnumerator_Class::wrap_new);
  Glib::wrap_register(g_file_icon_get_type(), &Gio::FileIcon_Class::wrap_new);
  Glib::wrap_register(g_file_info_get_type(), &Gio::FileInfo_Class::wrap_new);
  Glib::wrap_register(g_file_input_stream_get_type(), &Gio::FileInputStream_Class::wrap_new);
  Glib::wrap_register(g_file_monitor_get_type(), &Gio::FileMonitor_Class::wrap_new);
  Glib::wrap_register(g_file_output_stream_get_type(), &Gio::FileOutputStream_Class::wrap_new);
  Glib::wrap_register(g_filename_completer_get_type(), &Gio::FilenameCompleter_Class::wrap_new);
  Glib::wrap_register(g_filter_input_stream_get_type(), &Gio::FilterInputStream_Class::wrap_new);
  Glib::wrap_register(g_filter_output_stream_get_type(), &Gio::FilterOutputStream_Class::wrap_new);
  Glib::wrap_register(g_input_stream_get_type(), &Gio::InputStream_Class::wrap_new);
  Glib::wrap_register(g_memory_input_stream_get_type(), &Gio::MemoryInputStream_Class::wrap_new);
  Glib::wrap_register(g_mount_operation_get_type(), &Gio::MountOperation_Class::wrap_new);
  Glib::wrap_register(g_output_stream_get_type(), &Gio::OutputStream_Class::wrap_new);
  Glib::wrap_register(g_themed_icon_get_type(), &Gio::ThemedIcon_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(g_unix_input_stream_get_type(), &Gio::UnixInputStream_Class::wrap_new);
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
  Glib::wrap_register(g_unix_output_stream_get_type(), &Gio::UnixOutputStream_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(g_volume_monitor_get_type(), &Gio::VolumeMonitor_Class::wrap_new);

  // Register the gtkmm gtypes:
  Gio::AppLaunchContext::get_type();
  Gio::BufferedInputStream::get_type();
  Gio::BufferedOutputStream::get_type();
  Gio::Cancellable::get_type();
  Gio::DataInputStream::get_type();
  Gio::DataOutputStream::get_type();
#ifndef G_OS_WIN32
  Gio::DesktopAppInfo::get_type();
#endif //G_OS_WIN32
  Gio::FileEnumerator::get_type();
  Gio::FileIcon::get_type();
  Gio::FileInfo::get_type();
  Gio::FileInputStream::get_type();
  Gio::FileMonitor::get_type();
  Gio::FileOutputStream::get_type();
  Gio::FilenameCompleter::get_type();
  Gio::FilterInputStream::get_type();
  Gio::FilterOutputStream::get_type();
  Gio::InputStream::get_type();
  Gio::MemoryInputStream::get_type();
  Gio::MountOperation::get_type();
  Gio::OutputStream::get_type();
  Gio::ThemedIcon::get_type();
#ifndef G_OS_WIN32
  Gio::UnixInputStream::get_type();
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
  Gio::UnixOutputStream::get_type();
#endif //G_OS_WIN32
  Gio::VolumeMonitor::get_type();

} // wrap_init()

} //Gio


