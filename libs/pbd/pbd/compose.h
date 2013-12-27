/* Defines String::compose(fmt, arg...) for easy, i18n-friendly
 * composition of strings.
 *
 * Version 1.0.
 *
 * Copyright (c) 2002 Ole Laursen <olau@hardworking.dk>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

//
// Basic usage is like
//
//   std::cout << String::compose("This is a %1x%2 matrix.", rows, cols);
//
// See http://www.cs.auc.dk/~olau/compose/ or the included README.compose for
// more details.
//

#ifndef STRING_COMPOSE_H
#define STRING_COMPOSE_H

#include <sstream>
#include <string>
#include <list>
#include <map>			// for multimap

#include "pbd/libpbd_visibility.h"

namespace StringPrivate
{
  // the actual composition class - using string::compose is cleaner, so we
  // hide it here
  class LIBPBD_API Composition
  {
  public:
    // initialize and prepare format string on the form "text %1 text %2 etc."
    explicit Composition(std::string fmt);

    // supply an replacement argument starting from %1
    template <typename T>
    Composition &arg(const T &obj);

    // compose and return string
    std::string str() const;

  private:
    std::ostringstream os;
    int arg_no;

    // we store the output as a list - when the output string is requested, the
    // list is concatenated to a string; this way we can keep iterators into
    // the list instead of into a string where they're possibly invalidated on
    // inserting a specification string
    typedef std::list<std::string> output_list;
    output_list output;

    // the initial parse of the format string fills in the specification map
    // with positions for each of the various %?s
    typedef std::multimap<int, output_list::iterator> specification_map;
    specification_map specs;
  };

  // helper for converting spec string numbers
  inline int char_to_int(char c)
  {
    switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    default: return -1000;
    }
  }

  inline bool is_number(int n)
  {
    switch (n) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return true;
    
    default:
      return false;
    }
  }


  // implementation of class Composition
  template <typename T>
  inline Composition &Composition::arg(const T &obj)
  {
    os << obj;

    std::string rep = os.str();
  
    if (!rep.empty()) {		// manipulators don't produce output
      for (specification_map::const_iterator i = specs.lower_bound(arg_no),
	     end = specs.upper_bound(arg_no); i != end; ++i) {
	output_list::iterator pos = i->second;
	++pos;
      
	output.insert(pos, rep);
      }
    
      os.str(std::string());
      //os.clear();
      ++arg_no;
    }
  
    return *this;
  }

  inline Composition::Composition(std::string fmt)
    : arg_no(1)
  {
    std::string::size_type b = 0, i = 0;
  
    // fill in output with the strings between the %1 %2 %3 etc. and
    // fill in specs with the positions
    while (i < fmt.length()) {
      if (fmt[i] == '%' && i + 1 < fmt.length()) {
	if (fmt[i + 1] == '%') {	// catch %%
	  fmt.replace(i, 2, "%");
	  ++i;
	}
	else if (is_number(fmt[i + 1])) { // aha! a spec!
	  // save string
	  output.push_back(fmt.substr(b, i - b));
	
	  int n = 1;		// number of digits
	  int spec_no = 0;

	  do {
	    spec_no += char_to_int(fmt[i + n]);
	    spec_no *= 10;
	    ++n;
	  } while (i + n < fmt.length() && is_number(fmt[i + n]));

	  spec_no /= 10;
	  output_list::iterator pos = output.end();
	  --pos;		// safe since we have just inserted a string>
	
	  specs.insert(specification_map::value_type(spec_no, pos));
	
	  // jump over spec string
	  i += n;
	  b = i;
	}
	else
	  ++i;
      }
      else
	++i;
    }
  
    if (i - b > 0)		// add the rest of the string
      output.push_back(fmt.substr(b, i - b));
  }

  inline std::string Composition::str() const
  {
    // assemble string
    std::string str;
  
    for (output_list::const_iterator i = output.begin(), end = output.end();
	 i != end; ++i)
      str += *i;
  
    return str;
  }
}

// now for the real thing(s)
//namespace PBD
//{
  // a series of functions which accept a format string on the form "text %1
  // more %2 less %3" and a number of templated parameters and spits out the
  // composited string
  template <typename T1> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt, const T1 &o1)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1);
    return c.str();
  }

  template <typename T1, typename T2> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2);
    return c.str();
  }

  template <typename T1, typename T2, typename T3> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9, typename T10> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9,
			     const T10 &o10)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9)
      .arg(o10);
    return c.str();
  }
  
  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9, typename T10,
	    typename T11> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9,
			     const T10 &o10, const T11 &o11)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9)
      .arg(o10).arg(o11);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9, typename T10,
	    typename T11, typename T12> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9,
			     const T10 &o10, const T11 &o11, const T12 &o12)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9)
      .arg(o10).arg(o11).arg(o12);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9, typename T10,
	    typename T11, typename T12, typename T13> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9,
			     const T10 &o10, const T11 &o11, const T12 &o12,
			     const T13 &o13)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9)
      .arg(o10).arg(o11).arg(o12).arg(o13);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9, typename T10,
	    typename T11, typename T12, typename T13, typename T14> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9,
			     const T10 &o10, const T11 &o11, const T12 &o12,
			     const T13 &o13, const T14 &o14)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9)
      .arg(o10).arg(o11).arg(o12).arg(o13).arg(o14);
    return c.str();
  }

  template <typename T1, typename T2, typename T3, typename T4, typename T5,
	    typename T6, typename T7, typename T8, typename T9, typename T10,
	    typename T11, typename T12, typename T13, typename T14,
	    typename T15> LIBPBD_API 
  inline std::string string_compose(const std::string &fmt,
			     const T1 &o1, const T2 &o2, const T3 &o3,
			     const T4 &o4, const T5 &o5, const T6 &o6,
			     const T7 &o7, const T8 &o8, const T9 &o9,
			     const T10 &o10, const T11 &o11, const T12 &o12,
			     const T13 &o13, const T14 &o14, const T15 &o15)
  {
    StringPrivate::Composition c(fmt);
    c.arg(o1).arg(o2).arg(o3).arg(o4).arg(o5).arg(o6).arg(o7).arg(o8).arg(o9)
      .arg(o10).arg(o11).arg(o12).arg(o13).arg(o14).arg(o15);
    return c.str();
  }
//}


#endif // STRING_COMPOSE_H
