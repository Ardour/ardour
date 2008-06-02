// -*- c++ -*-
/* Do not edit! -- generated file */

#ifndef _SIGC_MACROS_HIDEHM4_
#define _SIGC_MACROS_HIDEHM4_

#include <sigc++/slot.h>
#include <sigc++/adaptors/hide.h>


#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

template <class T_hidden1, class T_return>
inline SigC::Slot1<T_return, T_hidden1>
hide(const SigC::Slot0<T_return>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot0<T_return> >
      (_A_slot); }

template <class T_hidden1, class T_return, class T_arg1>
inline SigC::Slot2<T_return, T_arg1, T_hidden1>
hide(const SigC::Slot1<T_return, T_arg1>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot1<T_return, T_arg1> >
      (_A_slot); }

template <class T_hidden1, class T_return, class T_arg1,class T_arg2>
inline SigC::Slot3<T_return, T_arg1,T_arg2, T_hidden1>
hide(const SigC::Slot2<T_return, T_arg1,T_arg2>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot2<T_return, T_arg1,T_arg2> >
      (_A_slot); }

template <class T_hidden1, class T_return, class T_arg1,class T_arg2,class T_arg3>
inline SigC::Slot4<T_return, T_arg1,T_arg2,T_arg3, T_hidden1>
hide(const SigC::Slot3<T_return, T_arg1,T_arg2,T_arg3>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot3<T_return, T_arg1,T_arg2,T_arg3> >
      (_A_slot); }

template <class T_hidden1, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline SigC::Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4, T_hidden1>
hide(const SigC::Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4> >
      (_A_slot); }

template <class T_hidden1, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline SigC::Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_hidden1>
hide(const SigC::Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> >
      (_A_slot); }

template <class T_hidden1, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5,class T_arg6>
inline SigC::Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6, T_hidden1>
hide(const SigC::Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6>& _A_slot)
{ return ::sigc::hide_functor<0, SigC::Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5,T_arg6> >
      (_A_slot); }


template <class T_hidden1,class T_hidden2, class T_return>
inline SigC::Slot2<T_return, T_hidden1,T_hidden2>
hide(const SigC::Slot0<T_return>& _A_slot)
{ return ::sigc::hide<0>(
    ::sigc::hide_functor<0, SigC::Slot0<T_return> >
      (_A_slot)); }

template <class T_hidden1,class T_hidden2, class T_return, class T_arg1>
inline SigC::Slot3<T_return, T_arg1, T_hidden1,T_hidden2>
hide(const SigC::Slot1<T_return, T_arg1>& _A_slot)
{ return ::sigc::hide<0>(
    ::sigc::hide_functor<0, SigC::Slot1<T_return, T_arg1> >
      (_A_slot)); }

template <class T_hidden1,class T_hidden2, class T_return, class T_arg1,class T_arg2>
inline SigC::Slot4<T_return, T_arg1,T_arg2, T_hidden1,T_hidden2>
hide(const SigC::Slot2<T_return, T_arg1,T_arg2>& _A_slot)
{ return ::sigc::hide<0>(
    ::sigc::hide_functor<0, SigC::Slot2<T_return, T_arg1,T_arg2> >
      (_A_slot)); }

template <class T_hidden1,class T_hidden2, class T_return, class T_arg1,class T_arg2,class T_arg3>
inline SigC::Slot5<T_return, T_arg1,T_arg2,T_arg3, T_hidden1,T_hidden2>
hide(const SigC::Slot3<T_return, T_arg1,T_arg2,T_arg3>& _A_slot)
{ return ::sigc::hide<0>(
    ::sigc::hide_functor<0, SigC::Slot3<T_return, T_arg1,T_arg2,T_arg3> >
      (_A_slot)); }

template <class T_hidden1,class T_hidden2, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4>
inline SigC::Slot6<T_return, T_arg1,T_arg2,T_arg3,T_arg4, T_hidden1,T_hidden2>
hide(const SigC::Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4>& _A_slot)
{ return ::sigc::hide<0>(
    ::sigc::hide_functor<0, SigC::Slot4<T_return, T_arg1,T_arg2,T_arg3,T_arg4> >
      (_A_slot)); }

template <class T_hidden1,class T_hidden2, class T_return, class T_arg1,class T_arg2,class T_arg3,class T_arg4,class T_arg5>
inline SigC::Slot7<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5, T_hidden1,T_hidden2>
hide(const SigC::Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5>& _A_slot)
{ return ::sigc::hide<0>(
    ::sigc::hide_functor<0, SigC::Slot5<T_return, T_arg1,T_arg2,T_arg3,T_arg4,T_arg5> >
      (_A_slot)); }



} /* namespace SigC */

#endif /* LIBSIGC_DISABLE_DEPRECATED */
#endif /* _SIGC_MACROS_HIDEHM4_ */
