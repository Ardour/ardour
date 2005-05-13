// -*- c++ -*-
// This is a generated file, do not edit.  Generated from value_basictypes.h.m4

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifndef _GLIBMM_VALUE_H_INCLUDE_VALUE_BASICTYPES_H
#error "glibmm/value_basictypes.h cannot be included directly"
#endif
#endif

/* Suppress warnings about `long long' when GCC is in -pedantic mode.
 */
#if (__GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96))
#pragma GCC system_header
#endif

namespace Glib
{

/**
 * @ingroup glibmmValue
 */
template <>
class Value<bool> : public ValueBase
{
public:
  typedef bool CppType;
  typedef gboolean CType;

  static GType value_type() G_GNUC_CONST;

  void set(bool data);
  bool get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<char> : public ValueBase
{
public:
  typedef char CppType;
  typedef gchar CType;

  static GType value_type() G_GNUC_CONST;

  void set(char data);
  char get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<unsigned char> : public ValueBase
{
public:
  typedef unsigned char CppType;
  typedef guchar CType;

  static GType value_type() G_GNUC_CONST;

  void set(unsigned char data);
  unsigned char get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<int> : public ValueBase
{
public:
  typedef int CppType;
  typedef gint CType;

  static GType value_type() G_GNUC_CONST;

  void set(int data);
  int get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<unsigned int> : public ValueBase
{
public:
  typedef unsigned int CppType;
  typedef guint CType;

  static GType value_type() G_GNUC_CONST;

  void set(unsigned int data);
  unsigned int get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<long> : public ValueBase
{
public:
  typedef long CppType;
  typedef glong CType;

  static GType value_type() G_GNUC_CONST;

  void set(long data);
  long get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<unsigned long> : public ValueBase
{
public:
  typedef unsigned long CppType;
  typedef gulong CType;

  static GType value_type() G_GNUC_CONST;

  void set(unsigned long data);
  unsigned long get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<long long> : public ValueBase
{
public:
  typedef long long CppType;
  typedef gint64 CType;

  static GType value_type() G_GNUC_CONST;

  void set(long long data);
  long long get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<unsigned long long> : public ValueBase
{
public:
  typedef unsigned long long CppType;
  typedef guint64 CType;

  static GType value_type() G_GNUC_CONST;

  void set(unsigned long long data);
  unsigned long long get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<float> : public ValueBase
{
public:
  typedef float CppType;
  typedef gfloat CType;

  static GType value_type() G_GNUC_CONST;

  void set(float data);
  float get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<double> : public ValueBase
{
public:
  typedef double CppType;
  typedef gdouble CType;

  static GType value_type() G_GNUC_CONST;

  void set(double data);
  double get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};


/**
 * @ingroup glibmmValue
 */
template <>
class Value<void*> : public ValueBase
{
public:
  typedef void* CppType;
  typedef gpointer CType;

  static GType value_type() G_GNUC_CONST;

  void set(void* data);
  void* get() const;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
  GParamSpec* create_param_spec(const Glib::ustring& name) const;
#endif
};

} // namespace Glib

