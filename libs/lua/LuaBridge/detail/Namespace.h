//------------------------------------------------------------------------------
/*
  https://github.com/vinniefalco/LuaBridge

  Copyright 2016, Robin Gareus <robin@gareus.org>
  Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
  Copyright 2007, Nathan Reed

  License: The MIT License (http://www.opensource.org/licenses/mit-license.php)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
//==============================================================================

#ifdef LUABINDINGDOC
#include <iostream>
#include <typeinfo>
#include <execinfo.h>
#include <type_traits>
#include <cxxabi.h>
#include <memory>
#include <string>
#include <cstdlib>

template <class T>
std::string type_name()
{
	typedef typename std::remove_reference<T>::type TR;
	std::unique_ptr<char, void(*)(void*)> own
		(
		 abi::__cxa_demangle(typeid(TR).name(), nullptr,
			 nullptr, nullptr),
		 std::free
		);
	std::string r = own != nullptr ? own.get() : typeid(TR).name();
	if (std::is_const<TR>::value)
		r += " const";
	if (std::is_volatile<TR>::value)
		r += " volatile";
	if (std::is_lvalue_reference<T>::value)
		r += "&";
	else if (std::is_rvalue_reference<T>::value)
		r += "&&";
	return r;
}

//#define LUADOCOUT

#ifdef LUADOCOUT // lua
#define KEYSTA "[\""
#define KEYEND "\"] = "
#else // JSON
#define KEYSTA "\""
#define KEYEND "\" : "
#endif

#define CLASSDOC(TYPE, LUANAME, DECL, PARENTDECL) \
  if (LuaBindingDoc::printBindings ()) { \
    std::cout <<   "{ " << KEYSTA << "type"   << KEYEND << "  \""  << TYPE << "\",\n"; \
    std::cout <<   "  " << KEYSTA << "lua"    << KEYEND << "   \"" << LUANAME << "\",\n"; \
    std::cout <<   "  " << KEYSTA << "decl"   << KEYEND << "  \"" << DECL << "\",\n"; \
    std::cout <<   "  " << KEYSTA << "parent" << KEYEND << "\""  << PARENTDECL << "\"\n"; \
    std::cout <<   "},\n"; \
  }

#define PRINTDOC(TYPE, LUANAME, RETVAL, DECL) \
  if (LuaBindingDoc::printBindings ()) { \
    std::cout <<   "{ " << KEYSTA << "type"   << KEYEND << "  \""  << TYPE << "\",\n"; \
    std::cout <<   "  " << KEYSTA << "lua"    << KEYEND << "   \"" << LUANAME << "\",\n"; \
    if (!(RETVAL).empty()) { \
      std::cout << "  " << KEYSTA << "ret"    << KEYEND << "   \"" << (RETVAL) << "\",\n"; \
    } \
    std::cout <<   "  " << KEYSTA << "decl"   << KEYEND << "  \""  << DECL << "\"\n"; \
    std::cout <<   "},\n"; \
  }

#define FUNDOC(TYPE, NAME, FUNCTOR) \
  PRINTDOC(TYPE, _name << NAME, \
      type_name< typename FuncTraits <FUNCTOR>::ReturnType >(), \
      type_name< typename FuncTraits <FUNCTOR>::DeclType >())

#define DATADOC(TYPE, NAME, FUNCTOR) \
  PRINTDOC(TYPE, _name << NAME, \
      std::string(), \
      type_name< decltype(FUNCTOR) >())\


#else

#define CLASSDOC(TYPE, LUANAME, DECL, PARENTDECL)
#define PRINTDOC(TYPE, LUANAME, RETVAL, DECL)
#define FUNDOC(TYPE, NAME, FUNCTOR)
#define DATADOC(TYPE, NAME, FUNCTOR)

#endif

/** Provides C++ to Lua registration capabilities.

    This class is not instantiated directly, call `getGlobalNamespace` to start
    the registration process.
*/
class Namespace
{
private:
  Namespace& operator= (Namespace const& other);

  lua_State* const L;
  int mutable m_stackSize;

private:
  //============================================================================
  /**
    Error reporting.

    VF: This function looks handy, why aren't we using it?
  */
#if 0
  static int luaError (lua_State* L, std::string message)
  {
    assert (lua_isstring (L, lua_upvalueindex (1)));
    std::string s;

    // Get information on the caller's caller to format the message,
    // so the error appears to originate from the Lua source.
    lua_Debug ar;
    int result = lua_getstack (L, 2, &ar);
    if (result != 0)
    {
      lua_getinfo (L, "Sl", &ar);
      s = ar.short_src;
      if (ar.currentline != -1)
      {
        // poor mans int to string to avoid <strstrream>.
        lua_pushnumber (L, ar.currentline);
        s = s + ":" + lua_tostring (L, -1) + ": ";
        lua_pop (L, 1);
      }
    }

    s = s + message;

    return luaL_error (L, s.c_str ());
  }
#endif

  //----------------------------------------------------------------------------
  /**
    Pop the Lua stack.
  */
  void pop (int n) const
  {
    if (m_stackSize >= n && lua_gettop (L) >= n)
    {
      lua_pop (L, n);
      m_stackSize -= n;
    }
    else
    {
      throw std::logic_error ("invalid stack");
    }
  }

private:
  /**
    Factored base to reduce template instantiations.
  */
  class ClassBase
  {
  private:
    ClassBase& operator= (ClassBase const& other);

  protected:
    friend class Namespace;

    lua_State* const L;
    int mutable m_stackSize;

#ifdef LUABINDINGDOC
    std::string _name;
    const Namespace* _parent;
#endif

  protected:
    //--------------------------------------------------------------------------
    /**
      __index metamethod for a class.

      This implements member functions, data members, and property members.
      Functions are stored in the metatable and const metatable. Data members
      and property members are in the __propget table.

      If the key is not found, the search proceeds up the hierarchy of base
      classes.
    */
    static int indexMetaMethod (lua_State* L)
    {
      int result = 0;

      assert (lua_isuserdata (L, 1));               // warn on security bypass
      lua_getmetatable (L, 1);                      // get metatable for object
      for (;;)
      {
        lua_pushvalue (L, 2);                       // push key arg2
        lua_rawget (L, -2);                         // lookup key in metatable
        if (lua_iscfunction (L, -1))                // ensure its a cfunction
        {
          lua_remove (L, -2);                       // remove metatable
          result = 1;
          break;
        }
        else if (lua_isnil (L, -1))
        {
          lua_pop (L, 1);
        }
        else
        {
          lua_pop (L, 2);
          throw std::logic_error ("not a cfunction");
        }

        rawgetfield (L, -1, "__propget");           // get __propget table
        if (lua_istable (L, -1))                    // ensure it is a table
        {
          lua_pushvalue (L, 2);                     // push key arg2
          lua_rawget (L, -2);                       // lookup key in __propget
          lua_remove (L, -2);                       // remove __propget
          if (lua_iscfunction (L, -1))              // ensure its a cfunction
          {
            lua_remove (L, -2);                     // remove metatable
            lua_pushvalue (L, 1);                   // push class arg1
            lua_call (L, 1, 1);
            result = 1;
            break;
          }
          else if (lua_isnil (L, -1))
          {
            lua_pop (L, 1);
          }
          else
          {
            lua_pop (L, 2);

            // We only put cfunctions into __propget.
            throw std::logic_error ("not a cfunction");
          }
        }
        else
        {
          lua_pop (L, 2);

          // __propget is missing, or not a table.
          throw std::logic_error ("missing __propget table");
        }

        // Repeat the lookup in the __parent metafield,
        // or return nil if the field doesn't exist.
        rawgetfield (L, -1, "__parent");
        if (lua_istable (L, -1))
        {
          // Remove metatable and repeat the search in __parent.
          lua_remove (L, -2);
        }
        else if (lua_isnil (L, -1))
        {
          result = 1;
          break;
        }
        else
        {
          lua_pop (L, 2);

          throw std::logic_error ("__parent is not a table");
        }
      }

      return result;
    }

