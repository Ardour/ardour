include(convert_glib.m4)

_EQUAL(gint8[],gint8*)
_EQUAL(guchar,guint8)
_EQUAL(guchar*,guint8*)
_EQUAL(gfloat,float)

# Enums
_CONV_ENUM(Gdk,AxisUse)
_CONV_ENUM(Gdk,ByteOrder)
_CONV_ENUM(Gdk,CapStyle)
_CONV_ENUM(Gdk,Colorspace)
_CONV_ENUM(Gdk,CursorType)
_CONV_ENUM(Gdk,DragAction)
_CONV_ENUM(Gdk,DragProtocol)
_CONV_ENUM(Gdk,EventMask)
_CONV_ENUM(Gdk,EventType)
_CONV_ENUM(Gdk,ExtensionMode)
_CONV_ENUM(Gdk,Fill)
_CONV_ENUM(Gdk,FillRule)
_CONV_ENUM(Gdk,Function)
_CONV_ENUM(Gdk,GCValuesMask)
_CONV_ENUM(Gdk,Gravity)
_CONV_ENUM(Gdk,ImageType)
_CONV_ENUM(Gdk,InputCondition)
_CONV_ENUM(Gdk,InputMode)
_CONV_ENUM(Gdk,InterpType)
_CONV_ENUM(Gdk,JoinStyle)
_CONV_ENUM(Gdk,LineStyle)
_CONV_ENUM(Gdk,ModifierType)
_CONV_ENUM(Gdk,OverlapType)
_CONV_ENUM(Gdk,PixbufAlphaMode)
_CONV_ENUM(Gdk,RgbDither)
_CONV_ENUM(Gdk,Status)
_CONV_ENUM(Gdk,SubwindowMode)
_CONV_ENUM(Gdk,VisualType)
_CONV_ENUM(Gdk,WindowAttributesType)
_CONV_ENUM(Gdk,WindowEdge)
_CONV_ENUM(Gdk,WindowHints)
_CONV_ENUM(Gdk,WindowState)
_CONV_ENUM(Gdk,WindowType)
_CONV_ENUM(Gdk,WindowTypeHint)
_CONV_ENUM(Gdk,WMDecoration)
_CONV_ENUM(Gdk,WMFunction)
_CONV_ENUM(Gdk,GrabStatus)


_CONVERSION(`Gdk::EventMask',`gint',`$3')
_CONVERSION(`gint',`Gdk::EventMask',`static_cast<Gdk::EventMask>($3)')
_CONVERSION(`ModifierType&',`GdkModifierType*',`(($2) &($3))')
_CONVERSION(`WMDecoration&',`GdkWMDecoration*',`(($2) &($3))')
_CONVERSION(`GdkDragProtocol&',`GdkDragProtocol*',`&($3)')

