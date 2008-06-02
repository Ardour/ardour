_PUSH()

dnl
dnl  These variables affect the generation of the list
dnl
define(GP_LIST_HELPER_NAMESPACE,`define(`__LIST_NAMESPACE__',$1)')
define(GP_LIST_ELEM,`define(`__LISTELEM__',`$*')')
define(GP_LIST_ITER,`define(`__LISTITER__',`$*')')
define(GP_LIST_NOINSERT,`define(`__LISTEO__')')

dnl
dnl GP_LIST(ListName, ParentCppType, ParentCType, ChildCppType, FieldNameC)
dnl
dnl In the .ccg file, you'll need to implement:
dnl   iterator insert(iterator position, element_type& e);
dnl
dnl  Fieldname assumed to be children if not specified
define(GP_LIST,`
_PUSH()

define(`__LISTNAME__',$1)
define(`__LISTPARENT__',$2)
define(`__LISTPARENT_G__',$3)
define(`__LISTTYPE__',$4)
define(`__LISTELEM__',const Element)
define(`__LISTITER__',Glib::List_Iterator< __LISTTYPE__ >)
define(`__LIST_NAMESPACE__',$2_Helpers)
#define(`__LISTFIELD__',ifelse($5,,children,$5))
define(`__LISTFIELD__',$5)

_SECTION(SECTION_USR)
')

dnl
dnl  GP_LIST_END()
dnl
dnl   Closes a list
define(GP_LIST_END,`dnl
_POP()
  
class __LISTNAME__ : public Glib::HelperList< __LISTTYPE__, __LISTELEM__, __LISTITER__ >
{
public:
  __LISTNAME__`'();
  explicit __LISTNAME__`'(__LISTPARENT_G__* gparent);
  __LISTNAME__`'(const __LISTNAME__& src);
  virtual ~__LISTNAME__`'() {}

  __LISTNAME__& operator=(const __LISTNAME__& src);

  typedef Glib::HelperList< __LISTTYPE__, __LISTELEM__,  __LISTITER__ > type_base;

  __LISTPARENT_G__* gparent();
  const __LISTPARENT_G__* gparent() const;

  virtual GList*& glist() const;      // front of list

  virtual void erase(iterator start, iterator stop);
  virtual iterator erase(iterator);  //Implented as custom or by LIST_CONTAINER_REMOVE
  virtual void remove(const_reference); //Implented as custom or by LIST_CONTAINER_REMOVE

  /// This is order n. (use at own risk)
  reference operator[](size_type l) const;

ifdef(`__LISTEO__',`dnl
protected:
  //Hide these because it's read-only:
  iterator insert(iterator position, element_type& e);

  inline void pop_front();
  inline void pop_back();
,`dnl
public:
  iterator insert(iterator position, element_type& e); //custom-implemented.

  template <class InputIterator>
  inline void insert(iterator position, InputIterator first, InputIterator last)
  {
    for(;first != last; ++first)
      position = insert(position, *first);
  }

 inline void push_front(element_type& e)
    { insert(begin(), e); }
  inline void push_back(element_type& e)
    { insert(end(), e); }
')dnl

_IMPORT(SECTION_USR)
};

_PUSH(SECTION_CC)

namespace __LIST_NAMESPACE__
{

__LISTNAME__::__LISTNAME__`'()
{}

__LISTNAME__::__LISTNAME__`'(__LISTPARENT_G__* gparent)
: type_base((GObject*)gparent)
{}

__LISTNAME__::__LISTNAME__`'(const __LISTNAME__& src)
:
  type_base(src)
{}

__LISTNAME__& __LISTNAME__::operator=(const __LISTNAME__& src)
{
  type_base::operator=(src);
  return *this;
}

ifelse(__LISTFIELD__,CUSTOM,`dnl
',`dnl else
GList*& __LISTNAME__::glist() const
{
  return ((__LISTPARENT_G__*)gparent_)->__LISTFIELD__;
}
')dnl endif

void __LISTNAME__::erase(iterator start, iterator stop)
{
  type_base::erase(start, stop);
}

__LISTPARENT_G__* __LISTNAME__::gparent()
{
  return (__LISTPARENT_G__*)type_base::gparent();
}

const __LISTPARENT_G__* __LISTNAME__::gparent() const
{
  return (__LISTPARENT_G__*)type_base::gparent();
}

__LISTNAME__::reference __LISTNAME__::operator[](size_type l) const
{
  return type_base::operator[](l);
}

} /* namespace __LIST_NAMESPACE__ */

undefine(`__LISTNAME__')dnl
undefine(`__LISTTYPE__')dnl
undefine(`__LISTPARENT__')dnl
undefine(`__LISTELEM__')dnl
undefine(`__LISTFIELD__')dnl
_POP()
')

dnl
dnl  GP_LIST_FIND(access_method)
dnl
dnl    Defines  find(containertype) and find(Widget&)
dnl    access_method is the name of method returning a Widget*
define(GP_LIST_FIND,`
  iterator find(const_reference c);
  iterator find(Widget&);
_PUSH(SECTION_CC)

namespace __LIST_NAMESPACE__
{

__LISTNAME__::iterator __LISTNAME__::find(const_reference w)
{
  iterator i = begin();
  for(i = begin(); i != end() && (i->ifelse($1,,,$1()->)gobj() != w.ifelse($1,,,$1()->)gobj()); i++);
  return i;
}

__LISTNAME__::iterator __LISTNAME__::find(Widget& w)
{
  iterator i;
  for(i = begin(); i != end() && ((GtkWidget*)i->ifelse($1,,,$1()->)gobj() != w.gobj()); i++);
  return i;
}

} /* namespace __LIST_NAMESPACE__ */

_POP()
')

dnl
dnl  GP_LIST_CONTAINER_REMOVE(access_method)
dnl
dnl    Implements remove(const_reference), erase(iterator)
dnl    and defines remove(Widget&)
dnl    (assumes that the widget uses gtk+ container methods).
dnl    access_method is the name of the method returning a Widget*
define(GP_LIST_CONTAINER_REMOVE,`
virtual void remove(Widget& w); //Implented as custom or by LIST_CONTAINER_REMOVE
_PUSH(SECTION_CC)

namespace __LIST_NAMESPACE__
{

void __LISTNAME__::remove(const_reference child)
{
  gtk_container_remove(GTK_CONTAINER(gparent_),
                       (GtkWidget*)(child.ifelse($1,,,$1()->)gobj()));
}

void __LISTNAME__::remove(Widget& widget)
{
  gtk_container_remove(GTK_CONTAINER(gparent_), (GtkWidget*)(widget.gobj()));
}

__LISTNAME__::iterator __LISTNAME__::erase(iterator position)
{
  //Check that it is a valid iterator, to a real item:
  if ( !position.node_|| (position == end()) )
    return end();

  //Get an iterator the the next item, to return:
  iterator next = position;
  next++;

  //Use GTK+ C function to remove it, by providing the GtkWidget*:
  gtk_container_remove( GTK_CONTAINER(gparent_), (GtkWidget*)(position->ifelse($1,,,$1()->)gobj()) );
  return next;
}

} /* namespace __LIST_NAMESPACE__ */

_POP()
')

_POP()dnl