    //--------------------------------------------------------------------------
    /**
      __newindex metamethod for classes.

      This supports writable variables and properties on class objects. The
      corresponding object is passed in the first parameter to the set function.
    */
    static int newindexMetaMethod (lua_State* L)
    {
      int result = 0;

      lua_getmetatable (L, 1);

      for (;;)
      {
        // Check __propset
        rawgetfield (L, -1, "__propset");
        if (!lua_isnil (L, -1))
        {
          lua_pushvalue (L, 2);
          lua_rawget (L, -2);
          if (!lua_isnil (L, -1))
          {
            // found it, call the setFunction.
            assert (lua_isfunction (L, -1));
            lua_pushvalue (L, 1);
            lua_pushvalue (L, 3);
            lua_call (L, 2, 0);
            result = 0;
            break;
          }
          lua_pop (L, 1);
        }
        lua_pop (L, 1);

        // Repeat the lookup in the __parent metafield.
        rawgetfield (L, -1, "__parent");
        if (lua_isnil (L, -1))
        {
          // Either the property or __parent must exist.
          result = luaL_error (L,
            "no member named '%s'", lua_tostring (L, 2));
        }
        lua_remove (L, -2);
      }

      return result;
    }

    //--------------------------------------------------------------------------
    /**
      Create the const table.
    */
    void createConstTable (char const* name)
    {
      lua_newtable (L);
      lua_pushvalue (L, -1);
      lua_setmetatable (L, -2);
      lua_pushboolean (L, 1);
      lua_rawsetp (L, -2, getIdentityKey ());
      lua_pushstring (L, (std::string ("const ") + name).c_str ());
      rawsetfield (L, -2, "__type");
      lua_pushcfunction (L, &indexMetaMethod);
      rawsetfield (L, -2, "__index");
      lua_pushcfunction (L, &newindexMetaMethod);
      rawsetfield (L, -2, "__newindex");
      lua_newtable (L);
      rawsetfield (L, -2, "__propget");

      if (Security::hideMetatables ())
      {
        lua_pushboolean (L, false);
        rawsetfield (L, -2, "__metatable");
      }
    }

    //--------------------------------------------------------------------------
    /**
      Create the class table.

      The Lua stack should have the const table on top.
    */
    void createClassTable (char const* name)
    {
      lua_newtable (L);
      lua_pushvalue (L, -1);
      lua_setmetatable (L, -2);
      lua_pushboolean (L, 1);
      lua_rawsetp (L, -2, getIdentityKey ());
      lua_pushstring (L, name);
      rawsetfield (L, -2, "__type");
      lua_pushcfunction (L, &indexMetaMethod);
      rawsetfield (L, -2, "__index");
      lua_pushcfunction (L, &newindexMetaMethod);
      rawsetfield (L, -2, "__newindex");
      lua_newtable (L);
      rawsetfield (L, -2, "__propget");
      lua_newtable (L);
      rawsetfield (L, -2, "__propset");

      lua_pushvalue (L, -2);
      rawsetfield (L, -2, "__const"); // point to const table

      lua_pushvalue (L, -1);
      rawsetfield (L, -3, "__class"); // point const table to class table

      if (Security::hideMetatables ())
      {
        lua_pushboolean (L, false);
        rawsetfield (L, -2, "__metatable");
      }
    }

    //--------------------------------------------------------------------------
    /**
      Create the static table.

      The Lua stack should have:
        -1 class table
        -2 const table
        -3 enclosing namespace
    */
    void createStaticTable (char const* name)
    {
      lua_newtable (L);
      lua_newtable (L);
      lua_pushvalue (L, -1);
      lua_setmetatable (L, -3);
      lua_insert (L, -2);
      rawsetfield (L, -5, name);

#if 0
      lua_pushlightuserdata (L, this);
      lua_pushcclosure (L, &tostringMetaMethod, 1);
      rawsetfield (L, -2, "__tostring");
#endif
      lua_pushcfunction (L, &CFunc::indexMetaMethod);
      rawsetfield (L, -2, "__index");
      lua_pushcfunction (L, &CFunc::newindexMetaMethod);
      rawsetfield (L, -2, "__newindex");
      lua_newtable (L);
      rawsetfield (L, -2, "__propget");
      lua_newtable (L);
      rawsetfield (L, -2, "__propset");

      lua_pushvalue (L, -2);
      rawsetfield (L, -2, "__class"); // point to class table

      if (Security::hideMetatables ())
      {
        lua_pushboolean (L, false);
        rawsetfield (L, -2, "__metatable");
      }
    }

    //==========================================================================
    /**
      lua_CFunction to construct a class object wrapped in a container.
    */
    template <class Params, class C>
    static int ctorContainerProxy (lua_State* L)
    {
      typedef typename ContainerTraits <C>::Type T;
      ArgList <Params, 2> args (L);
      T* const p = Constructor <T, Params>::call (args);
      UserdataSharedHelper <C, false>::push (L, p);
      return 1;
    }

    //--------------------------------------------------------------------------
    /**
      lua_CFunction to construct a class object in-place in the userdata.
    */
    template <class Params, class T>
    static int ctorPlacementProxy (lua_State* L)
    {
      ArgList <Params, 2> args (L);
      Constructor <T, Params>::call (UserdataValue <T>::place (L), args);
      return 1;
    }

    template <class Params, class T, class C>
    static int ctorPtrPlacementProxy (lua_State* L)
    {
      ArgList <Params, 2> args (L);
      T newobject (Constructor <C, Params>::call (args));
      Stack<T>::push (L, newobject);
      return 1;
    }

    template <class T>
    static int ctorNilPtrPlacementProxy (lua_State* L)
    {
      const T* newobject = new T ();
      Stack<T>::push (L, *newobject);
      return 1;
    }

    //--------------------------------------------------------------------------
    /**
      Pop the Lua stack.
    */
    void pop (int n) const
    {
      if (m_stackSize >= n && lua_gettop (L) >= n)
      {
        lua_pop (L, n);
        m_stackSize -= n;
      }
      else
      {
        throw std::logic_error ("invalid stack");
      }
    }

  public:
    //--------------------------------------------------------------------------
    explicit ClassBase (lua_State* L_)
      : L (L_)
      , m_stackSize (0)
    {
    }

    //--------------------------------------------------------------------------
    /**
      Copy Constructor.
    */
    ClassBase (ClassBase const& other)
      : L (other.L)
      , m_stackSize (0)
#ifdef LUABINDINGDOC
      , _name (other._name)
      , _parent (other._parent)
#endif
    {
      m_stackSize = other.m_stackSize;
      other.m_stackSize = 0;
    }

    ~ClassBase ()
    {
      pop (m_stackSize);
    }
  };

