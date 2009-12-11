#ifndef __pbd_closure_h__
#define __pbd_closure_h__

#include <glib.h>
#include <iostream>
using std::cerr;

namespace PBD {

struct ClosureBaseImpl {
    ClosureBaseImpl() { g_atomic_int_set (&_ref, 0); }

    ClosureBaseImpl* ref() { g_atomic_int_inc (&_ref); return this; }
    void unref() { if (g_atomic_int_dec_and_test (&_ref)) delete this; }

    virtual void operator() () = 0;

protected:
    virtual ~ClosureBaseImpl() { cerr << "DBI @ " << this << " destroyed\n"; }

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
    void operator() () { (*impl)(); }

private:
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

}

#endif /* __pbd_closure_h__ */
