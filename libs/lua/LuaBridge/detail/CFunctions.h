//------------------------------------------------------------------------------
/*
  https://github.com/vinniefalco/LuaBridge

  Copyright 2016, Robin Gareus <robin@gareus.org>
  Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>

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

// We use a structure so we can define everything in the header.
//
struct CFunc
{
  //----------------------------------------------------------------------------
  /**
      __index metamethod for a namespace or class static members.

      This handles:
        Retrieving functions and class static methods, stored in the metatable.
        Reading global and class static data, stored in the __propget table.
        Reading global and class properties, stored in the __propget table.
  */
  static int indexMetaMethod (lua_State* L)
  {
    int result = 0;
    lua_getmetatable (L, 1);                // push metatable of arg1
    for (;;)
    {
      lua_pushvalue (L, 2);                 // push key arg2
      lua_rawget (L, -2);                   // lookup key in metatable
      if (lua_isnil (L, -1))                // not found
      {
        lua_pop (L, 1);                     // discard nil
        rawgetfield (L, -1, "__propget");   // lookup __propget in metatable
        lua_pushvalue (L, 2);               // push key arg2
        lua_rawget (L, -2);                 // lookup key in __propget
        lua_remove (L, -2);                 // discard __propget
        if (lua_iscfunction (L, -1))
        {
          lua_remove (L, -2);               // discard metatable
          lua_pushvalue (L, 1);             // push arg1
          lua_call (L, 1, 1);               // call cfunction
          result = 1;
          break;
        }
        else
        {
          assert (lua_isnil (L, -1));
          lua_pop (L, 1);                   // discard nil and fall through
        }
      }
      else
      {
        assert (lua_istable (L, -1) || lua_iscfunction (L, -1));
        lua_remove (L, -2);
        result = 1;
        break;
      }

      rawgetfield (L, -1, "__parent");
      if (lua_istable (L, -1))
      {
        // Remove metatable and repeat the search in __parent.
        lua_remove (L, -2);
      }
      else
      {
        // Discard metatable and return nil.
        assert (lua_isnil (L, -1));
        lua_remove (L, -2);
        result = 1;
        break;
      }
    }

    return result;
  }

  //----------------------------------------------------------------------------
  /**
      __newindex metamethod for a namespace or class static members.

      The __propset table stores proxy functions for assignment to:
        Global and class static data.
        Global and class properties.
  */
  static int newindexMetaMethod (lua_State* L)
  {
    int result = 0;
    lua_getmetatable (L, 1);                // push metatable of arg1
    for (;;)
    {
      rawgetfield (L, -1, "__propset");     // lookup __propset in metatable
      assert (lua_istable (L, -1));
      lua_pushvalue (L, 2);                 // push key arg2
      lua_rawget (L, -2);                   // lookup key in __propset
      lua_remove (L, -2);                   // discard __propset
      if (lua_iscfunction (L, -1))          // ensure value is a cfunction
      {
        lua_remove (L, -2);                 // discard metatable
        lua_pushvalue (L, 3);               // push new value arg3
        lua_call (L, 1, 0);                 // call cfunction
        result = 0;
        break;
      }
      else
      {
        assert (lua_isnil (L, -1));
        lua_pop (L, 1);
      }

      rawgetfield (L, -1, "__parent");
      if (lua_istable (L, -1))
      {
        // Remove metatable and repeat the search in __parent.
        lua_remove (L, -2);
      }
      else
      {
        assert (lua_isnil (L, -1));
        lua_pop (L, 2);
        result = luaL_error (L,"no writable variable '%s'", lua_tostring (L, 2));
      }
    }

    return result;
  }

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to report an error writing to a read-only value.

      The name of the variable is in the first upvalue.
  */
  static int readOnlyError (lua_State* L)
  {
    std::string s;

    s = s + "'" + lua_tostring (L, lua_upvalueindex (1)) + "' is read-only";

    return luaL_error (L, s.c_str ());
  }

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to get a variable.

      This is used for global variables or class static data members.

      The pointer to the data is in the first upvalue.
  */
  template <class T>
  static int getVariable (lua_State* L)
  {
    assert (lua_islightuserdata (L, lua_upvalueindex (1)));
    T const* ptr = static_cast <T const*> (lua_touserdata (L, lua_upvalueindex (1)));
    assert (ptr != 0);
    Stack <T>::push (L, *ptr);
    return 1;
  }

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to set a variable.

      This is used for global variables or class static data members.

      The pointer to the data is in the first upvalue.
  */
  template <class T>
  static int setVariable (lua_State* L)
  {
    assert (lua_islightuserdata (L, lua_upvalueindex (1)));
    T* ptr = static_cast <T*> (lua_touserdata (L, lua_upvalueindex (1)));
    assert (ptr != 0);
    *ptr = Stack <T>::get (L, 1);
    return 0;
  }

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to call a function with a return value.

      This is used for global functions, global properties, class static methods,
      and class static properties.

      The function pointer is in the first upvalue.
  */
  template <class FnPtr,
            class ReturnType = typename FuncTraits <FnPtr>::ReturnType>
  struct Call
  {
    typedef typename FuncTraits <FnPtr>::Params Params;
    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      FnPtr const& fnptr = *static_cast <FnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params> args (L);
      Stack <typename FuncTraits <FnPtr>::ReturnType>::push (L, FuncTraits <FnPtr>::call (fnptr, args));
      return 1;
    }
  };

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to call a function with no return value.

      This is used for global functions, global properties, class static methods,
      and class static properties.

      The function pointer is in the first upvalue.
  */
  template <class FnPtr>
  struct Call <FnPtr, void>
  {
    typedef typename FuncTraits <FnPtr>::Params Params;
    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      FnPtr const& fnptr = *static_cast <FnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params> args (L);
      FuncTraits <FnPtr>::call (fnptr, args);
      return 0;
    }
  };

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to call a function with references as arguments.
  */
  template <class FnPtr,
            class ReturnType = typename FuncTraits <FnPtr>::ReturnType>
  struct CallRef
  {
    typedef typename FuncTraits <FnPtr>::Params Params;
    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      FnPtr const& fnptr = *static_cast <FnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 1> args (L);
      Stack <typename FuncTraits <FnPtr>::ReturnType>::push (L, FuncTraits <FnPtr>::call (fnptr, args));
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 2;
    }
  };

  template <class FnPtr>
  struct CallRef <FnPtr, void>
  {
    typedef typename FuncTraits <FnPtr>::Params Params;
    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      FnPtr const& fnptr = *static_cast <FnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 1> args (L);
      FuncTraits <FnPtr>::call (fnptr, args);
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 1;
    }
  };


  //----------------------------------------------------------------------------
  /**
      lua_CFunction to call a class member function with a return value.

      The member function pointer is in the first upvalue.
      The class userdata object is at the top of the Lua stack.
  */
  template <class MemFnPtr,
            class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallMember
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T* const t = Userdata::get <T> (L, 1, false);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (t, fnptr, args));
      return 1;
    }
  };

  template <class MemFnPtr,
            class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallConstMember
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T const* const t = Userdata::get <T> (L, 1, true);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args(L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (t, fnptr, args));
      return 1;
    }
  };

  template <class MemFnPtr, class T,
           class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallMemberPtr
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::shared_ptr<T>* const t = Userdata::get <boost::shared_ptr<T> > (L, 1, false);
      T* const tt = t->get();
      if (!tt) {
        return luaL_error (L, "shared_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (tt, fnptr, args));
      return 1;
    }
  };

  template <class T, class R>
  struct CastMemberPtr
  {
    static int f (lua_State* L)
    {
      boost::shared_ptr<T> t = luabridge::Stack<boost::shared_ptr<T> >::get (L, 1);
      Stack <boost::shared_ptr<R> >::push (L, boost::dynamic_pointer_cast<R> (t));
      return 1;
    }
  };

  template <class T>
  struct ClassEqualCheck
  {
    static int f (lua_State* L)
    {
      T const* const t0 = Userdata::get <T> (L, 1, true);
      T const* const t1 = Userdata::get <T> (L, 2, true);
      Stack <bool>::push (L, t0 == t1);
      return 1;
    }
  };

  template <class T, class R>
  struct CastClass
  {
    static int f (lua_State* L)
    {
      T * const t = Userdata::get <T> (L, 1, false );
      Stack <R*>::push (L, dynamic_cast<R*>(t));
      return 1;
    }
  };

  template <class T, class R>
  struct CastConstClass
  {
    static int f (lua_State* L)
    {
      T const* const t = Userdata::get <T> (L, 1, true);
      Stack <R const*>::push (L, dynamic_cast<R const*>(t));
      return 1;
    }
  };

  template <class T>
  struct PtrNullCheck
  {
    static int f (lua_State* L)
    {
      boost::shared_ptr<T> t = luabridge::Stack<boost::shared_ptr<T> >::get (L, 1);
      Stack <bool>::push (L, t == 0);
      return 1;
    }
  };

  template <class T>
  struct WPtrNullCheck
  {
    static int f (lua_State* L)
    {
      bool rv = true;
      boost::weak_ptr<T> tw = luabridge::Stack<boost::weak_ptr<T> >::get (L, 1);
      boost::shared_ptr<T> const t = tw.lock();
      if (t) {
        T* const tt = t.get();
        rv = (tt == 0);
      }
      Stack <bool>::push (L, rv);
      return 1;
    }
  };

  template <class T>
  struct PtrEqualCheck
  {
    static int f (lua_State* L)
    {
      boost::shared_ptr<T> t0 = luabridge::Stack<boost::shared_ptr<T> >::get (L, 1);
      boost::shared_ptr<T> t1 = luabridge::Stack<boost::shared_ptr<T> >::get (L, 2);
      Stack <bool>::push (L, t0 == t1);
      return 1;
    }
  };

  template <class T>
  struct WPtrEqualCheck
  {
    static int f (lua_State* L)
    {
      bool rv = false;
      boost::weak_ptr<T> tw0 = luabridge::Stack<boost::weak_ptr<T> >::get (L, 1);
      boost::weak_ptr<T> tw1 = luabridge::Stack<boost::weak_ptr<T> >::get (L, 2);
      boost::shared_ptr<T> const t0 = tw0.lock();
      boost::shared_ptr<T> const t1 = tw1.lock();
      if (t0 && t1) {
        T* const tt0 = t0.get();
        T* const tt1 = t1.get();
        rv = (tt0 == tt1);
      }
      Stack <bool>::push (L, rv);
      return 1;
    }
  };

  template <class T>
  struct ClassEqualCheck<boost::shared_ptr<T> >
  {
    static int f (lua_State* L)
    {
      return PtrEqualCheck<T>::f (L);
    }
  };

  template <class T>
  struct ClassEqualCheck<boost::weak_ptr<T> >
  {
    static int f (lua_State* L)
    {
      return WPtrEqualCheck<T>::f (L);
    }
  };

  template <class C, typename T>
  static int getPtrProperty (lua_State* L)
  {
    boost::shared_ptr<C> cp = luabridge::Stack<boost::shared_ptr<C> >::get (L, 1);
    C const* const c = cp.get();
    if (!c) {
      return luaL_error (L, "shared_ptr is nil");
    }
    T C::** mp = static_cast <T C::**> (lua_touserdata (L, lua_upvalueindex (1)));
    Stack <T>::push (L, c->**mp);
    return 1;
  }

  template <class C, typename T>
  static int getWPtrProperty (lua_State* L)
  {
    boost::weak_ptr<C> cw = luabridge::Stack<boost::weak_ptr<C> >::get (L, 1);
    boost::shared_ptr<C> const cp = cw.lock();
    if (!cp) {
      return luaL_error (L, "cannot lock weak_ptr");
    }
    C const* const c = cp.get();
    if (!c) {
      return luaL_error (L, "weak_ptr is nil");
    }
    T C::** mp = static_cast <T C::**> (lua_touserdata (L, lua_upvalueindex (1)));
    Stack <T>::push (L, c->**mp);
    return 1;
  }

  template <class C, typename T>
  static int setPtrProperty (lua_State* L)
  {
    boost::shared_ptr<C> cp = luabridge::Stack<boost::shared_ptr<C> >::get (L, 1);
    C* const c = cp.get();
    if (!c) {
      return luaL_error (L, "shared_ptr is nil");
    }
    T C::** mp = static_cast <T C::**> (lua_touserdata (L, lua_upvalueindex (1)));
    c->**mp = Stack <T>::get (L, 2);
    return 0;
  }

  template <class C, typename T>
  static int setWPtrProperty (lua_State* L)
  {
    boost::weak_ptr<C> cw = luabridge::Stack<boost::weak_ptr<C> >::get (L, 1);
    boost::shared_ptr<C> cp = cw.lock();
    if (!cp) {
      return luaL_error (L, "cannot lock weak_ptr");
    }
    C* const c = cp.get();
    if (!c) {
      return luaL_error (L, "weak_ptr is nil");
    }
    T C::** mp = static_cast <T C::**> (lua_touserdata (L, lua_upvalueindex (1)));
    c->**mp = Stack <T>::get (L, 2);
    return 0;
  }

  template <class MemFnPtr, class T,
           class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallMemberWPtr
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::weak_ptr<T>* const tw = Userdata::get <boost::weak_ptr<T> > (L, 1, false);
      boost::shared_ptr<T> const t = tw->lock();
      if (!t) {
        return luaL_error (L, "cannot lock weak_ptr");
      }
      T* const tt = t.get();
      if (!tt) {
        return luaL_error (L, "weak_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (tt, fnptr, args));
      return 1;
    }
  };

  /**
      lua_CFunction to calls for function references.
  */
  template <class MemFnPtr,
            class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallMemberRef
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T* const t = Userdata::get <T> (L, 1, false);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (t, fnptr, args));
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 2;
    }
  };

  template <class MemFnPtr,
            class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallConstMemberRef
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T const* const t = Userdata::get <T> (L, 1, true);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args(L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (t, fnptr, args));
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 2;
    }
  };

  template <class MemFnPtr, class T,
            class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallMemberRefPtr
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::shared_ptr<T>* const t = Userdata::get <boost::shared_ptr<T> > (L, 1, false);
      T* const tt = t->get();
      if (!tt) {
        return luaL_error (L, "shared_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (tt, fnptr, args));
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 2;
    }
  };

  template <class MemFnPtr, class T,
            class ReturnType = typename FuncTraits <MemFnPtr>::ReturnType>
  struct CallMemberRefWPtr
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::weak_ptr<T>* const tw = Userdata::get <boost::weak_ptr<T> > (L, 1, false);
      boost::shared_ptr<T> const t = tw->lock();
      if (!t) {
        return luaL_error (L, "cannot lock weak_ptr");
      }
      T* const tt = t.get();
      if (!tt) {
        return luaL_error (L, "weak_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      Stack <ReturnType>::push (L, FuncTraits <MemFnPtr>::call (tt, fnptr, args));
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 2;
    }
  };

  //----------------------------------------------------------------------------
  /**
      lua_CFunction to call a class member function with no return value.

      The member function pointer is in the first upvalue.
      The class userdata object is at the top of the Lua stack.
  */
  template <class MemFnPtr>
  struct CallMember <MemFnPtr, void>
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T* const t = Userdata::get <T> (L, 1, false);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (t, fnptr, args);
      return 0;
    }
  };

  template <class MemFnPtr>
  struct CallConstMember <MemFnPtr, void>
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T const* const t = Userdata::get <T> (L, 1, true);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (t, fnptr, args);
      return 0;
    }
  };

  template <class MemFnPtr, class T>
  struct CallMemberPtr <MemFnPtr, T, void>
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::shared_ptr<T>* const t = Userdata::get <boost::shared_ptr<T> > (L, 1, false);
      T* const tt = t->get();
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (tt, fnptr, args);
      return 0;
    }
  };

  template <class MemFnPtr, class T>
  struct CallMemberWPtr <MemFnPtr, T, void>
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::weak_ptr<T>* const tw = Userdata::get <boost::weak_ptr<T> > (L, 1, false);
      boost::shared_ptr<T> const t = tw->lock();
      if (!t) {
        return luaL_error (L, "cannot lock weak_ptr");
      }
      T* const tt = t.get();
      if (!tt) {
        return luaL_error (L, "weak_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (tt, fnptr, args);
      return 0;
    }
  };

  template <class MemFnPtr>
  struct CallMemberRef <MemFnPtr, void>
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T* const t = Userdata::get <T> (L, 1, false);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (t, fnptr, args);
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 1;
    }
  };

  template <class MemFnPtr>
  struct CallConstMemberRef <MemFnPtr, void>
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      T const* const t = Userdata::get <T> (L, 1, true);
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (t, fnptr, args);
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 1;
    }
  };

  template <class MemFnPtr, class T>
  struct CallMemberRefPtr <MemFnPtr, T, void>
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::shared_ptr<T>* const t = Userdata::get <boost::shared_ptr<T> > (L, 1, false);
      T* const tt = t->get();
      if (!tt) {
        return luaL_error (L, "shared_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (tt, fnptr, args);
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 1;
    }
  };

  template <class MemFnPtr, class T>
  struct CallMemberRefWPtr <MemFnPtr, T, void>
  {
    typedef typename FuncTraits <MemFnPtr>::Params Params;

    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      boost::weak_ptr<T>* const tw = Userdata::get <boost::weak_ptr<T> > (L, 1, false);
      boost::shared_ptr<T> const t = tw->lock();
      if (!t) {
        return luaL_error (L, "cannot lock weak_ptr");
      }
      T* const tt = t.get();
      if (!tt) {
        return luaL_error (L, "weak_ptr is nil");
      }
      MemFnPtr const& fnptr = *static_cast <MemFnPtr const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      ArgList <Params, 2> args (L);
      FuncTraits <MemFnPtr>::call (tt, fnptr, args);
      LuaRef v (newTable (L));
      FuncArgs <Params, 0>::refs (v, args);
      v.push(L);
      return 1;
    }
  };

  //--------------------------------------------------------------------------
  /**
      lua_CFunction to call a class member lua_CFunction.

      The member function pointer is in the first upvalue.
      The class userdata object is at the top of the Lua stack.
  */
  template <class T>
  struct CallMemberCFunction
  {
    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      typedef int (T::*MFP)(lua_State* L);
      T* const t = Userdata::get <T> (L, 1, false);
      MFP const& fnptr = *static_cast <MFP const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      return (t->*fnptr) (L);
    }
  };

  template <class T>
  struct CallConstMemberCFunction
  {
    static int f (lua_State* L)
    {
      assert (isfulluserdata (L, lua_upvalueindex (1)));
      typedef int (T::*MFP)(lua_State* L);
      T const* const t = Userdata::get <T> (L, 1, true);
      MFP const& fnptr = *static_cast <MFP const*> (lua_touserdata (L, lua_upvalueindex (1)));
      assert (fnptr != 0);
      return (t->*fnptr) (L);
    }
  };

  //--------------------------------------------------------------------------

  // SFINAE Helpers

  template <class MemFnPtr, bool isConst>
  struct CallMemberFunctionHelper
  {
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallConstMember <MemFnPtr>::f, 1);
      lua_pushvalue (L, -1);
      rawsetfield (L, -5, name); // const table
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr>
  struct CallMemberFunctionHelper <MemFnPtr, false>
  {
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallMember <MemFnPtr>::f, 1);
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr>
  struct CallMemberPtrFunctionHelper
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallMemberPtr <MemFnPtr, T>::f, 1);
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr>
  struct CallMemberRefPtrFunctionHelper
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallMemberRefPtr <MemFnPtr, T>::f, 1);
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr>
  struct CallMemberWPtrFunctionHelper
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallMemberWPtr <MemFnPtr, T>::f, 1);
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr>
  struct CallMemberRefWPtrFunctionHelper
  {
    typedef typename FuncTraits <MemFnPtr>::ClassType T;
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallMemberRefWPtr <MemFnPtr, T>::f, 1);
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr, bool isConst>
  struct CallMemberRefFunctionHelper
  {
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallConstMemberRef <MemFnPtr>::f, 1);
      lua_pushvalue (L, -1);
      rawsetfield (L, -5, name); // const table
      rawsetfield (L, -3, name); // class table
    }
  };

  template <class MemFnPtr>
  struct CallMemberRefFunctionHelper <MemFnPtr, false>
  {
    static void add (lua_State* L, char const* name, MemFnPtr mf)
    {
      new (lua_newuserdata (L, sizeof (MemFnPtr))) MemFnPtr (mf);
      lua_pushcclosure (L, &CallMemberRef <MemFnPtr>::f, 1);
      rawsetfield (L, -3, name); // class table
    }
  };

  //--------------------------------------------------------------------------
  /**
      __gc metamethod for a class.
  */
  template <class C>
  static int gcMetaMethod (lua_State* L)
  {
    Userdata* const ud = Userdata::getExact <C> (L, 1);
    ud->~Userdata ();
    return 0;
  }

  static int gcNOOPMethod (lua_State* L)
  {
    return 0;
  }

  //--------------------------------------------------------------------------
  /**
      lua_CFunction to get a class data member.

      The pointer-to-member is in the first upvalue.
      The class userdata object is at the top of the Lua stack.
  */
  template <class C, typename T>
  static int getProperty (lua_State* L)
  {
    C const* const c = Userdata::get <C> (L, 1, true);
    T C::** mp = static_cast <T C::**> (lua_touserdata (L, lua_upvalueindex (1)));
    Stack <T>::push (L, c->**mp);
    return 1;
  }

  //--------------------------------------------------------------------------

  /**
      lua_CFunction to get a constant (enum)
  */
  template <typename U>
  static int getConst (lua_State* L)
  {
    U *v = static_cast <U *> (lua_touserdata (L, lua_upvalueindex (1)));
    assert (v);
    Stack <U>::push (L, *v);
    return 1;
  }

  //--------------------------------------------------------------------------
  /**
      lua_CFunction to set a class data member.

      The pointer-to-member is in the first upvalue.
      The class userdata object is at the top of the Lua stack.
  */
  template <class C, typename T>
  static int setProperty (lua_State* L)
  {
    C* const c = Userdata::get <C> (L, 1, false);
    T C::** mp = static_cast <T C::**> (lua_touserdata (L, lua_upvalueindex (1)));
    c->**mp = Stack <T>::get (L, 2);
    return 0;
  }

  //--------------------------------------------------------------------------

  // metatable callback for "array[index]"
  template <typename T>
  static int array_index (lua_State* L) {
    T** parray = (T**) luaL_checkudata (L, 1, typeid(T).name());
    int const index = luabridge::Stack<int>::get (L, 2);
    assert (index > 0);
    luabridge::Stack<T>::push (L, (*parray)[index-1]);
    return 1;
  }

  // metatable callback for "array[index] = value"
  template <typename T>
  static int array_newindex (lua_State* L) {
    T** parray = (T**) luaL_checkudata (L, 1, typeid(T).name());
    int const index = luabridge::Stack<int>::get (L, 2);
    T const value = luabridge::Stack<T>::get (L, 3);
    assert (index > 0);
    (*parray)[index-1] = value;
    return 0;
  }

  template <typename T>
  static int getArray (lua_State* L) {
    T *v = luabridge::Stack<T*>::get (L, 1);
    T** parray = (T**) lua_newuserdata(L, sizeof(T**));
    *parray = v;
    luaL_getmetatable(L, typeid(T).name());
    lua_setmetatable(L, -2);
    return 1;
  }

  // copy complete c array to lua table
  template <typename T>
  static int getTable (lua_State* L) {
    T *v = luabridge::Stack<T*>::get (L, 1);
    const int cnt = luabridge::Stack<int>::get (L, 2);
    LuaRef t (L);
    t = newTable (L);
    for (int i = 0; i < cnt; ++i) {
      t[i + 1] = v[i];
    }
    t.push(L);
    return 1;
  }

  // copy lua table to c array
  template <typename T>
  static int setTable (lua_State* L) {
    T *v = luabridge::Stack<T*>::get (L, 1);
    LuaRef t (LuaRef::fromStack(L, 2));
    const int cnt = luabridge::Stack<int>::get (L, 3);
    for (int i = 0; i < cnt; ++i) {
      v[i] = t[i + 1];
    }
    return 0;
  }

  // return same array at an offset
  template <typename T>
  static int offsetArray (lua_State* L) {
    T *v = luabridge::Stack<T*>::get (L, 1);
    const unsigned int i = luabridge::Stack<unsigned int>::get (L, 2);
    Stack <T*>::push (L, &v[i]);
    return 1;
  }

  //--------------------------------------------------------------------------
  /**
    C++ STL iterators
   */

  // read lua table into C++ std::list
  template <class T, class C>
  static int tableToListHelper (lua_State *L, C * const t)
  {
    if (!t) { return luaL_error (L, "invalid pointer to std::list<>/std::vector"); }
    if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }
    lua_pushvalue (L, -1);
    lua_pushnil (L);
    while (lua_next (L, -2)) {
      lua_pushvalue (L, -2);
      T const value = Stack<T>::get (L, -2);
      t->push_back (value);
      lua_pop (L, 2);
    }
    lua_pop (L, 1);
    lua_pop (L, 2);
    Stack<C>::push (L, *t);
    return 1;
  }

  template <class T, class C>
  static int tableToList (lua_State *L)
  {
    C * const t = Userdata::get<C> (L, 1, false);
    return tableToListHelper<T, C> (L, t);
  }

  template <class T, class C>
  static int ptrTableToList (lua_State *L)
  {
    boost::shared_ptr<C> const* const t = Userdata::get<boost::shared_ptr<C> > (L, 1, true);
    if (!t) { return luaL_error (L, "cannot derefencee shared_ptr"); }
    return tableToListHelper<T, C> (L, t->get());
  }
  //--------------------------------------------------------------------------


  template <class T, class C>
  static int vectorToArray (lua_State *L)
  {
    C * const t = Userdata::get<C> (L, 1, false);
    T * a = &((*t)[0]);
    Stack <T*>::push (L, a);
    return 1;
  }

  //--------------------------------------------------------------------------
  template <class T, class C>
  static int listIterIter (lua_State *L) {
    typedef typename C::const_iterator IterType;
    IterType * const end = static_cast <IterType * const> (lua_touserdata (L, lua_upvalueindex (2)));
    IterType * const iter = static_cast <IterType * const> (lua_touserdata (L, lua_upvalueindex (1)));
    assert (end);
    assert (iter);
    if ((*iter) == (*end)) {
      return 0;
    }
    Stack <T>::push (L, **iter);
    ++(*iter);
    return 1;
  }

  // generate an iterator
  template <class T, class C>
  static int listIterHelper (lua_State *L, C const * const t)
  {
    if (!t) { return luaL_error (L, "invalid pointer to std::list<>/std::vector"); }
    typedef typename C::const_iterator IterType;
    new (lua_newuserdata (L, sizeof (IterType*))) IterType (t->begin());
    new (lua_newuserdata (L, sizeof (IterType*))) IterType (t->end());
    lua_pushcclosure (L, listIterIter<T, C>, 2);
    return 1;
  }

  template <class T, class C>
  static int listIter (lua_State *L)
  {
    C const * const t = Userdata::get <C> (L, 1, true);
    return listIterHelper<T, C> (L, t);
  }

  template <class T, class C>
  static int ptrListIter (lua_State *L)
  {
    boost::shared_ptr<C> const* const t = Userdata::get <boost::shared_ptr<C> >(L, 1, true);
    if (!t) { return luaL_error (L, "cannot derefencee shared_ptr"); }
    return listIterHelper<T, C> (L, t->get());
  }

  //--------------------------------------------------------------------------
  // generate table from std::list
  template <class T, class C>
  static int listToTableHelper (lua_State *L, C const* const t)
  {
    if (!t) { return luaL_error (L, "invalid pointer to std::list<>/std::vector"); }
#if 0 // direct lua api
    lua_createtable(L, t->size(), 0);
    int newTable = lua_gettop(L);
    int index = 1;
    for (typename C::const_iterator iter = t->begin(); iter != t->end(); ++iter, ++index) {
      Stack<T>::push(L, (*iter));
      lua_rawseti (L, newTable, index);
    }
#else // luabridge way
    LuaRef v (L);
    v = newTable (L);
    int index = 1;
    for (typename C::const_iterator iter = t->begin(); iter != t->end(); ++iter, ++index) {
      v[index] = (*iter);
    }
    v.push(L);
#endif
    return 1;
  }

  template <class T, class C>
  static int listToTable (lua_State *L)
  {
    C const* const t = Userdata::get <C> (L, 1, true);
    return listToTableHelper<T, C> (L, t);
  }

  template <class T, class C>
  static int ptrListToTable (lua_State *L)
  {
    boost::shared_ptr<C> const* const t = Userdata::get <boost::shared_ptr<C> > (L, 1, true);
    if (!t) { return luaL_error (L, "cannot derefencee shared_ptr"); }
    return listToTableHelper<T, C> (L, t->get());
  }

  //--------------------------------------------------------------------------
  // push back a C-pointer to a std::list<T*>

  template <class T, class C>
  static int pushbackptr (lua_State *L)
  {
    C * const c = Userdata::get <C> (L, 1, false);
    if (!c) { return luaL_error (L, "invalid pointer to std::list<>"); }
    T * const v = Userdata::get <T> (L, 2, true);
    if (!v) { return luaL_error (L, "invalid pointer to std::list<>::value_type"); }
    c->push_back (v);
    return 0;
  }

  //--------------------------------------------------------------------------
  // generate std::map from table

  template <class K, class V>
  static int tableToMap (lua_State *L)
  {
    typedef std::map<K, V> C;
    C * const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::map"); }
    if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }

    lua_pushvalue (L, -1);
    lua_pushnil (L);
    while (lua_next (L, -2)) {
      lua_pushvalue (L, -2);
      K const key = Stack<K>::get (L, -1);
      V const value = Stack<V>::get (L, -2);
      t->insert (std::pair<K,V> (key, value));
      //(*t)[key] = value;
      lua_pop (L, 2);
    }
    lua_pop (L, 1);
    lua_pop (L, 2);
    Stack<C>::push (L, *t);
    return 1;
  }

  // iterate over a std::map
  template <class K, class V>
  static int mapIterIter (lua_State *L)
  {
    typedef std::map<K, V> C;
    typedef typename C::const_iterator IterType;
    IterType * const end = static_cast <IterType * const> (lua_touserdata (L, lua_upvalueindex (2)));
    IterType * const iter = static_cast <IterType * const> (lua_touserdata (L, lua_upvalueindex (1)));
    assert (end);
    assert (iter);
    if ((*iter) == (*end)) {
      return 0;
    }
    Stack <K>::push (L, (*iter)->first);
    Stack <V>::push (L, (*iter)->second);
    ++(*iter);
    return 2;
  }

  // generate iterator
  template <class K, class V>
  static int mapIter (lua_State *L)
  {
    typedef std::map<K, V> C;
    C const * const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::map"); }
    typedef typename C::const_iterator IterType;
    new (lua_newuserdata (L, sizeof (IterType*))) IterType (t->begin());
    new (lua_newuserdata (L, sizeof (IterType*))) IterType (t->end());
    lua_pushcclosure (L, mapIterIter<K, V>, 2);
    return 1;
  }

  // generate table from std::map
  template <class K, class V>
  static int mapToTable (lua_State *L)
  {
    typedef std::map<K, V> C;
    C const* const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::map"); }

    LuaRef v (L);
    v = newTable (L);
    for (typename C::const_iterator iter = t->begin(); iter != t->end(); ++iter) {
      v[(*iter).first] = (*iter).second;
    }
    v.push(L);
    return 1;
  }

  // generate table from std::map
  template <class K, class V>
  static int mapAt (lua_State *L)
  {
    typedef std::map<K, V> C;
    C const* const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::map"); }
    K const key = Stack<K>::get (L, 2);
    typename C::const_iterator iter = t->find(key);
    if (iter == t->end()) {
      return 0;
    }
    Stack <V>::push (L, (*iter).second);
    return 1;
  }


  //--------------------------------------------------------------------------
  // generate std::set from table keys ( table[member] = true )
  // http://www.lua.org/pil/11.5.html

  template <class T, class C>
  static int tableToSet (lua_State *L)
  {
    C * const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::set"); }
    if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }

    lua_pushvalue (L, -1);
    lua_pushnil (L);
    while (lua_next (L, -2)) {
      lua_pushvalue (L, -2);
      T const member = Stack<T>::get (L, -1);
      bool const v = Stack<bool>::get (L, -2);
      if (v) {
        t->insert (member);
      }
      lua_pop (L, 2);
    }
    lua_pop (L, 1);
    lua_pop (L, 2);
    Stack<C>::push (L, *t);
    return 1;
  }

  // iterate over a std::set, explicit "true" value.
  // compare to http://www.lua.org/pil/11.5.html
  template <class T, class C>
  static int setIterIter (lua_State *L)
  {
    typedef typename C::const_iterator IterType;
    IterType * const end = static_cast <IterType * const> (lua_touserdata (L, lua_upvalueindex (2)));
    IterType * const iter = static_cast <IterType * const> (lua_touserdata (L, lua_upvalueindex (1)));
    assert (end);
    assert (iter);
    if ((*iter) == (*end)) {
      return 0;
    }
    Stack <T>::push (L, **iter);
    Stack <bool>::push (L, true);
    ++(*iter);
    return 2;
  }

  // generate iterator
  template <class T, class C>
  static int setIter (lua_State *L)
  {
    C const * const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::set"); }
    typedef typename C::const_iterator IterType;
    new (lua_newuserdata (L, sizeof (IterType*))) IterType (t->begin());
    new (lua_newuserdata (L, sizeof (IterType*))) IterType (t->end());
    lua_pushcclosure (L, setIterIter<T, C>, 2);
    return 1;
  }

  // generate table from std::set
  template <class T, class C>
  static int setToTable (lua_State *L)
  {
    C const* const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::set"); }

    LuaRef v (L);
    v = newTable (L);
    for (typename C::const_iterator iter = t->begin(); iter != t->end(); ++iter) {
      v[(*iter)] = true;
    }
    v.push(L);
    return 1;
  }

  //--------------------------------------------------------------------------
  // bitset { num = true }
  // compare to http://www.lua.org/pil/11.5.html
  template <unsigned int T>
  static int tableToBitSet (lua_State *L)
  {
    typedef std::bitset<T> C;
    C * const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::bitset"); }
    if (!lua_istable (L, -1)) { return luaL_error (L, "argument is not a table"); }

    lua_pushvalue (L, -1);
    lua_pushnil (L);
    while (lua_next (L, -2)) {
      lua_pushvalue (L, -2);
      unsigned int const member = Stack<unsigned int>::get (L, -1);
      bool const v = Stack<bool>::get (L, -2);
      if (member < T && v) {
        t->set (member);
      }
      lua_pop (L, 2);
    }
    lua_pop (L, 1);
    lua_pop (L, 2);
    Stack<C>::push (L, *t);
    return 1;
  }

  // generate table from std::bitset
  template <unsigned int T>
  static int bitSetToTable (lua_State *L)
  {
    typedef std::bitset<T> C;
    C const* const t = Userdata::get <C> (L, 1, true);
    if (!t) { return luaL_error (L, "invalid pointer to std::bitset"); }

    LuaRef v (L);
    v = newTable (L);
    for (unsigned int i = 0; i < T; ++i) {
      if (t->test (i)) {
        v[i] = true;
      }
    }
    v.push(L);
    return 1;
  }

};

/* vim: set et sw=2: */