  //============================================================================
  //
  // Class
  //
  //============================================================================
  /**
    Provides a class registration in a lua_State.

    After contstruction the Lua stack holds these objects:
      -1 static table
      -2 class table
      -3 const table
      -4 (enclosing namespace)
  */
  template <class T>
  class Class : virtual public ClassBase
  {
  public:
    //==========================================================================
    /**
      Register a new class or add to an existing class registration.
    */
    Class (char const* name, Namespace const* parent) : ClassBase (parent->L)
    {
#ifdef LUABINDINGDOC
      _parent = parent;
      _name = parent->_name + name + ":";
#endif
      PRINTDOC ("[C] Class", parent->_name << name, std::string(), type_name <T>())
      m_stackSize = parent->m_stackSize + 3;
      parent->m_stackSize = 0;

      assert (lua_istable (L, -1));
      rawgetfield (L, -1, name);

      if (lua_isnil (L, -1))
      {
        lua_pop (L, 1);

        createConstTable (name);
        lua_pushcfunction (L, &CFunc::gcMetaMethod <T>);
        rawsetfield (L, -2, "__gc");
        lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
        rawsetfield (L, -2, "__eq");

        createClassTable (name);
        lua_pushcfunction (L, &CFunc::gcMetaMethod <T>);
        rawsetfield (L, -2, "__gc");
        lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
        rawsetfield (L, -2, "__eq");

        createStaticTable (name);

        // Map T back to its tables.
        lua_pushvalue (L, -1);
        lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getStaticKey ());
        lua_pushvalue (L, -2);
        lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getClassKey ());
        lua_pushvalue (L, -3);
        lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getConstKey ());
      }
      else
      {
        lua_pop (L, 1);
        lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getStaticKey ());
        rawgetfield (L, -1, "__class");
        rawgetfield (L, -1, "__const");

        // Reverse the top 3 stack elements
        lua_insert (L, -3);
        lua_insert (L, -2);
      }
    }

    //==========================================================================
    /**
      Derive a new class.
    */
    Class (char const* name, Namespace const* parent, void const* const staticKey)
      : ClassBase (parent->L)
    {
#ifdef LUABINDINGDOC
      _parent = parent;
      _name = parent->_name + name + ":";
#endif
      m_stackSize = parent->m_stackSize + 3;
      parent->m_stackSize = 0;

      assert (lua_istable (L, -1));

      createConstTable (name);
      lua_pushcfunction (L, &CFunc::gcMetaMethod <T>);
      rawsetfield (L, -2, "__gc");
      lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
      rawsetfield (L, -2, "__eq");

      createClassTable (name);
      lua_pushcfunction (L, &CFunc::gcMetaMethod <T>);
      rawsetfield (L, -2, "__gc");
      lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
      rawsetfield (L, -2, "__eq");

      createStaticTable (name);

      lua_rawgetp (L, LUA_REGISTRYINDEX, staticKey);
      assert (lua_istable (L, -1));
      rawgetfield (L, -1, "__class");
      assert (lua_istable (L, -1));
      rawgetfield (L, -1, "__const");
      assert (lua_istable (L, -1));

      rawsetfield (L, -6, "__parent");
      rawsetfield (L, -4, "__parent");
      rawsetfield (L, -2, "__parent");

      lua_pushvalue (L, -1);
      lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getStaticKey ());
      lua_pushvalue (L, -2);
      lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getClassKey ());
      lua_pushvalue (L, -3);
      lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getConstKey ());
    }

    //--------------------------------------------------------------------------
    /**
      Continue registration in the enclosing namespace.
    */
    Namespace endClass ()
    {
      return Namespace (this);
    }

    //--------------------------------------------------------------------------
    /**
      Add or replace a static data member.
    */
    template <class U>
    Class <T>& addStaticData (char const* name, U* pu, bool isWritable = true)
    {
      DATADOC ("Static Data Member", name, pu)
      assert (lua_istable (L, -1));

      rawgetfield (L, -1, "__propget");
      assert (lua_istable (L, -1));
      lua_pushlightuserdata (L, pu);
      lua_pushcclosure (L, &CFunc::getVariable <U>, 1);
      rawsetfield (L, -2, name);
      lua_pop (L, 1);

      rawgetfield (L, -1, "__propset");
      assert (lua_istable (L, -1));
      if (isWritable)
      {
        lua_pushlightuserdata (L, pu);
        lua_pushcclosure (L, &CFunc::setVariable <U>, 1);
      }
      else
      {
        lua_pushstring (L, name);
        lua_pushcclosure (L, &CFunc::readOnlyError, 1);
      }
      rawsetfield (L, -2, name);
      lua_pop (L, 1);

      return *this;
    }

    //--------------------------------------------------------------------------
#if 0 // unused
    /**
      Add or replace a static property member.

      If the set function is null, the property is read-only.
    */
    template <class U>
    Class <T>& addStaticProperty (char const* name, U (*get)(), void (*set)(U) = 0)
    {
      typedef U (*get_t)();
      typedef void (*set_t)(U);

      assert (lua_istable (L, -1));

      rawgetfield (L, -1, "__propget");
      assert (lua_istable (L, -1));
      new (lua_newuserdata (L, sizeof (get))) get_t (get);
      lua_pushcclosure (L, &CFunc::Call <U (*) (void)>::f, 1);
      rawsetfield (L, -2, name);
      lua_pop (L, 1);

      rawgetfield (L, -1, "__propset");
      assert (lua_istable (L, -1));
      if (set != 0)
      {
        new (lua_newuserdata (L, sizeof (set))) set_t (set);
        lua_pushcclosure (L, &CFunc::Call <void (*) (U)>::f, 1);
      }
      else
      {
        lua_pushstring (L, name);
        lua_pushcclosure (L, &CFunc::readOnlyError, 1);
      }
      rawsetfield (L, -2, name);
      lua_pop (L, 1);

      return *this;
    }
#endif

    /**
      Add or replace a Metatable Metamethod calling a lua_CFunction.
    */
    Class <T>& addOperator (char const* name, int (*const fp)(lua_State*))
    {
      /* get metatable */
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getClassKey ());
      lua_pushcfunction (L, fp);
      rawsetfield (L, -2, name);
      lua_pop (L, 1);

      /* reset stack */
      lua_pop (L, 3);
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getStaticKey ());
      rawgetfield (L, -1, "__class");
      rawgetfield (L, -1, "__const");
      lua_insert (L, -3);
      lua_insert (L, -2);

      return *this;
    }

    /**
      Add or replace a Metamethod Metamethod, evaluate a const class-member method
    */
    template <class FP>
    Class <T>& addMetamethod (char const* name, FP const fp)
    {
      /* get metatable */
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getClassKey ());
      new (lua_newuserdata (L, sizeof (FP))) FP (fp);
      lua_pushcclosure (L, &CFunc::CallConstMember <FP>::f, 1);
      rawsetfield (L, -2, name);
      lua_pop (L, 1);

      /* reset stack */
      lua_pop (L, 3);
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getStaticKey ());
      rawgetfield (L, -1, "__class");
      rawgetfield (L, -1, "__const");
      lua_insert (L, -3);
      lua_insert (L, -2);

      return *this;
    }

    //--------------------------------------------------------------------------
    /**
      Add or replace a static member function.
    */
    template <class FP>
    Class <T>& addStaticFunction (char const* name, FP const fp)
    {
      FUNDOC ("Static Member Function", name, FP)
      new (lua_newuserdata (L, sizeof (fp))) FP (fp);
      lua_pushcclosure (L, &CFunc::Call <FP>::f, 1);
      rawsetfield (L, -2, name);

      return *this;
    }

    //--------------------------------------------------------------------------
    /**
      Add or replace a lua_CFunction.
    */
    Class <T>& addStaticCFunction (char const* name, int (*const fp)(lua_State*))
    {
      DATADOC ("Static C Function", name, fp)
      lua_pushcfunction (L, fp);
      rawsetfield (L, -2, name);
      return *this;
    }

    //--------------------------------------------------------------------------
    /**
      Add or replace a data member.
    */
    template <class U>
    Class <T>& addData (char const* name, const U T::* mp, bool isWritable = true)
    {
      DATADOC ("Data Member", name, mp)
      typedef const U T::*mp_t;

      // Add to __propget in class and const tables.
      {
        rawgetfield (L, -2, "__propget");
        rawgetfield (L, -4, "__propget");
        new (lua_newuserdata (L, sizeof (mp_t))) mp_t (mp);
        lua_pushcclosure (L, &CFunc::getProperty <T,U>, 1);
        lua_pushvalue (L, -1);
        rawsetfield (L, -4, name);
        rawsetfield (L, -2, name);
        lua_pop (L, 2);
      }

      if (isWritable)
      {
        // Add to __propset in class table.
        rawgetfield (L, -2, "__propset");
        assert (lua_istable (L, -1));
        new (lua_newuserdata (L, sizeof (mp_t))) mp_t (mp);
        lua_pushcclosure (L, &CFunc::setProperty <T,U>, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);
      }

      return *this;
    }

    //--------------------------------------------------------------------------
    /**
      Add or replace a property member.
    */
    template <class TG, class TS>
    Class <T>& addProperty (char const* name, TG (T::* get) () const, bool (T::* set) (TS))
    {
      DATADOC ("Property", name, get)
      // Add to __propget in class and const tables.
      {
        rawgetfield (L, -2, "__propget");
        rawgetfield (L, -4, "__propget");
        typedef TG (T::*get_t) () const;
        new (lua_newuserdata (L, sizeof (get_t))) get_t (get);
        lua_pushcclosure (L, &CFunc::CallConstMember <get_t>::f, 1);
        lua_pushvalue (L, -1);
        rawsetfield (L, -4, name);
        rawsetfield (L, -2, name);
        lua_pop (L, 2);
      }

      {
        // Add to __propset in class table.
        rawgetfield (L, -2, "__propset");
        assert (lua_istable (L, -1));
        typedef bool (T::* set_t) (TS);
        new (lua_newuserdata (L, sizeof (set_t))) set_t (set);
        lua_pushcclosure (L, &CFunc::CallMember <set_t>::f, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);
      }

      return *this;
    }