_CONVERSION(`GdkRectangle&',`GdkRectangle*',`&$3',`*$3')
_CONVERSION(`GdkRgbCmap&',`GdkRgbCmap*',`&$3',`*$3')

# TODO: Remove this, and use Gdk::Device:
_CONVERSION(`GdkDevice*',`const GdkDevice*',`$3')

_CONVERSION(`GdkKeymap*',`const GdkKeymap*',`$3')




# for GtkStyle public struct members
_CONVERSION(`Gdk::Color',`GdkColor', `(*($3).gobj())')
_CONVERSION(`GdkColor',`Gdk::Color', `Gdk::Color(const_cast<GdkColor*>(&($3)), true)')

# Ref (gdkmm) -> Ptr (gtk+)
_CONVERSION(`Color&',`GdkColor*',($3).gobj())
_CONVERSION(`Rectangle&',`GdkRectangle*',($3).gobj())
_CONVERSION(`Gdk::Rectangle&',`GdkRectangle*',($3).gobj())
_CONVERSION(`Font&',`GdkFont*',($3).gobj())
_CONVERSION(`Region&',`GdkRegion*',($3).gobj())

_CONVERSION(`const Glib::RefPtr<Gdk::Colormap>&',`GdkColormap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::Pixmap>&',`GdkPixmap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::Window>&',`GdkWindow*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Window>&',`GdkWindow*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Pixmap>&',`GdkPixmap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::Pixmap>&',`GdkPixmap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Bitmap>&',`GdkBitmap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::Bitmap>&',`GdkBitmap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Colormap>&',`GdkColormap*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<const Colormap>&',`GdkColormap*',__CONVERT_CONST_REFPTR_TO_P_SUN(Colormap))
_CONVERSION(`const Glib::RefPtr<GC>&',`GdkGC*',`Glib::unwrap<Gdk::GC>($3)')
_CONVERSION(`const Glib::RefPtr<const GC>&',`GdkGC*',__CONVERT_CONST_REFPTR_TO_P_SUN(GC))
_CONVERSION(`const Glib::RefPtr<Gdk::GC>&',`GdkGC*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Drawable>&',`GdkDrawable*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<const Drawable>&',`GdkDrawable*',__CONVERT_CONST_REFPTR_TO_P_SUN(Drawable))
_CONVERSION(`const Glib::RefPtr<Image>&',`GdkImage*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<const Image>&',`GdkImage*',__CONVERT_CONST_REFPTR_TO_P_SUN(Image))
_CONVERSION(`const Glib::RefPtr<Gdk::Image>&',`GdkImage*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Pixbuf>&',`GdkPixbuf*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<const Pixbuf>&',`GdkPixbuf*',__CONVERT_CONST_REFPTR_TO_P_SUN(Pixbuf))
_CONVERSION(`const Glib::RefPtr<Gdk::Pixbuf>&',`GdkPixbuf*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`Glib::RefPtr<Gdk::Pixbuf>',`GdkPixbuf*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::PixbufAnimation>&',`GdkPixbufAnimation*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::PixbufAnimationIter>&',`GdkPixbufAnimationIter*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::DragContext>&',`GdkDragContext*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Display>&',`GdkDisplay*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Screen>&',`GdkScreen*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::Display>&',`GdkDisplay*',__CONVERT_REFPTR_TO_P)
_CONVERSION(`const Glib::RefPtr<Gdk::Screen>&',`GdkScreen*',__CONVERT_REFPTR_TO_P)



define(`__CFR2P',`const_cast<$`'2>($`'3.gobj())')
_CONVERSION(const Font&,GdkFont*,__CFR2P)
_CONVERSION(const Gdk::Color&,GdkColor*,__CFR2P)
_CONVERSION(const Color&,GdkColor*,__CFR2P)
_CONVERSION(const Gdk::Rectangle&,GdkRectangle*,__CFR2P)
_CONVERSION(const Rectangle&,GdkRectangle*,__CFR2P)
_CONVERSION(const Gdk::Geometry&,GdkGeometry*,const_cast<$2>(&($3)))
_CONVERSION(const Geometry&,GdkGeometry*,const_cast<$2>(&($3)))
_CONVERSION(const RgbCmap&,GdkRgbCmap*,__CFR2P)

_CONVERSION(`Gdk::Rectangle*',`GdkRectangle*',`Glib::unwrap($3)')
_CONVERSION(`const Gdk::Rectangle*',`GdkRectangle*',`Glib::unwrap(const_cast<Gdk::Rectangle*>($3))')
_CONVERSION(`GdkRectangle*',`Gdk::Rectangle*',`&Glib::wrap($3)')
_CONVERSION(`GdkRectangle*',`const Gdk::Rectangle*',`&Glib::wrap($3)')
_CONVERSION(`GdkRectangle*',`const Gdk::Rectangle&',`Glib::wrap($3)')


dnl TODO: Should this always be a copy?
_CONVERSION(const Cursor&,GdkCursor*,($3).gobj_copy())

# Special treatment for the Sun Forte compiler
#_CONVERSION(const Glib::RefPtr<const Gdk::Pixmap>&,GdkPixmap*,__CONVERT_CONST_REFPTR_TO_P)
#_CONVERSION(const Glib::RefPtr<const Gdk::Window>&,GdkWindow*,__CONVERT_CONST_REFPTR_TO_P)
#_CONVERSION(const Glib::RefPtr<const Gdk::Colormap>&,GdkColormap*,__CONVERT_CONST_REFPTR_TO_P)
#_CONVERSION(const Glib::RefPtr<const Gdk::Visual>&,GdkVisual*,__CONVERT_CONST_REFPTR_TO_P)
#_CONVERSION(const Glib::RefPtr<const Gdk::Bitmap>&,GdkBitmap*,__CONVERT_CONST_REFPTR_TO_P)
#_CONVERSION(const Glib::RefPtr<const Gdk::Image>&,GdkImage*,__CONVERT_CONST_REFPTR_TO_P)
#_CONVERSION(const Glib::RefPtr<const Gdk::GC>&,GdkGC*,__CONVERT_CONST_REFPTR_TO_P)

_CONVERSION(`const Glib::RefPtr<const Gdk::Pixmap>&', `GdkPixmap*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Pixmap))
_CONVERSION(`const Glib::RefPtr<const Gdk::Window>&', `GdkWindow*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Window))
_CONVERSION(`const Glib::RefPtr<const Window>&', `GdkWindow*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Window))
_CONVERSION(`const Glib::RefPtr<const Gdk::Colormap>&', `GdkColormap*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Colormap))
_CONVERSION(`const Glib::RefPtr<const Gdk::Visual>&', `GdkVisual*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Visual))
_CONVERSION(`const Glib::RefPtr<const Gdk::Bitmap>&', `GdkBitmap*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Bitmap))
_CONVERSION(`const Glib::RefPtr<const Gdk::Image>&', `GdkImage*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Image))
_CONVERSION(`const Glib::RefPtr<const Image>&', `GdkImage*',__CONVERT_CONST_REFPTR_TO_P_SUN(Image))
_CONVERSION(`const Glib::RefPtr<const Gdk::GC>&', `GdkGC*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::GC))
_CONVERSION(`const Glib::RefPtr<const GC>&', `GdkGC*',__CONVERT_CONST_REFPTR_TO_P_SUN(GC))
#_CONVERSION(`const Glib::RefPtr<const Gdk::Drawable>&', `GdkDrawable*',__CONVERT_CONST_REFPTR_TO_P_SUN(Gdk::Drawable))
#_CONVERSION(`const Glib::RefPtr<const Drawable>&', `GdkDrawable*',__CONVERT_CONST_REFPTR_TO_P_SUN(Drawable))
#_CONVERSION(`const Glib::RefPtr<const Display>&', `GdkDrawable*',__CONVERT_CONST_REFPTR_TO_P_SUN(Drawable))


_CONVERSION(`GdkWindow*',`Glib::RefPtr<Window>', `Glib::wrap((GdkWindowObject*)($3))')
_CONVERSION(`GdkWindow*',`Glib::RefPtr<const Window>', `Glib::wrap((GdkWindowObject*)($3))')
_CONVERSION(`GdkWindow*',`Glib::RefPtr<Gdk::Window>', `Glib::wrap((GdkWindowObject*)($3))')
_CONVERSION(`GdkWindow*',`Glib::RefPtr<const Gdk::Window>', `Glib::wrap((GdkWindowObject*)($3))')
_CONVERSION(`GdkWindow*',`const Glib::RefPtr<Gdk::Window>&', `Glib::wrap((GdkWindowObject*)($3), true)')
_CONVERSION(`GdkPixmap*',`Glib::RefPtr<Pixmap>', `Glib::wrap((GdkPixmapObject*)($3))')
_CONVERSION(`GdkPixmap*',`Glib::RefPtr<const Pixmap>', `Glib::wrap((GdkPixmapObject*)($3))')
_CONVERSION(`GdkPixmap*',`Glib::RefPtr<const Gdk::Pixmap>', `Glib::wrap((GdkPixmapObject*)($3))')
_CONVERSION(`GdkPixmap*',`Glib::RefPtr<Gdk::Pixmap>', `Glib::wrap((GdkPixmapObject*)($3))')
_CONVERSION(`GdkColormap*',`Glib::RefPtr<Colormap>', `Glib::wrap($3)')
_CONVERSION(`GdkColormap*',`Glib::RefPtr<const Colormap>', `Glib::wrap($3)')
_CONVERSION(`GdkColormap*',`Glib::RefPtr<Gdk::Colormap>', `Glib::wrap($3)')
_CONVERSION(`GdkVisual*',`Glib::RefPtr<Gdk::Visual>', `Glib::wrap($3)')
_CONVERSION(`GdkVisual*',`Glib::RefPtr<Visual>', `Glib::wrap($3)')
_CONVERSION(`GdkVisual*',`Glib::RefPtr<const Visual>', `Glib::wrap($3)')
_CONVERSION(`GdkImage*',`Glib::RefPtr<Image>', `Glib::wrap($3)')
_CONVERSION(`GdkPixbuf*',`Glib::RefPtr<Pixbuf>', `Glib::wrap($3)')
_CONVERSION(`GdkPixbuf*',`Glib::RefPtr<Gdk::Pixbuf>', `Glib::wrap($3)')
_CONVERSION(`GdkPixbufAnimationIter*',`Glib::RefPtr<PixbufAnimationIter>', `Glib::wrap($3)')
_CONVERSION(`GdkPixbuf*',`Glib::RefPtr<Gdk::Pixbuf>', Glib::wrap($3))
_CONVERSION(`GdkPixbufAnimation*',`Glib::RefPtr<Gdk::PixbufAnimation>', `Glib::wrap($3)')
_CONVERSION(`GdkGC*',`Glib::RefPtr<Gdk::GC>', `Glib::wrap($3)')
_CONVERSION(`GdkGC*',`Glib::RefPtr<const Gdk::GC>', `Glib::wrap($3)')

_CONVERSION(`GdkDisplay*',`Glib::RefPtr<Display>', `Glib::wrap($3)')
_CONVERSION(`GdkDisplay*',`Glib::RefPtr<const Display>', `Glib::wrap($3)')
_CONVERSION(`GdkDisplay*',`Glib::RefPtr<Gdk::Display>', `Glib::wrap($3)')
_CONVERSION(`GdkDisplay*',`Glib::RefPtr<const Gdk::Display>', `Glib::wrap($3)')

_CONVERSION(`GdkDisplayManager*',`Glib::RefPtr<DisplayManager>', `Glib::wrap($3)')
_CONVERSION(`GdkDisplayManager*',`Glib::RefPtr<const DisplayManager>', `Glib::wrap($3)')

_CONVERSION(`GdkScreen*',`Glib::RefPtr<Screen>', `Glib::wrap($3)')
_CONVERSION(`GdkScreen*',`Glib::RefPtr<const Screen>', `Glib::wrap($3)')
_CONVERSION(`GdkScreen*',`Glib::RefPtr<Gdk::Screen>', `Glib::wrap($3)')
_CONVERSION(`GdkScreen*',`Glib::RefPtr<const Gdk::Screen>', `Glib::wrap($3)')

_CONVERSION(`GdkDevice*',`Glib::RefPtr<Device>', `Glib::wrap($3)')
_CONVERSION(`GdkDevice*',`Glib::RefPtr<const Device>', `Glib::wrap($3)')







# Glib::ListHandle<> (gdkmm) -> GList (gdk)
_CONVERSION(`const Glib::ListHandle< Glib::RefPtr<Gdk::Pixbuf> >&',`GList*',`$3.data()')

# GList (gdk) -> Glib::ListHandle<> (gdkmm)
_CONVERSION(`GList*',`Glib::ListHandle< Glib::RefPtr<Gdk::Pixbuf> >',`$2($3, Glib::OWNERSHIP_SHALLOW)')
_CONVERSION(`GList*',`Glib::ListHandle< Glib::RefPtr<Device> >',`$2($3, Glib::OWNERSHIP_DEEP)')
_CONVERSION(`GList*',`Glib::ListHandle< Glib::RefPtr<Visual> >',`$2($3, Glib::OWNERSHIP_SHALLOW)')
_CONVERSION(`GList*',`Glib::ListHandle< Glib::RefPtr<Window> >',`$2($3, Glib::OWNERSHIP_SHALLOW)')
_CONVERSION(`GSList*',`Glib::SListHandle< Glib::RefPtr<Display> >',`$2($3, Glib::OWNERSHIP_SHALLOW)')




# XPM data
_CONVERSION(`const char*const*',`const char**',`const_cast<const char**>($3)',`$3')


_CONVERSION(GdkFont*, Gdk::Font, `Gdk::Font($3)')
_CONVERSION(GdkEvent*, Event, `Event($3)')
_CONVERSION(GdkRegion*, Region, `Region($3)')

_CONVERSION(`GdkTimeCoord**&',`GdkTimeCoord***',`&($3)')

dnl _CONVERSION(GdkPixmap*,Gdk::Pixmap&,`Glib::unwrap_boxed($3)',`$3')
dnl _CONVERSION(GdkBitmap*,Gdk::Bitmap&,`Glib::unwrap_boxed($3)',`$3')



# Used by signals:
_CONVERSION(`GdkDragContext*',`const Glib::RefPtr<Gdk::DragContext>&',Glib::wrap($3, true))
_CONVERSION(`GdkPixbuf*',`const Glib::RefPtr<Gdk::Pixbuf>&', Glib::wrap($3, true))
_CONVERSION(`GdkDragContext*',`Glib::RefPtr<Gdk::DragContext>',Glib::wrap($3, true))
_CONVERSION(`GdkDisplay*',`const Glib::RefPtr<Display>&', Glib::wrap($3, true))

