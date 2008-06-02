/*
 * Copyright 2002, The libsigc++ Development Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef _SIGC_BIND_HPP_
#define _SIGC_BIND_HPP_

#include <sigc++/adaptors/bind.h>

#ifndef LIBSIGC_DISABLE_DEPRECATED

namespace SigC {

template <class T_bound1, class T_functor>
inline ::sigc::bind_functor<-1, T_functor,
                            typename ::sigc::unwrap_reference<T_bound1>::type>
bind(const T_functor& _A_functor, T_bound1 _A_b1)
{ return ::sigc::bind_functor<-1, T_functor,
                              typename ::sigc::unwrap_reference<T_bound1>::type>
                              (_A_functor, _A_b1);
}

template <class T_bound1, class T_bound2, class T_functor>
inline ::sigc::bind_functor<-1, T_functor,
                            typename ::sigc::unwrap_reference<T_bound1>::type,
                            typename ::sigc::unwrap_reference<T_bound2>::type>
bind(const T_functor& _A_functor, T_bound1 _A_b1, T_bound2 _A_b2)
{ return ::sigc::bind_functor<-1, T_functor,
                              typename ::sigc::unwrap_reference<T_bound1>::type,
                              typename ::sigc::unwrap_reference<T_bound2>::type>
                              (_A_functor, _A_b1, _A_b2); 
}

template <class T_bound1, class T_bound2, class T_bound3, class T_functor>
inline ::sigc::bind_functor<-1, T_functor,
                            typename ::sigc::unwrap_reference<T_bound1>::type,
                            typename ::sigc::unwrap_reference<T_bound2>::type,
                            typename ::sigc::unwrap_reference<T_bound3>::type>
bind(const T_functor& _A_functor, T_bound1 _A_b1, T_bound2 _A_b2,T_bound3 _A_b3)
{ return ::sigc::bind_functor<-1, T_functor,
                              typename ::sigc::unwrap_reference<T_bound1>::type,
                              typename ::sigc::unwrap_reference<T_bound2>::type,
                              typename ::sigc::unwrap_reference<T_bound3>::type>
                              (_A_functor, _A_b1, _A_b2, _A_b3);
}

}

#endif /* LIBSIGC_DISABLE_DEPRECATED */

#endif /* _SIGC_BIND_HPP_ */