#if 0 // unused
    // read-only
    template <class TG>
    Class <T>& addProperty (char const* name, TG (T::* get) () const)
    {
      // Add to __propget in class and const tables.
      rawgetfield (L, -2, "__propget");
      rawgetfield (L, -4, "__propget");
      typedef TG (T::*get_t) () const;
      new (lua_newuserdata (L, sizeof (get_t))) get_t (get);
      lua_pushcclosure (L, &CFunc::CallConstMember <get_t>::f, 1);
      lua_pushvalue (L, -1);
      rawsetfield (L, -4, name);
      rawsetfield (L, -2, name);
      lua_pop (L, 2);

      return *this;
    }
#endif

    //--------------------------------------------------------------------------
    /**
      Add or replace a property member, by proxy.

      When a class is closed for modification and does not provide (or cannot
      provide) the function signatures necessary to implement get or set for
      a property, this will allow non-member functions act as proxies.

      Both the get and the set functions require a T const* and T* in the first
      argument respectively.
    */
    template <class TG, class TS>
    Class <T>& addProperty (char const* name, TG (*get) (T const*), bool (*set) (T*, TS))
    {
      // Add to __propget in class and const tables.
      {
        rawgetfield (L, -2, "__propget");
        rawgetfield (L, -4, "__propget");
        typedef TG (*get_t) (T const*);
        new (lua_newuserdata (L, sizeof (get_t))) get_t (get);
        lua_pushcclosure (L, &CFunc::Call <get_t>::f, 1);
        lua_pushvalue (L, -1);
        rawsetfield (L, -4, name);
        rawsetfield (L, -2, name);
        lua_pop (L, 2);
      }

      if (set != 0)
      {
        // Add to __propset in class table.
        rawgetfield (L, -2, "__propset");
        assert (lua_istable (L, -1));
        typedef void (*set_t) (T*, TS);
        new (lua_newuserdata (L, sizeof (set_t))) set_t (set);
        lua_pushcclosure (L, &CFunc::Call <set_t>::f, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);
      }

      return *this;
    }

#if 0 // unused
    // read-only
    template <class TG, class TS>
    Class <T>& addProperty (char const* name, TG (*get) (T const*))
    {
      // Add to __propget in class and const tables.
      rawgetfield (L, -2, "__propget");
      rawgetfield (L, -4, "__propget");
      typedef TG (*get_t) (T const*);
      new (lua_newuserdata (L, sizeof (get_t))) get_t (get);
      lua_pushcclosure (L, &CFunc::Call <get_t>::f, 1);
      lua_pushvalue (L, -1);
      rawsetfield (L, -4, name);
      rawsetfield (L, -2, name);
      lua_pop (L, 2);

      return *this;
    }
