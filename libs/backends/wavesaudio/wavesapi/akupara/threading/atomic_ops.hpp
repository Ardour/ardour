/*
*  Akupara/threading/atomic_ops.hpp
*  
*
*  Created by Udi Barzilai on 06/06.
*  Copyright 2006 __MyCompanyName__. All rights reserved.
*
*/
#if !defined(_AKUPARA_THREADING_ATOMIC_OPS_HPP__INCLUDED_)
#define _AKUPARA_THREADING_ATOMIC_OPS_HPP__INCLUDED_

#include "Akupara/basics.hpp"  // for EXPECT macro
#include "Akupara/compiletime_functions.hpp" // for TR1 stuff, signed/unsigned stuff

namespace Akupara
{
    namespace threading
    {
        namespace atomic
        {
            namespace machine
            {
                // Machine capabilities
                // The following templates are specialized by the machine-specific headers to indicate
                // the capabilities of the machine being compiled for. A true 'value' member for a given
                // byte count means that there is an implementation of the corresponding atomic operation.
                //-------------------------------------
                template<unsigned int _byte_count> struct implements_load          : public false_type {};  // simple assignment from memory (assumes naturally aligned address)
                template<unsigned int _byte_count> struct implements_store         : public false_type {};  // simple assignment to memory (assumes naturally aligned address)
                template<unsigned int _byte_count> struct implements_CAS           : public false_type {};  // compare_and_store()
                template<unsigned int _byte_count> struct implements_LL_SC         : public false_type {};  // load_linked(), store_conditional()
                template<unsigned int _byte_count> struct implements_add           : public false_type {};  // add(), subtract()
                template<unsigned int _byte_count> struct implements_fetch_and_add : public false_type {};  // fetch_and_add(), fetch_and_subtract()
                template<unsigned int _byte_count> struct implements_add_and_fetch : public false_type {};  // add_and_fetch(), subtract_and_fetch()
                //-------------------------------------


                //-------------------------------------
                // functions in this namespace may or may not be implemented, for any integer types, as specified by the machine capabilities templates above
                template<typename _integer_type> bool compare_and_store(volatile _integer_type * operand_address, const _integer_type & expected_value, const _integer_type & value_to_store);

                template<typename _integer_type> _integer_type load_linked(volatile _integer_type * operand_address);
                template<typename _integer_type> bool store_conditional(volatile _integer_type * operand_address, const _integer_type & value_to_store);

                template<typename _integer_type> void add(volatile _integer_type * operand_address, const _integer_type & addend);
                template<typename _integer_type> void subtract(volatile _integer_type * operand_address, const _integer_type & subtrahend);

                template<typename _integer_type> _integer_type fetch_and_add(volatile _integer_type * operand_address, const _integer_type & addend);
                template<typename _integer_type> _integer_type fetch_and_subtract(volatile _integer_type * operand_address, const _integer_type & subtrahend);

                template<typename _integer_type> _integer_type add_and_fetch(volatile _integer_type * operand_address, const _integer_type & addend);
                template<typename _integer_type> _integer_type subtract_and_fetch(volatile _integer_type * operand_address, const _integer_type & subtrahend);

                void memory_barrier_read();
                void memory_barrier_write();
                void memory_barrier_readwrite();
                //-------------------------------------

            } // namespace machine
        } // namespace atomic
    } // namespace threading
} // namespace Akupara

// Include the machine-specific implementations; these only implement the templates above for some of the _signed_ integer types
#if defined(__GNUC__) && defined(__POWERPC__)
#include "atomic_ops_gcc_ppc.hpp"
#endif // defined(__GNUC__) && defined(__POWERPC__)

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include "atomic_ops_gcc_x86.hpp"
#endif // defined(__GNUC__) && defined(__i386__)

#if defined(_MSC_VER) && defined(_M_IX86)
#include "atomic_ops_msvc_x86.hpp"
#endif // defined(_MSC_VER) && defined(_M_IX86)

#if defined(_MSC_VER) && defined(_M_X64)
#include "atomic_ops_msvc_x86_64.hpp"
#endif // defined(_MSC_VER) && defined(_M_X64)

namespace Akupara
{
    namespace threading
    {
        namespace atomic
        {


            // Select the most convenient atomic integer type based on the machine's ability to load/store atomically
            // The definition below selects that largest atomically accessible integer up to the size of int
            //----------------------------------------------------------------------------------------
            namespace detail
            {
                template<unsigned int _byte_count> 
                struct largest_atomic_byte_count_upto 
                { 
                    static const unsigned int value = 
                        machine::implements_load<_byte_count>::value && machine::implements_store<_byte_count>::value ? 
_byte_count : 
                    largest_atomic_byte_count_upto<_byte_count/2>::value; 
                };

                template<> 
                struct largest_atomic_byte_count_upto<0> { static const unsigned int value = 0; };

