// -*- c++ -*-

#ifndef _GLIBMM_INTERFACE_P_H
#define _GLIBMM_INTERFACE_P_H

#include <glibmm/class.h>


namespace Glib
{

class Interface_Class : public Glib::Class
{
public:
  typedef Interface       CppObjectType;
  typedef GTypeInterface  BaseClassType;

  void add_interface(GType instance_type) const;
};

} // namespace Glib

#endif /* _GLIBMM_INTERFACE_P_H */