#endif
    //--------------------------------------------------------------------------
    /**
        Add or replace a member function.
    */
    template <class MemFn>
    Class <T>& addFunction (char const* name, MemFn mf)
    {
      FUNDOC("Member Function", name, MemFn)
      CFunc::CallMemberFunctionHelper <MemFn, FuncTraits <MemFn>::isConstMemberFunction>::add (L, name, mf);
      return *this;
    }

    template <class MemFn>
    Class <T>& addPtrFunction (char const* name, MemFn mf)
    {
      FUNDOC("Member Pointer Function", name, MemFn)
      CFunc::CallMemberPtrFunctionHelper <MemFn>::add (L, name, mf);
      return *this;
    }

    template <class MemFn>
    Class <T>& addWPtrFunction (char const* name, MemFn mf)
    {
      FUNDOC("Member Weak Pointer Function", name, MemFn)
      CFunc::CallMemberWPtrFunctionHelper <MemFn>::add (L, name, mf);
      return *this;
    }

    template <class MemFn>
    Class <T>& addRefFunction (char const* name, MemFn mf)
    {
      FUNDOC("Member Function RefReturn", name, MemFn)
      CFunc::CallMemberRefFunctionHelper <MemFn, FuncTraits <MemFn>::isConstMemberFunction>::add (L, name, mf);
      return *this;
    }


    //--------------------------------------------------------------------------
    /**
        Add or replace a member lua_CFunction.
    */
    Class <T>& addCFunction (char const* name, int (T::*mfp)(lua_State*))
    {
      DATADOC ("C Function", name, mfp)
      typedef int (T::*MFP)(lua_State*);
      assert (lua_istable (L, -1));
      new (lua_newuserdata (L, sizeof (mfp))) MFP (mfp);
      lua_pushcclosure (L, &CFunc::CallMemberCFunction <T>::f, 1);
      rawsetfield (L, -3, name); // class table

      return *this;
    }

    // custom callback - extend existing classes
    // with non-class member functions (e.g STL iterator)
    Class <T>& addExtCFunction (char const* name, int (*const fp)(lua_State*))
    {
      DATADOC ("Ext C Function", name, fp)
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, fp, 0);
      lua_pushvalue (L, -1);
      rawsetfield (L, -5, name); // const table
      rawsetfield (L, -3, name); // class table
      return *this;
    }

    //--------------------------------------------------------------------------
    /**
        Add or replace a const member lua_CFunction.
    */
    Class <T>& addCFunction (char const* name, int (T::*mfp)(lua_State*) const)
    {
      DATADOC ("Const C Member Function", name, mfp)
      typedef int (T::*MFP)(lua_State*) const;
      assert (lua_istable (L, -1));
      new (lua_newuserdata (L, sizeof (mfp))) MFP (mfp);
      lua_pushcclosure (L, &CFunc::CallConstMemberCFunction <T>::f, 1);
      lua_pushvalue (L, -1);
      rawsetfield (L, -5, name); // const table
      rawsetfield (L, -3, name); // class table

      return *this;
    }

    /**
        Add or replace a static const data
    */
    template <typename U>
      Class <T>& addConst (char const* name, const U val)
      {
        DATADOC ("Constant/Enum Member", name, val)
        assert (lua_istable (L, -1));

        rawgetfield (L, -1, "__propget"); // static
        new (lua_newuserdata (L, sizeof (val))) U (val);
        lua_pushcclosure (L, &CFunc::getConst <U>, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);

        rawgetfield (L, -1, "__propset"); // static
        lua_pushstring (L, name);
        lua_pushcclosure (L, &CFunc::readOnlyError, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);
        return *this;
      }

    //--------------------------------------------------------------------------
    /**
      Add or replace a primary Constructor.

      The primary Constructor is invoked when calling the class type table
      like a function.

      The template parameter should be a function pointer type that matches
      the desired Constructor (since you can't take the address of a Constructor
      and pass it as an argument).
    */
    template <class MemFn, class C>
    Class <T>& addConstructor ()
    {
      FUNDOC("Constructor", "", MemFn)
      lua_pushcclosure (L,
        &ctorContainerProxy <typename FuncTraits <MemFn>::Params, C>, 0);
      rawsetfield(L, -2, "__call");

      return *this;
    }

    template <class MemFn>
    Class <T>& addConstructor ()
    {
      FUNDOC("Constructor", "", MemFn)
      lua_pushcclosure (L,
        &ctorPlacementProxy <typename FuncTraits <MemFn>::Params, T>, 0);
      rawsetfield(L, -2, "__call");

      return *this;
    }

    template <class MemFn, class PT>
    Class <T>& addPtrConstructor ()
    {
      FUNDOC("Constructor", "", MemFn)
      lua_pushcclosure (L,
        &ctorPtrPlacementProxy <typename FuncTraits <MemFn>::Params, T, PT>, 0);
      rawsetfield(L, -2, "__call");

      return *this;
    }

    Class <T>& addVoidConstructor ()
    {
      return addConstructor <void (*) ()> ();
    }

    template <class PT>
    Class <T>& addVoidPtrConstructor ()
    {
      return addPtrConstructor <void (*) (), PT> ();
    }

    Class <T>& addEqualCheck ()
    {
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
      rawsetfield (L, -3, "sameinstance");
      return *this;
    }

    template <class U>
    Class <T>& addCast (char const* name)
    {
      PRINTDOC("Cast", _name << name,
          type_name< U >(),
          type_name< U >() << " (" << type_name< T >() << "::*)()")

      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::CastClass <T, U>::f, 0);
      rawsetfield (L, -3, name); // class table

      lua_pushcclosure (L, &CFunc::CastConstClass <T, U>::f, 0);
      rawsetfield (L, -4, name); // const table
      return *this;
    }

  };

  /** C Array to/from table */
  template <typename T>
  class Array : virtual public ClassBase
  {
  public:
    Array (char const* name, Namespace const* parent) : ClassBase (parent->L)
    {
#ifdef LUABINDINGDOC
      _parent = parent;
      _name = parent->_name + name + ":";
#endif
      PRINTDOC ("[C] Array", parent->_name << name,
          std::string(), type_name <T>() + "*")
      PRINTDOC ("Ext C Function", _name << "array",
          std::string(""), "int (*)(lua_State*)")
      PRINTDOC ("Ext C Function", _name << "get_table",
          std::string(""), "int (*)(lua_State*)")
      PRINTDOC ("Ext C Function", _name << "set_table",
          std::string(""), "int (*)(lua_State*)")
      PRINTDOC("Member Function", _name << "offset",
          std::string(type_name <T>() + "*"), std::string(type_name <T>() + "* (*)(unsigned int)"))

      m_stackSize = parent->m_stackSize + 3;
      parent->m_stackSize = 0;

#if 0 // don't allow to duplicates handlers for same array-type
      assert (lua_istable (L, -1));
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getClassKey ());
      if (lua_istable (L, -1)) {
        lua_pushnil (L);
        lua_pushnil (L);
        return;
      }
      lua_pop (L, 1);
#endif

      assert (lua_istable (L, -1));
      rawgetfield (L, -1, name);

      if (lua_isnil (L, -1))
      {
        lua_pop (L, 1);

        // register array access in global namespace
        luaL_newmetatable (L, typeid(T).name());
        lua_pushcclosure (L, CFunc::array_index<T>, 0);
        lua_setfield(L, -2, "__index");
        lua_pushcclosure (L, CFunc::array_newindex<T>, 0);
        lua_setfield(L, -2, "__newindex");
        if (Security::hideMetatables ())
        {
          lua_pushboolean (L, false);
          rawsetfield (L, -2, "__metatable");
        }
        lua_pop (L, 1);


        createConstTable (name);
        lua_pushcfunction (L, &CFunc::gcMetaMethod <T>);
        rawsetfield (L, -2, "__gc");
        lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
        rawsetfield (L, -2, "__eq");

        createClassTable (name);
        lua_pushcfunction (L, &CFunc::gcMetaMethod <T>);
        rawsetfield (L, -2, "__gc");
        lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
        rawsetfield (L, -2, "__eq");

        createStaticTable (name);

        // Map T back to its tables.
        lua_pushvalue (L, -1);
        lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getStaticKey ());
        lua_pushvalue (L, -2);
        lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getClassKey ());
        lua_pushvalue (L, -3);
        lua_rawsetp (L, LUA_REGISTRYINDEX, ClassInfo <T>::getConstKey ());

        assert (lua_istable (L, -1));
        lua_pushcclosure (L, &CFunc::getArray <T>, 0);
        rawsetfield (L, -3, "array"); // class table

        lua_pushcclosure (L, &CFunc::getTable <T>, 0);
        rawsetfield (L, -3, "get_table"); // class table

        lua_pushcclosure (L, &CFunc::setTable <T>, 0);
        rawsetfield (L, -3, "set_table"); // class table

        lua_pushcclosure (L, &CFunc::ClassEqualCheck <T>::f, 0);
        rawsetfield (L, -3, "sameinstance");

        lua_pushcclosure (L, &CFunc::offsetArray <T>, 0);
        rawsetfield (L, -3, "offset"); // class table

      }
      else
      {
        lua_pushnil (L);
        lua_pushnil (L);
      }
    }

    Namespace endArray ()
    {
      return Namespace (this);
    }
  };

  /** Boost Weak & Shared Pointer Class Wrapper */
  template <class T>
  class WSPtrClass : virtual public ClassBase
  {
  public:
    WSPtrClass (char const* name, Namespace const* parent)
      : ClassBase (parent->L)
      , shared (name, parent)
      , weak (name, parent)
    {
#ifdef LUABINDINGDOC
      _parent = parent;
      _name = parent->_name + name + ":";
#endif
      PRINTDOC ("[C] Weak/Shared Pointer Class",
          parent->_name + name,
          std::string(), type_name <T>())
      m_stackSize = shared.m_stackSize;
      parent->m_stackSize = weak.m_stackSize = shared.m_stackSize = 0;
      lua_pop (L, 3);
    }

    WSPtrClass (char const* name, Namespace const* parent, void const* const sharedkey, void const* const weakkey)
      : ClassBase (parent->L)
      , shared (name, parent, sharedkey)
      , weak (name, parent, weakkey)
    {
#ifdef LUABINDINGDOC
      _parent = parent;
      _name = parent->_name + name + ":";
#endif
      m_stackSize = shared.m_stackSize;
      parent->m_stackSize = weak.m_stackSize = shared.m_stackSize = 0;
      lua_pop (L, 3);
    }

    template <class MemFn>
    WSPtrClass <T>& addFunction (char const* name, MemFn mf)
    {
      FUNDOC ("Weak/Shared Pointer Function", name, MemFn)
      set_shared_class ();
      CFunc::CallMemberPtrFunctionHelper <MemFn>::add (L, name, mf);

      set_weak_class ();
      CFunc::CallMemberWPtrFunctionHelper <MemFn>::add (L, name, mf);
      return *this;
    }

    template <class MemFn>
    WSPtrClass <T>& addRefFunction (char const* name, MemFn mf)
    {
      FUNDOC ("Weak/Shared Pointer Function RefReturn", name, MemFn)
      set_shared_class ();
      CFunc::CallMemberRefPtrFunctionHelper <MemFn>::add (L, name, mf);

      set_weak_class ();
      CFunc::CallMemberRefWPtrFunctionHelper <MemFn>::add (L, name, mf);
      return *this;
    }

    template <class MemFn>
    WSPtrClass <T>& addConstructor ()
    {
      FUNDOC ("Weak/Shared Pointer Constructor", "", MemFn)
      set_shared_class ();
      lua_pushcclosure (L,
          &shared. template ctorPtrPlacementProxy <typename FuncTraits <MemFn>::Params, boost::shared_ptr<T>, T >, 0);
      rawsetfield(L, -2, "__call");

      set_weak_class ();
      // NOTE: this constructs an empty weak-ptr,
      // ideally we'd construct a weak-ptr from a referenced shared-ptr
      lua_pushcclosure (L,
          &weak. template ctorPlacementProxy <typename FuncTraits <MemFn>::Params, boost::weak_ptr<T> >, 0);
      rawsetfield(L, -2, "__call");
      return *this;
    }

    WSPtrClass <T>& addVoidConstructor ()
    {
      return addConstructor <void (*) ()> ();
    }

    template <class FP>
    WSPtrClass <T>& addStaticFunction (char const* name, FP const fp)
    {
      FUNDOC ("Static Member Function", name, FP)
      set_shared_class ();
      new (lua_newuserdata (L, sizeof (fp))) FP (fp);
      lua_pushcclosure (L, &CFunc::Call <FP>::f, 1);
      rawsetfield (L, -2, name);

      set_weak_class ();
      new (lua_newuserdata (L, sizeof (fp))) FP (fp);
      lua_pushcclosure (L, &CFunc::Call <FP>::f, 1);
      rawsetfield (L, -2, name);
      return *this;
    }

    WSPtrClass <T>& addNilPtrConstructor ()
    {
      FUNDOC ("Weak/Shared Pointer NIL Constructor", "", void (*) ())
      set_shared_class ();
      lua_pushcclosure (L,
          &shared. template ctorNilPtrPlacementProxy <boost::shared_ptr<T> >, 0);
      rawsetfield(L, -2, "__call");

      set_weak_class ();
      // NOTE: this constructs an empty weak-ptr,
      // ideally we'd construct a weak-ptr from a referenced shared-ptr
      lua_pushcclosure (L,
          &weak. template ctorNilPtrPlacementProxy <boost::weak_ptr<T> >, 0);
      rawsetfield(L, -2, "__call");

      return *this;
    }

    WSPtrClass <T>& addExtCFunction (char const* name, int (*const fp)(lua_State*))
    {
      DATADOC ("Weak/Shared Ext C Function", name, fp)
      set_shared_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, fp, 0);
      lua_pushvalue (L, -1);
      rawsetfield (L, -5, name); // const table
      rawsetfield (L, -3, name); // class table

      set_weak_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, fp, 0);
      lua_pushvalue (L, -1);
      rawsetfield (L, -5, name); // const table
      rawsetfield (L, -3, name); // class table

      return *this;
    }

    template <class U>
    WSPtrClass <T>& addCast (char const* name)
    {
      PRINTDOC("Weak/Shared Pointer Cast", _name << name,
          type_name< U >(),
          type_name< U >() << " (" << type_name< T >() << "::*)()")

      // TODO weak ptr
      set_shared_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::CastMemberPtr <T, U>::f, 0);
      rawsetfield (L, -3, name); // class table
      return *this;
    }

    WSPtrClass <T>& addNullCheck ()
    {
      PRINTDOC("Weak/Shared Null Check", _name << "isnil", std::string("bool"), std::string("void (*)()"))
      set_shared_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::PtrNullCheck <T>::f, 0);
      rawsetfield (L, -3, "isnil"); // class table

      set_weak_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::WPtrNullCheck <T>::f, 0);
      rawsetfield (L, -3, "isnil"); // class table

      return *this;
    }

    WSPtrClass <T>& addEqualCheck ()
    {
      set_shared_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::PtrEqualCheck <T>::f, 0);
      rawsetfield (L, -3, "sameinstance"); // class table

      set_weak_class ();
      assert (lua_istable (L, -1));
      lua_pushcclosure (L, &CFunc::WPtrEqualCheck <T>::f, 0);
      rawsetfield (L, -3, "sameinstance"); // class table

      return *this;
    }

    template <class U>
    WSPtrClass <T>& addData (char const* name, const U T::* mp, bool isWritable = true)
    {
      DATADOC ("Data Member", name, mp)
      typedef const U T::*mp_t;

      set_weak_class ();
      assert (lua_istable (L, -1));
      // Add to __propget in class and const tables.
      {
        rawgetfield (L, -2, "__propget");
        rawgetfield (L, -4, "__propget");
        new (lua_newuserdata (L, sizeof (mp_t))) mp_t (mp);
        lua_pushcclosure (L, &CFunc::getWPtrProperty <T,U>, 1);
        lua_pushvalue (L, -1);
        rawsetfield (L, -4, name);
        rawsetfield (L, -2, name);
        lua_pop (L, 2);
      }

      if (isWritable)
      {
        // Add to __propset in class table.
        rawgetfield (L, -2, "__propset");
        assert (lua_istable (L, -1));
        new (lua_newuserdata (L, sizeof (mp_t))) mp_t (mp);
        lua_pushcclosure (L, &CFunc::setWPtrProperty <T,U>, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);
      }

      set_shared_class ();
      assert (lua_istable (L, -1));
      // Add to __propget in class and const tables.
      {
        rawgetfield (L, -2, "__propget");
        rawgetfield (L, -4, "__propget");
        new (lua_newuserdata (L, sizeof (mp_t))) mp_t (mp);
        lua_pushcclosure (L, &CFunc::getPtrProperty <T,U>, 1);
        lua_pushvalue (L, -1);
        rawsetfield (L, -4, name);
        rawsetfield (L, -2, name);
        lua_pop (L, 2);
      }

      if (isWritable)
      {
        // Add to __propset in class table.
        rawgetfield (L, -2, "__propset");
        assert (lua_istable (L, -1));
        new (lua_newuserdata (L, sizeof (mp_t))) mp_t (mp);
        lua_pushcclosure (L, &CFunc::setPtrProperty <T,U>, 1);
        rawsetfield (L, -2, name);
        lua_pop (L, 1);
      }

      return *this;
    }


    Namespace endClass ()
    {
      return Namespace (this);
    }

  private:
    void set_weak_class () {
      lua_pop (L, 3);
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <boost::weak_ptr<T> >::getStaticKey ());
      rawgetfield (L, -1, "__class");
      rawgetfield (L, -1, "__const");
      lua_insert (L, -3);
      lua_insert (L, -2);
    }
    void set_shared_class () {
      lua_pop (L, 3);
      lua_rawgetp (L, LUA_REGISTRYINDEX, ClassInfo <boost::shared_ptr<T> >::getStaticKey ());
      rawgetfield (L, -1, "__class");
      rawgetfield (L, -1, "__const");
      lua_insert (L, -3);
      lua_insert (L, -2);
    }
    Class<boost::shared_ptr<T> > shared;
    Class<boost::weak_ptr<T> > weak;
  };


