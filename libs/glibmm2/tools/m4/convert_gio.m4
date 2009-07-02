_CONV_ENUM(G,PasswordSave)
_CONV_ENUM(G,AskPasswordFlags)
_CONV_ENUM(G,MountOperationResult)
_CONV_ENUM(G,MountUnmountFlags)
_CONV_ENUM(G,MountMountFlags)
_CONV_ENUM(G,FileAttributeType)
_CONV_ENUM(G,FileAttributeInfoFlags)
_CONV_ENUM(G,FileCopyFlags)
_CONV_ENUM(G,FileCreateFlags)
_CONV_ENUM(G,FileMonitorFlags)
_CONV_ENUM(G,FileMonitorEvent)
_CONV_ENUM(G,FileQueryInfoFlags)
_CONV_ENUM(G,FileType)
_CONV_ENUM(G,OutputStreamSpliceFlags)
_CONV_ENUM(G,AppInfoCreateFlags)
_CONV_ENUM(G,DataStreamByteOrder)
_CONV_ENUM(G,DataStreamNewlineType)
_CONV_ENUM(G,EmblemOrigin)


# AppInfo
_CONVERSION(`GAppInfo*',`Glib::RefPtr<AppInfo>',`Glib::wrap($3)')
_CONVERSION(`const Glib::RefPtr<AppLaunchContext>&',`GAppLaunchContext*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`GAppLaunchContext*',`const Glib::RefPtr<AppLaunchContext>&',Glib::wrap($3))
_CONVERSION(`const Glib::RefPtr<AppInfo>&',`GAppInfo*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`Glib::RefPtr<AppInfo>',`GAppInfo*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`GAppInfo*',`const Glib::RefPtr<AppInfo>&',`Glib::wrap($3)')
_CONVERSION(`const Glib::ListHandle< Glib::RefPtr<Gio::File> >&',`GList*',`$3.data()')

# AsyncResult
_CONVERSION(`Glib::RefPtr<Glib::Object>',`GObject*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<AsyncResult>&',`GAsyncResult*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`Glib::RefPtr<AsyncResult>&',`GAsyncResult*',__CONVERT_REFPTR_TO_P)

# Cancellable
_CONVERSION(`const Glib::RefPtr<Cancellable>&',`GCancellable*',__CONVERT_CONST_REFPTR_TO_P)
_CONVERSION(`GCancellable*', `Glib::RefPtr<Cancellable>', `Glib::wrap($3)')

# DesktopAppInfo
_CONVERSION(`GDesktopAppInfo*', `Glib::RefPtr<DesktopAppInfo>', `Glib::wrap($3)')

# Drive
_CONVERSION(`GDrive*',`Glib::RefPtr<Drive>',`Glib::wrap($3)')

# File
_CONVERSION(`return-char*',`std::string',`Glib::convert_return_gchar_ptr_to_stdstring($3)')
_CONVERSION(`Glib::RefPtr<File>',`GFile*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<File>&',`GFile*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<const File>&',`GFile*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gio::File))
_CONVERSION(`GFile*',`Glib::RefPtr<File>',`Glib::wrap($3)')
_CONVERSION(`GFile*',`Glib::RefPtr<const File>',`Glib::wrap($3)')


# FileAttribute
_CONVERSION(`GFileAttributeValue*',`FileAttributeValue',`Glib::wrap($3)')
_CONVERSION(`const FileAttributeValue&',`const GFileAttributeValue*',`$3.gobj()')
_CONVERSION(`GFileAttributeInfoList*',`Glib::RefPtr<FileAttributeInfoList>',`Glib::wrap($3)')

#FileEnumerator
_CONVERSION(`GFileEnumerator*',`Glib::RefPtr<FileEnumerator>',`Glib::wrap($3)')

# FileInfo
_CONVERSION(`GFileInfo*',`Glib::RefPtr<FileInfo>',`Glib::wrap($3)')
_CONVERSION(`Glib::RefPtr<FileInfo>&',`GFileInfo*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<FileInfo>&',`GFileInfo*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`char**',`Glib::StringArrayHandle',`Glib::StringArrayHandle($3)')
_CONVERSION(`Glib::TimeVal&', `GTimeVal*', static_cast<$2>(&$3))
_CONVERSION(`const Glib::TimeVal&', `GTimeVal*', const_cast<GTimeVal*>(static_cast<const GTimeVal*>(&$3)))
_CONVERSION(`const Glib::RefPtr<FileAttributeMatcher>&',`GFileAttributeMatcher*',__CONVERT_CONST_REFPTR_TO_P)

# FileInputStream
_CONVERSION(`GFileInputStream*',`Glib::RefPtr<FileInputStream>',`Glib::wrap($3)')

# FileMonitor
_CONVERSION(`GFileMonitor*',`Glib::RefPtr<FileMonitor>',`Glib::wrap($3)')

# FileOutputStream
_CONVERSION(`GFileOutputStream*',`Glib::RefPtr<FileOutputStream>',`Glib::wrap($3)')

# FilterInputStream
#_CONVERSION(`GFilterInputStream*',`Glib::RefPtr<FilterInputStream>',`Glib::wrap($3)')


# Icon
_CONVERSION(`GIcon*',`Glib::RefPtr<Icon>',`Glib::wrap($3)')
_CONVERSION(`const Glib::RefPtr<Icon>&',`GIcon*',__CONVERT_CONST_REFPTR_TO_P)
_CONVERSION(`Glib::RefPtr<Icon>',`GIcon*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`Glib::RefPtr<const Icon>',`GIcon*',__CONVERT_CONST_REFPTR_TO_P)

_CONVERSION(`const Glib::RefPtr<Emblem>&',`GEmblem*',__CONVERT_CONST_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Emblem>&',`GEmblem*',__CONVERT_CONST_REFPTR_TO_P)

# InputStream
_CONVERSION(`const Glib::RefPtr<InputStream>&',`GInputStream*',__CONVERT_CONST_REFPTR_TO_P)
_CONVERSION(`GInputStream*',`Glib::RefPtr<InputStream>',`Glib::wrap($3)')

#Mount
_CONVERSION(`GMount*',`Glib::RefPtr<Mount>',`Glib::wrap($3)')
_CONVERSION(`const Glib::RefPtr<Mount>&',`GMount*',__CONVERT_CONST_REFPTR_TO_P)

# MountOptions
_CONVERSION(`GPasswordSave',`PasswordSave',`($2)$3')
_CONVERSION(`PasswordSave',`GPasswordSave',`($2)$3')

#MountOperation
#_CONVERSION(`GAskPasswordFlags',`AskPasswordFlags',`($2)$3')

# OutputStream
_CONVERSION(`GOutputStream*',`Glib::RefPtr<OutputStream>',`Glib::wrap($3)')
_CONVERSION(`const Glib::RefPtr<OutputStream>&',`GOutputStream*',__CONVERT_CONST_REFPTR_TO_P)

#Volume
_CONVERSION(`GVolume*',`Glib::RefPtr<Volume>',`Glib::wrap($3)')

# VolumeMonitor
_CONVERSION(`GVolumeMonitor*',`Glib::RefPtr<VolumeMonitor>',`Glib::wrap($3)')
_CONVERSION(`const Glib::RefPtr<Drive>&',`GDrive*',__CONVERT_CONST_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Mount>&',`GMount*',__CONVERT_CONST_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Volume>&',`GVolume*',__CONVERT_CONST_REFPTR_TO_P)

#Vfs
_CONVERSION(`GVfs*', `Glib::RefPtr<Vfs>', `Glib::wrap($3)')