                const unsigned int k_byte_count_best_atomic = largest_atomic_byte_count_upto<sizeof(int)>::value;
            }
            typedef   signed_integer_with_byte_count< detail::k_byte_count_best_atomic >::type signed_integer_type;
            typedef unsigned_integer_with_byte_count< detail::k_byte_count_best_atomic >::type unsigned_integer_type;
            typedef signed_integer_type integer_type;
            //----------------------------------------------------------------------------------------

            //----------------------------------------------------------------------------------------
            // These need to be implemented by all machines
            using machine::memory_barrier_read;
            using machine::memory_barrier_write;
            using machine::memory_barrier_readwrite;
            //----------------------------------------------------------------------------------------

            //----------------------------------------------------------------------------------------
            // These may or may not be implemented, but if they aren't, we can't help much
            using machine::load_linked;
            using machine::store_conditional;
            //----------------------------------------------------------------------------------------


            //----------------------------------------------------------------------------------------
            // CAS implementation
            namespace detail
            {
                template<
                    typename _integer_type, 
                    bool _implements_CAS   = machine::implements_CAS  <sizeof(_integer_type)>::value,
                    bool _implements_LL_SC = machine::implements_LL_SC<sizeof(_integer_type)>::value>
                struct implementation_CAS
                {
                    static const bool s_exists = false;
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization for native CAS support
                template<typename _integer_type, bool _implements_LL_SC> 
                struct implementation_CAS<_integer_type, true, _implements_LL_SC>
                {
                    static const bool s_exists = true;
                    static inline bool compare_and_store(volatile _integer_type * operand_address, const _integer_type & expected_value, const _integer_type & value_to_store)
                    {
                        return machine::compare_and_store(operand_address, expected_value, value_to_store);
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization for cases with no CAS but with LL/SC
                template<typename _integer_type>
                struct implementation_CAS<_integer_type, false, true>
                {
                    static const bool s_exists = true;
                    static inline bool compare_and_store(volatile _integer_type * operand_address, const _integer_type & expected_value, const _integer_type & value_to_store)
                    {
                        while (machine::load_linked(operand_address) == expected_value)
                            if (AKUPARA_EXPECT_TRUE(machine::store_conditional(operand_address, value_to_store)))
                                return true;
                        return false;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            } // namespace detail
            // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            template<typename _integer_type> 
            inline bool compare_and_store(volatile _integer_type * operand_address, const _integer_type & expected_value, const _integer_type & value_to_store)
            {
                // if your compiler can't find the function to call here then there is no implementation available for your machine
                return detail::implementation_CAS<_integer_type>::compare_and_store(operand_address, expected_value, value_to_store);
            }
            //----------------------------------------------------------------------------------------





            //----------------------------------------------------------------------------------------
            // fetch_and_add
            namespace detail
            {
                template<
                    typename _integer_type, 
                    bool _0 = machine::implements_fetch_and_add<sizeof(_integer_type)>::value,
                    bool _1 = machine::implements_add_and_fetch<sizeof(_integer_type)>::value,
                    bool _2 = machine::implements_LL_SC        <sizeof(_integer_type)>::value,
                    bool _3 = machine::implements_CAS          <sizeof(_integer_type)>::value>
                struct implementation_FAA
                {
                    static const bool s_exists = false;
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization for native support
                template<typename _integer_type, bool _1, bool _2, bool _3>
                struct implementation_FAA<_integer_type, true, _1, _2, _3>
                {
                    static const bool s_exists = true;
                    static inline _integer_type fetch_and_add(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        return machine::fetch_and_add(operand_address, addend);
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization using add_and_fetch
                template<typename _integer_type, bool _2, bool _3>
                struct implementation_FAA<_integer_type, false, true, _2, _3>
                {
                    static const bool s_exists = true;
                    static inline _integer_type fetch_and_add(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        return machine::add_and_fetch(operand_address, addend) - addend;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization using LL/SC
                template<typename _integer_type, bool _3>
                struct implementation_FAA<_integer_type, false, false, true, _3>
                {
                    static const bool s_exists = true;
                    static inline _integer_type fetch_and_add(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        _integer_type old_value;
                        do
                        old_value  = machine::load_linked(operand_address);
                        while (!machine::store_conditional(operand_address, old_value+addend));
                        return old_value;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization using CAS
                template<typename _integer_type>
                struct implementation_FAA<_integer_type, false, false, false, true>
                {
                    static const bool s_exists = true;
                    static inline _integer_type fetch_and_add(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        _integer_type old_value;
                        do
                        old_value  = *operand_address;
                        while (!machine::compare_and_store(operand_address, old_value, old_value+addend));
                        return old_value;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            } // namespace detail
            template<typename _integer_type> 
            inline _integer_type fetch_and_add(volatile _integer_type * operand_address, const _integer_type & addend)
            {
                // if your compiler can't find the function to call here then there is no implementation available for your machine
                return detail::implementation_FAA<_integer_type>::fetch_and_add(operand_address, addend);
            }
            //----------------------------------------------------------------------------------------




            //----------------------------------------------------------------------------------------
            // add_and_fetch
            namespace detail
            {
                template<
                    typename _integer_type, 
                    bool _0 = machine::implements_add_and_fetch<sizeof(_integer_type)>::value,
                    bool _1 = machine::implements_fetch_and_add<sizeof(_integer_type)>::value,
                    bool _2 = machine::implements_LL_SC        <sizeof(_integer_type)>::value,
                    bool _3 = machine::implements_CAS          <sizeof(_integer_type)>::value>
                struct implementation_AAF
                {
                    static const bool s_exists = false;
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization for native support
                template<typename _integer_type, bool _1, bool _2, bool _3>
                struct implementation_AAF<_integer_type, true, _1, _2, _3>
                {
                    static const bool s_exists = true;
                    static inline _integer_type add_and_fetch(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        return machine::add_and_fetch(operand_address, addend);
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization using add_and_fetch
                template<typename _integer_type, bool _2, bool _3>
                struct implementation_AAF<_integer_type, false, true, _2, _3>
                {
                    static const bool s_exists = true;
                    static inline _integer_type add_and_fetch(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        return machine::fetch_and_add(operand_address, addend) + addend;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization using LL/SC
                template<typename _integer_type, bool _3>
                struct implementation_AAF<_integer_type, false, false, true, _3>
                {
                    static const bool s_exists = true;
                    static inline _integer_type add_and_fetch(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        _integer_type new_value;
                        do
                        new_value  = machine::load_linked(operand_address)+addend;
                        while (!machine::store_conditional(operand_address, new_value));
                        return new_value;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                // specialization using CAS
                template<typename _integer_type>
                struct implementation_AAF<_integer_type, false, false, false, true>
                {
                    static const bool s_exists = true;
                    static inline _integer_type add_and_fetch(volatile _integer_type * operand_address, const _integer_type & addend)
                    {
                        _integer_type old_value, new_value;
                        do
                        old_value = *operand_address, new_value  = old_value + addend;
                        while (!machine::compare_and_store(operand_address, old_value, new_value));
                        return new_value;
                    }
                };
                // - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
            } // namespace detail
            template<typename _integer_type> 
            inline _integer_type add_and_fetch(volatile _integer_type * operand_address, const _integer_type & addend)
            {
                // if your compiler can't find the function to call here then there is no implementation available for your machine
                return detail::implementation_AAF<_integer_type>::add_and_fetch(operand_address, addend);
            }
            //----------------------------------------------------------------------------------------



            //----------------------------------------------------------------------------------------
            // add
            template<typename _integer_type> 
            inline void add(volatile _integer_type * operand_address, const _integer_type & addend)
            {
                if (machine::implements_add<sizeof(_integer_type)>::value)
                    machine::add(operand_address, addend);
                else if (machine::implements_fetch_and_add<sizeof(_integer_type)>::value)
                    machine::fetch_and_add(operand_address, addend);
                else if (machine::implements_add_and_fetch<sizeof(_integer_type)>::value)
                    machine::add_and_fetch(operand_address, addend);
                else
                    fetch_and_add(operand_address, addend); // this will simulate using CAS or LL/SC (or it will fail the compilation if neither is available)
            }
            //----------------------------------------------------------------------------------------



            //----------------------------------------------------------------------------------------
            // TODO: this is where we add implementations for:
            // - functions not implemented by the machine
            // - functions that take unsigned types (routed to call the signed versions with appropriate conversions)
            // For now we add nothing, so developers will need to stick to what their machine can do, and use signed
            // integers only.
            using machine::subtract;
            using machine::subtract_and_fetch;
            using machine::fetch_and_subtract;
            //----------------------------------------------------------------------------------------



            //---------------------------------------------------------------------
            template<class _base_type, unsigned int _bytes_per_cache_line=machine::k_bytes_per_cache_line>
            struct pad_to_cache_line : public _base_type
            {
            private:
                typedef pad_to_cache_line this_type;
                typedef _base_type base_type;
            public:
                static const unsigned int s_bytes_per_cache_line = _bytes_per_cache_line;
            private:
                int m_padding[(s_bytes_per_cache_line - sizeof(base_type))/sizeof(int)];
            public:
                pad_to_cache_line() {}
                template<typename _arg_type> pad_to_cache_line(_arg_type arg) : base_type(arg) {}
            };	
            //---------------------------------------------------------------------

        } // namespace atomic
    } // namespace threading
} // namespace Akupara

#endif // _AKUPARA_THREADING_ATOMIC_OPS_HPP__INCLUDED_