private:
  //----------------------------------------------------------------------------
  /**
      Open the global namespace for registrations.
  */
  explicit Namespace (lua_State* L_)
    : L (L_)
    , m_stackSize (0)
#ifdef LUABINDINGDOC
    , _name ("")
    , _parent (0)
#endif
  {
    lua_getglobal (L, "_G");
    ++m_stackSize;
  }

#ifdef LUABINDINGDOC
  std::string _name;
  Namespace const * _parent;
#endif

  //----------------------------------------------------------------------------
  /**
      Open a namespace for registrations.

      The namespace is created if it doesn't already exist.
      The parent namespace is at the top of the Lua stack.
  */
  Namespace (char const* name, Namespace const* parent)
    : L (parent->L)
    , m_stackSize (0)
#ifdef LUABINDINGDOC
    , _name (parent->_name + name + ":")
    , _parent (parent)
#endif
  {
    m_stackSize = parent->m_stackSize + 1;
    parent->m_stackSize = 0;

    assert (lua_istable (L, -1));
    rawgetfield (L, -1, name);
    if (lua_isnil (L, -1))
    {
      lua_pop (L, 1);

      lua_newtable (L);
      lua_pushvalue (L, -1);
      lua_setmetatable (L, -2);
      lua_pushcfunction (L, &CFunc::indexMetaMethod);
      rawsetfield (L, -2, "__index");
      lua_pushcfunction (L, &CFunc::newindexMetaMethod);
      rawsetfield (L, -2, "__newindex");
      lua_newtable (L);
      rawsetfield (L, -2, "__propget");
      lua_newtable (L);
      rawsetfield (L, -2, "__propset");
      lua_pushvalue (L, -1);
      rawsetfield (L, -3, name);
#if 0
      lua_pushcfunction (L, &tostringMetaMethod);
      rawsetfield (L, -2, "__tostring");
#endif
      if (Security::hideMetatables ())
      {
        lua_pushboolean (L, false);
        rawsetfield (L, -2, "__metatable");
      }

    }
  }

  //----------------------------------------------------------------------------
  /**
      Creates a continued registration from a child namespace.
  */
  explicit Namespace (Namespace const* child)
    : L (child->L)
    , m_stackSize (0)
#ifdef LUABINDINGDOC
    , _name (child->_parent ? child->_parent->_name : "")
    , _parent (child->_parent ? child->_parent->_parent : NULL)
#endif
  {
    m_stackSize = child->m_stackSize - 1;
    child->m_stackSize = 1;
    child->pop (1);

    // It is not necessary or valid to call
    // endNamespace() for the global namespace!
    //
    assert (m_stackSize != 0);
  }

  //----------------------------------------------------------------------------
  /**
      Creates a continued registration from a child class.
  */
  explicit Namespace (ClassBase const* child)
    : L (child->L)
    , m_stackSize (0)
#ifdef LUABINDINGDOC
    , _name (child->_parent ? child->_parent->_name : "")
    , _parent (child->_parent ? child->_parent->_parent : NULL)
#endif
  {
    m_stackSize = child->m_stackSize - 3;
    child->m_stackSize = 3;
    child->pop (3);
  }

