/*
    Copyright (C) 2009 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __pbd_closure_h__
#define __pbd_closure_h__

#include <glib.h>

/**
 * Here we implement thread-safe but lifetime-unsafe closures (aka "functor"), which
 * wrap an object and one of its methods, plus zero-or-more arguments, in a convenient,
 * ready to use package. 
 *
 * These differ from sigc::slot<> in that they are totally non-invasive with 
 * respect to the objects referenced by the closure. There is no requirement
 * that the object be derived from any particular base class, and nothing
 * will be done to the object during the creation of the closure, or its deletion,
 * or at any time other than when the object's method is invoked via
 * Closure::operator(). As a result, the closure can be constructed and deleted without
 * concerns for thread safety. If the object method is thread-safe, then the closure
 * can also be invoked in a thread safe fashion.
 *
 * However, this also means that the closure is not safe against lifetime
 * management issues - if the referenced object is deleted before the closure,
 * and then closure is invoked via operator(), the results are undefined (but 
 * will almost certainly be bad). This class should therefore be used only
 * where you can guarantee that the referred-to object will outlive the
 * life of the closure.
*/

namespace PBD {

struct ClosureBaseImpl {
    ClosureBaseImpl() { g_atomic_int_set (&_ref, 0); }

    ClosureBaseImpl* ref() { g_atomic_int_inc (&_ref); return this; }
    void unref() { if (g_atomic_int_dec_and_test (&_ref)) delete this; }

    virtual void operator() () = 0;

protected:
    virtual ~ClosureBaseImpl() { }

private:
    gint _ref;
};

struct Closure {
    Closure () : impl (0) {}
    Closure (ClosureBaseImpl* i) : impl (i->ref()) {}
    Closure (const Closure& other) : impl (other.impl ? other.impl->ref() : 0) {}

    Closure& operator= (const Closure& other) { 
	    if (&other == this) {
		    return *this;
	    }
	    if (impl) {
		    impl->unref();
	    } 
	    if (other.impl) {
		    impl = other.impl->ref();
	    } else {
		    impl = 0;
	    }
	    return *this;
    }

    virtual ~Closure () { if (impl) { impl->unref(); } }

    /* will crash if impl is unset */
    void operator() () const { (*impl)(); }

 protected:
    ClosureBaseImpl* impl;
};

template<typename T>
struct ClosureImpl0 : public ClosureBaseImpl {
	ClosureImpl0 (T& obj, void (T::*m)())
		: object (obj), method (m) {}
	void operator() () { (object.*method)(); }
  private:
	T& object;
	void (T::*method)();
};

template<typename T, typename A1>
struct ClosureImpl1 : public ClosureBaseImpl 
{
	ClosureImpl1 (T& obj, void (T::*m)(A1), A1 arg)
		: object (obj), method (m), arg1 (arg) {}
	void operator() () { (object.*method) (arg1); }

  private:
	T& object;
	void (T::*method)(A1);
	A1 arg1;
};

template<typename T, typename A1, typename A2>
struct ClosureImpl2 : public ClosureBaseImpl 
{
	ClosureImpl2 (T& obj, void (T::*m)( A1,  A2),  A1 arga,  A2 argb)
		: object (obj), method (m), arg1 (arga), arg2 (argb) {}
	void operator() () { (object.*method) (arg1, arg2); }

  private:
	 T& object;
	 void (T::*method)(A1, A2);
	 A1 arg1;
	 A2 arg2;
};

template<typename T, typename A1,  typename A2, typename A3>
struct ClosureImpl3 : public ClosureBaseImpl 
{
	ClosureImpl3 (T& obj, void (T::*m)( A1,  A2,  A3),  A1 arga,  A2 argb,  A3 argc)
		: object (obj), method (m), arg1 (arga), arg2 (argb), arg3 (argc) {}
	void operator() () { (object.*method) (arg1, arg2, arg3); }

  private:
	 T& object;
	 void (T::*method)(A1, A2, A3);
	 A1 arg1;
	 A2 arg2;
	 A3 arg3;
};

template<typename T>
Closure closure (T& t, void (T::*m)()) {return Closure (new ClosureImpl0<T> (t,m)); }

template<typename T, typename A>
Closure closure (T& t, void (T::*m)(A),  A a) { return Closure (new ClosureImpl1<T,A>(t,m,a)); }

template<typename T, typename A1, typename A2>
Closure closure (T& t, void (T::*m)(A1,A2), A1 a1, A2 a2) { return Closure (new ClosureImpl2<T,A1,A2>(t,m, a1, a2)); }

template<typename T, typename A1, typename A2, typename A3>
Closure closure (T& t, void (T::*m)(A1, A2, A3), A1 a1, A2 a2, A3 a3) { return Closure (new ClosureImpl3<T,A1,A2,A3>(t,m , a1, a2, a3)); }

/*---------*/

template<typename A>
struct CTClosureBaseImpl : ClosureBaseImpl {
    CTClosureBaseImpl() {}

    virtual void operator() () { operator() (A()); }
    virtual void operator() (A arg) = 0;

protected:
    virtual ~CTClosureBaseImpl() { }
};

template<typename A>
struct CTClosure : public Closure {
	CTClosure() {}
	CTClosure (CTClosureBaseImpl<A>* i) : Closure (i) {}
	CTClosure (const CTClosure& other) : Closure (other) {}
	
	/* will crash if impl is unset */
	void operator() (A arg) const { (*(dynamic_cast<CTClosureBaseImpl<A>*> (impl))) (arg); }
};

template<typename T, typename A>
struct CTClosureImpl1 : public CTClosureBaseImpl<A> 
{
	CTClosureImpl1 (T& obj, void (T::*m)(A))
		: object (obj), method (m) {}
	void operator() (A call_time_arg) { (object.*method) (call_time_arg); }
	
  private:
	T& object;
	void (T::*method)(A);
};

/* functor wraps a method that takes 1 arg provided at call-time */

template<typename T, typename A>
CTClosure<A> closure (T& t, void (T::*m)(A)) { return CTClosure<A> (new CTClosureImpl1<T,A>(t,m)); }

}

#endif /* __pbd_closure_h__ */
