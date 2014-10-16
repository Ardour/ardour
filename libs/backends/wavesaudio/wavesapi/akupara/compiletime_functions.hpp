/*
*  compiletime_functions.hpp
*  Akupara
*
*  Created by Udi on 12/19/06.
*
*/
#if !defined(_AKUPARA_COMPILETIME_FUNCTIONS_HPP__INCLUDED_)
#define _AKUPARA_COMPILETIME_FUNCTIONS_HPP__INCLUDED_

//#include "WavesPublicAPIs/wstdint.h"

namespace Akupara
{
    // For templates that "return" a value, use template_name<arguments>::value
    // For templates that "return" a type, use template_name<arguments>::type


    // Integer log2 functions
    //------------------------------------------------------------------------
    template<unsigned int n> 
    struct compiletime_bit_count_to_represent { static const unsigned int value = 1+compiletime_bit_count_to_represent<(n>>1)>::value; };

    template<> 
    struct compiletime_bit_count_to_represent<0> { static const unsigned int value = 0; };
    //------------------------------------------------------------------------
    template<unsigned int n> 
    struct compiletime_log2_ceiling  { static const unsigned int value=compiletime_bit_count_to_represent<n-1>::value; };

    template<> 
    struct compiletime_log2_ceiling<0> {}; // no value for 0 argument
    //------------------------------------------------------------------------
    template<unsigned int n> 
    struct compiletime_log2_floor { static const unsigned int value=compiletime_bit_count_to_represent<n>::value-1; };

    template<> 
    struct compiletime_log2_floor<0> {}; // no value for 0 argument
    //------------------------------------------------------------------------



    // Assertion - accessing 'value' will generate a compile-time error if the argument evaluates to false
    //------------------------------------------------------------------------
    template<bool> 
    struct compiletime_assert;

    template<>
    struct compiletime_assert<true> { static const bool value=true; };

    template<> 
    struct compiletime_assert<false> {}; // no value member for false assertion -> compile time error
    //------------------------------------------------------------------------


    // Select type - selects one of two types based on a boolean
    //------------------------------------------------------------------------
    template<bool, typename, typename>
    struct compiletime_select_type;

    template<typename _true_type, typename _false_type>
    struct compiletime_select_type<true,  _true_type, _false_type> { typedef _true_type  type; };

    template<typename _true_type, typename _false_type> 
    struct compiletime_select_type<false, _true_type, _false_type> { typedef _false_type type; };
    //------------------------------------------------------------------------





    // Integer types by byte count
    //------------------------------------------------------------------------
    namespace detail
    {
        template<unsigned int _size, bool _signed> 
        struct integer_with_byte_count_base;

        template<>
        struct integer_with_byte_count_base<1,true> { typedef int8_t  type; };

        template<>
        struct integer_with_byte_count_base<2,true> { typedef int16_t type; };

        template<>
        struct integer_with_byte_count_base<4,true> { typedef int32_t type; };

        template<>
        struct integer_with_byte_count_base<8,true> { typedef int64_t type; };

        template<>
        struct integer_with_byte_count_base<1,false> { typedef uint8_t  type; };

        template<>
        struct integer_with_byte_count_base<2,false> { typedef uint16_t type; };

        template<>
        struct integer_with_byte_count_base<4,false> { typedef uint32_t type; };

        template<>
        struct integer_with_byte_count_base<8,false> { typedef uint64_t type; };
    } // namespace detail
    //------------------------------------------------------------------------
    template<unsigned int _size, bool _signed=true>
    struct integer_with_byte_count : public detail::integer_with_byte_count_base<_size,_signed>
    {
        typedef typename detail::integer_with_byte_count_base<_size,_signed>::type type; // not required but makes the statement below less messy
        static const bool s_correct_size = compiletime_assert<sizeof(type)==_size>::value;  // if you get a compilation error here then integer_with_byte_count is not defined correctly
    };
    //------------------------------------------------------------------------
    template<unsigned int _size>
    struct signed_integer_with_byte_count : public integer_with_byte_count<_size,true> {};

    template<unsigned int _size>
    struct unsigned_integer_with_byte_count : public integer_with_byte_count<_size,false> {};
    //------------------------------------------------------------------------



    // The following are TR1 compatible, until we get decent TR1 library support on all platforms
    //------------------------------------------------------------------------
    template<typename _T, _T _v>
    struct integral_constant
    {
        static const _T                    value = _v;
        typedef _T                         value_type;
        typedef integral_constant<_T, _v>  type;
    }; // struct integral_constant
    typedef integral_constant<bool, false> false_type;
    typedef integral_constant<bool, true > true_type;
    //------------------------------------------------------------------------
    template<typename _T, typename _U> struct is_same : public false_type {};
    template<typename _T> struct is_same<_T,_T> : public true_type {};
    //------------------------------------------------------------------------