public:
  //----------------------------------------------------------------------------
  /**
      Copy Constructor.

      Ownership of the stack is transferred to the new object. This happens
      when the compiler emits temporaries to hold these objects while chaining
      registrations across namespaces.
  */
  Namespace (Namespace const& other) : L (other.L)
  {
    m_stackSize = other.m_stackSize;
    other.m_stackSize = 0;
#ifdef LUABINDINGDOC
    _name = other._name;
    _parent = other._parent;
#endif
  }

  //----------------------------------------------------------------------------
  /**
      Closes this namespace registration.
  */
  ~Namespace ()
  {
    pop (m_stackSize);
  }

  //----------------------------------------------------------------------------
  /**
      Open the global namespace.
  */
  static Namespace getGlobalNamespace (lua_State* L)
  {
    return Namespace (L);
  }

  //----------------------------------------------------------------------------
  /**
      Open a new or existing namespace for registrations.
  */
  Namespace beginNamespace (char const* name)
  {
    return Namespace (name, this);
  }

  //----------------------------------------------------------------------------
  /**
      Continue namespace registration in the parent.

      Do not use this on the global namespace.
  */
  Namespace endNamespace ()
  {
    return Namespace (this);
  }

  //----------------------------------------------------------------------------
  /**
      Add or replace a variable.
  */
  template <class T>
  Namespace& addVariable (char const* name, T* pt, bool isWritable = true)
  {
    assert (lua_istable (L, -1));

    rawgetfield (L, -1, "__propget");
    assert (lua_istable (L, -1));
    lua_pushlightuserdata (L, pt);
    lua_pushcclosure (L, &CFunc::getVariable <T>, 1);
    rawsetfield (L, -2, name);
    lua_pop (L, 1);

    rawgetfield (L, -1, "__propset");
    assert (lua_istable (L, -1));
    if (isWritable)
    {
      lua_pushlightuserdata (L, pt);
      lua_pushcclosure (L, &CFunc::setVariable <T>, 1);
    }
    else
    {
      lua_pushstring (L, name);
      lua_pushcclosure (L, &CFunc::readOnlyError, 1);
    }
    rawsetfield (L, -2, name);
    lua_pop (L, 1);

    return *this;
  }

  template <typename U>
  Namespace& addConst (char const* name, const U val)
  {
    DATADOC ("Constant/Enum", name, val)
    assert (lua_istable (L, -1));
    rawgetfield (L, -1, "__propget");
    new (lua_newuserdata (L, sizeof (val))) U (val);
    lua_pushcclosure (L, &CFunc::getConst <U>, 1);
    rawsetfield (L, -2, name);
    lua_pop (L, 1);

    rawgetfield (L, -1, "__propset");
    assert (lua_istable (L, -1));
    lua_pushstring (L, name);
    lua_pushcclosure (L, &CFunc::readOnlyError, 1);
    rawsetfield (L, -2, name);
    lua_pop (L, 1);
    return *this;
  }

  //----------------------------------------------------------------------------
  /**
      Add or replace a property.

      If the set function is omitted or null, the property is read-only.
  */
#if 0 // unused
  template <class TG, class TS>
  Namespace& addProperty (char const* name, TG (*get) (), void (*set)(TS) = 0)
  {
    assert (lua_istable (L, -1));

    rawgetfield (L, -1, "__propget");
    assert (lua_istable (L, -1));
    typedef TG (*get_t) ();
    new (lua_newuserdata (L, sizeof (get_t))) get_t (get);
    lua_pushcclosure (L, &CFunc::Call <TG (*) (void)>::f, 1);
    rawsetfield (L, -2, name);
    lua_pop (L, 1);

    rawgetfield (L, -1, "__propset");
    assert (lua_istable (L, -1));
    if (set != 0)
    {
      typedef void (*set_t) (TS);
      new (lua_newuserdata (L, sizeof (set_t))) set_t (set);
      lua_pushcclosure (L, &CFunc::Call <void (*) (TS)>::f, 1);
    }
    else
    {
      lua_pushstring (L, name);
      lua_pushcclosure (L, &CFunc::readOnlyError, 1);
    }
    rawsetfield (L, -2, name);
    lua_pop (L, 1);

    return *this;
  }
#endif

  //----------------------------------------------------------------------------
  /**
      Add or replace a free function.
  */
  template <class FP>
  Namespace& addFunction (char const* name, FP const fp)
  {
    FUNDOC ("Free Function", name, FP)
    assert (lua_istable (L, -1));

    new (lua_newuserdata (L, sizeof (fp))) FP (fp);
    lua_pushcclosure (L, &CFunc::Call <FP>::f, 1);
    rawsetfield (L, -2, name);

    return *this;
  }

  template <class FP>
  Namespace& addRefFunction (char const* name, FP const fp)
  {
    FUNDOC ("Free Function RefReturn", name, FP)
    assert (lua_istable (L, -1));

    new (lua_newuserdata (L, sizeof (fp))) FP (fp);
    lua_pushcclosure (L, &CFunc::CallRef <FP>::f, 1);
    rawsetfield (L, -2, name);

    return *this;
  }

  //----------------------------------------------------------------------------
  /**
      Add or replace a array type
  */

  template <typename T>
  Namespace registerArray (char const* name)
  {
    return Array <T> (name, this).endArray();
  }


  //----------------------------------------------------------------------------
  /**
      Add or replace a lua_CFunction.
  */
  Namespace& addCFunction (char const* name, int (*const fp)(lua_State*))
  {
    DATADOC ("Free C Function", name, fp)
    lua_pushcfunction (L, fp);
    rawsetfield (L, -2, name);

    return *this;
  }

  //----------------------------------------------------------------------------
  /**
      Open a new or existing class for registrations.
  */
  template <class T>
  Class <T> beginClass (char const* name)
  {
    return Class <T> (name, this);
  }

  /** weak & shared pointer class */
  template <class T>
  WSPtrClass <T> beginWSPtrClass (char const* name)
  {
    return WSPtrClass <T> (name, this)
      .addNullCheck()
      .addEqualCheck();
  }

  //----------------------------------------------------------------------------

  template <class K, class V>
  Class<std::map<K, V> > beginStdMap (char const* name)
  {
    typedef std::map<K, V> LT;
    typedef std::pair<const K, V> T;

    typedef typename std::map<K, V>::size_type T_SIZE;

    return beginClass<LT> (name)
      .addVoidConstructor ()
      .addFunction ("empty", (bool (LT::*)()const)&LT::empty)
      .addFunction ("size", (T_SIZE (LT::*)()const)&LT::size)
      .addFunction ("clear", (void (LT::*)())&LT::clear)
      .addFunction ("count", (T_SIZE (LT::*)(const K&) const)&LT::count)
      .addExtCFunction ("add", &CFunc::tableToMap<K, V>)
      .addExtCFunction ("iter", &CFunc::mapIter<K, V>)
      .addExtCFunction ("table", &CFunc::mapToTable<K, V>)
      .addExtCFunction ("at", &CFunc::mapAt<K, V>);
  }

  template <class T>
  Class<std::set<T> > beginStdSet (char const* name)
  {
    typedef std::set<T> LT;
    return beginClass<LT> (name)
      .addVoidConstructor ()
      .addFunction ("clear", (void (LT::*)())&LT::clear)
      .addFunction ("empty", &LT::empty)
      .addFunction ("size", &LT::size)
      .addExtCFunction ("iter", &CFunc::setIter<T, LT>)
      .addExtCFunction ("table", &CFunc::setToTable<T, LT>);
  }

  template <unsigned int T>
  Class<std::bitset<T> > beginStdBitSet (char const* name)
  {
    typedef std::bitset<T> BS;
    return beginClass<BS> (name)
      .addVoidConstructor ()
      .addFunction ("reset", (BS& (BS::*)())&BS::reset)
      .addFunction ("set", (BS& (BS::*)(size_t, bool))&BS::set)
      .addFunction ("count", (size_t (BS::*)()const)&BS::count)
      .addFunction ("size", (size_t (BS::*)()const)&BS::size)
      .addFunction ("any", (bool (BS::*)()const)&BS::any)
      .addFunction ("none", (bool (BS::*)()const)&BS::none)
      .addFunction ("test", &BS::test)
      .addExtCFunction ("add", &CFunc::tableToBitSet<T>)
      .addExtCFunction ("table", &CFunc::bitSetToTable<T>);
  }

  template <class T>
  Class<std::list<T> > beginConstStdList (char const* name)
  {
    typedef std::list<T> LT;
    typedef typename LT::size_type T_SIZE;
    return beginClass<LT> (name)
      .addVoidConstructor ()
      .addFunction ("empty", static_cast<bool (LT::*)() const>(&LT::empty))
      .addFunction ("size", static_cast<T_SIZE (LT::*)() const>(&LT::size))
      .addFunction ("reverse", static_cast<void (LT::*)()>(&LT::reverse))
      .addFunction ("front", static_cast<T& (LT::*)()>(&LT::front))
      .addFunction ("back", static_cast<T& (LT::*)()>(&LT::back))
      .addExtCFunction ("iter", &CFunc::listIter<T, LT>)
      .addExtCFunction ("table", &CFunc::listToTable<T, LT>);
  }

  template <class T>
  Class<std::list<T> > beginStdList (char const* name)
  {
    typedef std::list<T> LT;
    typedef typename LT::size_type T_SIZE;
    return beginConstStdList<T> (name)
#if !defined(_MSC_VER) || (_MSC_VER < 1900)
	  /* std::list::unique() got broken in later versions of MSVC */
# if (defined(__cplusplus) &&  __cplusplus >= 201709L)
      .addFunction ("unique", (T_SIZE (LT::*)())&LT::unique)
# else
      .addFunction ("unique", (void (LT::*)())&LT::unique)
# endif
#endif
      .addFunction ("push_back", (void (LT::*)(const T&))&LT::push_back)
      .addExtCFunction ("add", &CFunc::tableToList<T, LT>);
  }

  template <class T>
  Class<std::list<T*> > beginConstStdCPtrList (char const* name)
  {
    typedef T* TP;
    typedef std::list<TP> LT;
    typedef typename LT::size_type T_SIZE;
    return beginClass<LT> (name)
      .addVoidConstructor ()
      .addFunction ("empty", static_cast<bool (LT::*)() const>(&LT::empty))
      .addFunction ("size", static_cast<T_SIZE (LT::*)() const>(&LT::size))
      .addFunction ("reverse", static_cast<void (LT::*)()>(&LT::reverse))
      .addFunction ("front", static_cast<const TP& (LT::*)() const>(&LT::front))
      .addFunction ("back", static_cast<const TP& (LT::*)() const>(&LT::back))
      .addExtCFunction ("iter", &CFunc::listIter<T*, LT>)
      .addExtCFunction ("table", &CFunc::listToTable<T*, LT>);
  }

  template <class T>
  Class<std::list<T*> > beginStdCPtrList (char const* name)
  {
    typedef T* TP;
    typedef std::list<TP> LT;
    typedef typename LT::size_type T_SIZE;
    return beginConstStdCPtrList<T> (name)
#if !defined(_MSC_VER) || (_MSC_VER < 1900)
	  /* std::list::unique() got broken in later versions of MSVC */
# if (defined(__cplusplus) &&  __cplusplus >= 201709L)
      .addFunction ("unique", (T_SIZE (LT::*)())&LT::unique)
# else
      .addFunction ("unique", (void (LT::*)())&LT::unique)
# endif
#endif
      .addExtCFunction ("push_back", &CFunc::pushbackptr<T, LT>);
  }


  template <class T>
  Class<std::vector<T> > beginConstStdVector (char const* name)
  {
    typedef std::vector<T> LT;
    typedef typename std::vector<T>::reference T_REF;
    typedef typename std::vector<T>::size_type T_SIZE;

    return beginClass<LT> (name)
      .addVoidConstructor ()
      .addFunction ("empty", (bool (LT::*)()const)&LT::empty)
      .addFunction ("size", (T_SIZE (LT::*)()const)&LT::size)
      .addFunction ("at", (T_REF (LT::*)(T_SIZE))&LT::at)
      .addExtCFunction ("iter", &CFunc::listIter<T, LT>)
      .addExtCFunction ("table", &CFunc::listToTable<T, LT>);
  }

  template <class T>
  Class<std::vector<T> > beginStdVector (char const* name)
  {
    typedef std::vector<T> LT;
    return beginConstStdVector<T> (name)
      .addVoidConstructor ()
      .addFunction ("push_back", (void (LT::*)(const T&))&LT::push_back)
      .addFunction ("clear", (void (LT::*)())&LT::clear)
      .addExtCFunction ("to_array", &CFunc::vectorToArray<T, LT>)
      .addExtCFunction ("add", &CFunc::tableToList<T, LT>);
  }

  //----------------------------------------------------------------------------

  template <class T>
  Class<boost::shared_ptr<std::list<T> > > beginPtrStdList (char const* name)
  {
    typedef std::list<T> LT;
    typedef typename LT::size_type T_SIZE;
    return beginClass<boost::shared_ptr<LT> > (name)
      //.addVoidPtrConstructor<LT> ()
      .addPtrFunction ("empty", (bool (LT::*)()const)&LT::empty)
      .addPtrFunction ("size", (T_SIZE (LT::*)()const)&LT::size)
      .addPtrFunction ("reverse", (void (LT::*)())&LT::reverse)
#if !defined(_MSC_VER) || (_MSC_VER < 1900)
	  /* std::list::unique() got broken in later versions of MSVC */
# if (defined(__cplusplus) &&  __cplusplus >= 201709L)
      .addPtrFunction ("unique", (T_SIZE (LT::*)())&LT::unique)
# else
      .addPtrFunction ("unique", (void (LT::*)())&LT::unique)
# endif
#endif
      .addPtrFunction ("push_back", (void (LT::*)(const T&))&LT::push_back)
      .addExtCFunction ("add", &CFunc::ptrTableToList<T, LT>)
      .addExtCFunction ("iter", &CFunc::ptrListIter<T, LT>)
      .addExtCFunction ("table", &CFunc::ptrListToTable<T, LT>);
  }

  template <class T>
  Class<boost::shared_ptr<std::vector<T> > > beginPtrStdVector (char const* name)
  {
    typedef std::vector<T> LT;
    typedef typename std::vector<T>::reference T_REF;
    typedef typename std::vector<T>::size_type T_SIZE;

    return beginClass<boost::shared_ptr<LT> > (name)
      //.addVoidPtrConstructor<LT> ()
      .addPtrFunction ("empty", (bool (LT::*)()const)&LT::empty)
      .addPtrFunction ("size", (T_SIZE (LT::*)()const)&LT::size)
      .addPtrFunction ("push_back", (void (LT::*)(const T&))&LT::push_back)
      .addPtrFunction ("at", (T_REF (LT::*)(T_SIZE))&LT::at)
      .addExtCFunction ("add", &CFunc::ptrTableToList<T, LT>)
      .addExtCFunction ("iter", &CFunc::ptrListIter<T, LT>)
      .addExtCFunction ("table", &CFunc::ptrListToTable<T, LT>);
  }

  //----------------------------------------------------------------------------
  /**
      Derive a new class for registrations.

      To continue registrations for the class later, use beginClass().
      Do not call deriveClass() again.
  */
  template <class T, class U>
  Class <T> deriveClass (char const* name)
  {
    CLASSDOC ("[C] Derived Class", _name << name, type_name <T>(), type_name <U>())
    return Class <T> (name, this, ClassInfo <U>::getStaticKey ());
  }

  template <class T, class U>
  WSPtrClass <T> deriveWSPtrClass (char const* name)
  {

    CLASSDOC ("[C] Derived Class", _name << name, type_name <boost::shared_ptr<T> >(), type_name <boost::shared_ptr<U> >())
    CLASSDOC ("[C] Derived Class", _name << name, type_name <boost::weak_ptr<T> >(), type_name <boost::weak_ptr<U> >())
    CLASSDOC ("[C] Derived Pointer Class", _name << name, type_name <T>(), type_name <U>())
    return WSPtrClass <T> (name, this,
        ClassInfo <boost::shared_ptr<U> >::getStaticKey (),
        ClassInfo <boost::weak_ptr<U> >::getStaticKey ())
      .addNullCheck()
      .addEqualCheck();
  }

};

//------------------------------------------------------------------------------
/**
    Retrieve the global namespace.

    It is recommended to put your namespace inside the global namespace, and
    then add your classes and functions to it, rather than adding many classes
    and functions directly to the global namespace.
*/
inline Namespace getGlobalNamespace (lua_State* L)
{
  return Namespace::getGlobalNamespace (L);
}


#undef KEYSTA
#undef KEYEND
#undef CLASSDOC
#undef PRINTDOC
#undef FUNDOC
#undef DATADOC

/* vim: set et sw=2: */