    // These are NOT TR1 but make use of some TR1 stuff
    //------------------------------------------------------------------------
    namespace detail
    {
        struct no_type;   // if you end up getting this type, it means that you asked for something that doesn't exist
        template<unsigned int _pair_index> struct signed_unsigned_pair;
#define AKUPARA_SIGNED_UNSIGNED_INTEGER_PAIR(index, base_type_name) \
    template<> struct signed_unsigned_pair<index> { typedef signed base_type_name signed_type; typedef unsigned base_type_name unsigned_type; };
#define AKUPARA_SIGNED_UNSIGNED_FLOAT_PAIR(index, type_name) \
    template<> struct signed_unsigned_pair<index> { typedef type_name signed_type; typedef no_type unsigned_type; };
        AKUPARA_SIGNED_UNSIGNED_INTEGER_PAIR(1, char     )
            AKUPARA_SIGNED_UNSIGNED_INTEGER_PAIR(2, short    )
            AKUPARA_SIGNED_UNSIGNED_INTEGER_PAIR(3, int      )
			
			//AKUPARA_SIGNED_UNSIGNED_INTEGER_PAIR(4, int32_t     )// 64BitConversion
			template<> 
			struct 
			signed_unsigned_pair<4> 
			{ 
				typedef int32_t signed_type; 
				typedef uint32_t  unsigned_type; 
			};
            
            
			AKUPARA_SIGNED_UNSIGNED_INTEGER_PAIR(5, long long)
            AKUPARA_SIGNED_UNSIGNED_FLOAT_PAIR  (6, float      )
            AKUPARA_SIGNED_UNSIGNED_FLOAT_PAIR  (7, double     )
            AKUPARA_SIGNED_UNSIGNED_FLOAT_PAIR  (8, long double)
            const unsigned int k_signed_unsigned_pair_count = 8;

        // eliminate the no_type type
        template<typename _T> struct filtered_type { typedef _T type; };
        template<> struct filtered_type<no_type> {}; // no type defined

        // search for _T in signed type list
        template<unsigned int _index, typename _T> struct find_in_signed_type_list_from_index
        {
            static const unsigned int value = is_same< _T, typename signed_unsigned_pair<_index>::signed_type >::value ? _index : find_in_signed_type_list_from_index<_index-1,_T>::value;
        };
        template<typename _T> struct find_in_signed_type_list_from_index<0, _T> { static const unsigned int value = 0; };
        template<typename _T> struct find_in_signed_type_list : public find_in_signed_type_list_from_index<k_signed_unsigned_pair_count, _T> {};

        // search for _T in unsigned type list
        template<unsigned int _index, typename _T> struct find_in_unsigned_type_list_from_index
        {
            static const unsigned int value = is_same< _T, typename signed_unsigned_pair<_index>::unsigned_type >::value ? _index : find_in_unsigned_type_list_from_index<_index-1,_T>::value;
        };
        template<typename _T> struct find_in_unsigned_type_list_from_index<0, _T> { static const unsigned int value = 0; };
        template<typename _T> struct find_in_unsigned_type_list : public find_in_unsigned_type_list_from_index<k_signed_unsigned_pair_count, _T> {};

        template<bool _is_signed, bool _is_unsigned, typename _T> struct equivalent_signed_type;
        template<typename _T> struct equivalent_signed_type  <true, false, _T> { typedef _T type; };
        template<typename _T> struct equivalent_signed_type  <false, true, _T> { typedef typename filtered_type< typename signed_unsigned_pair< find_in_unsigned_type_list<_T>::value >::signed_type >::type type; };

        template<bool _is_signed, bool _is_unsigned, typename _T> struct equivalent_unsigned_type;
        template<typename _T> struct equivalent_unsigned_type<true, false, _T> { typedef typename filtered_type< typename signed_unsigned_pair< find_in_signed_type_list<_T>::value >::unsigned_type >::type type; };
        template<typename _T> struct equivalent_unsigned_type<false, true, _T> { typedef _T type; };
    } // namespace detail
    //------------------------------------------------------------------------
    template<typename _T> struct is_signed   { static const bool value = detail::find_in_signed_type_list  <_T>::value != 0; };
    template<typename _T> struct is_unsigned { static const bool value = detail::find_in_unsigned_type_list<_T>::value != 0; };
    //------------------------------------------------------------------------
    template<typename _T> struct equivalent_signed_type   : public detail::equivalent_signed_type  < is_signed<_T>::value, is_unsigned<_T>::value, _T > {};
    template<typename _T> struct equivalent_unsigned_type : public detail::equivalent_unsigned_type< is_signed<_T>::value, is_unsigned<_T>::value, _T > {};
    //------------------------------------------------------------------------

} // namespace Akupara

#endif // _AKUPARA_COMPILETIME_FUNCTIONS_HPP__INCLUDED_
