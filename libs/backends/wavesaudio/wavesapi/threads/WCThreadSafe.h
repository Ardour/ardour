/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifndef __WCThreadSafe_h_
	#define __WCThreadSafe_h_

/* Copy to include
#include "Threads/WCThreadSafe.h"
*/

//
// * WCThreadSafe.h (used to be called XPlatformOSServices.hpp)
// *
// * Consistent C++ interfaces to common Operating System services.
// *
// *
// *
// *
// * Created 2004-December-13 by Udi Barzilai as XPlatformOSServices.hpp
// * Moved to WCThreadSafe.h by Shai 26/10/2005
// * 26/10/2005:	ThreadMutex now inhetites from ThreadMutexInited
// * 				namespace changed to wvThread

#include "WavesPublicAPI/wstdint.h"
#include <string>

#include "BasicTypes/WUDefines.h"

#if defined(__linux__) || defined(__APPLE__)
	#define XPLATFORMOSSERVICES_UNIX  1
#endif

#if defined(_WIN32)
	#define XPLATFORMOSSERVICES_WIN32 1
#endif

#if XPLATFORMOSSERVICES_WIN32
	#define XPLATFORMTHREADS_WINDOWS 1
#elif XPLATFORMOSSERVICES_UNIX
	#define XPLATFORMTHREADS_POSIX   1
#endif
namespace wvNS {
typedef uint32_t WTThreadSafetyType;
const WTThreadSafetyType kNoThreadSafetyNeeded = 0;
const WTThreadSafetyType kpthreadsmutexThreadSafety = 1;


namespace wvThread
{
    //#include "BasicTypes/WavesAPISetAligment.h"
    //Packing affects the layout of classes, and commonly, if packing changes across header files, there can be problems. 
#ifdef PLATFORM_WINDOWS
#pragma pack(push)
#pragma pack(8)
#endif

#ifdef __APPLE__
#ifdef __GNUC__
#pragma pack(push, 8)
#endif
#endif

	//--------------------------------------------------------
	typedef  int32_t timediff;    // in microseconds
	static const timediff ktdOneSecond = 1000*1000;
	//--------------------------------------------------------
	class timestamp
	{
	protected:
		typedef uint32_t tickcount;
		tickcount m_nMicroseconds;  // may wrap around
		static const tickcount ms_knWraparoundThreshold = ~tickcount(0) ^ (~tickcount(0)>>1);  // half the range

	public:
    	timestamp() : m_nMicroseconds(0) { /* uninitialized */ }
    	timestamp(const timestamp &_ts) : m_nMicroseconds(_ts.m_nMicroseconds) {}
    	timestamp &operator=(const timestamp &_rhs) { m_nMicroseconds = _rhs.m_nMicroseconds; return *this; }
    	explicit timestamp(tickcount _i) : m_nMicroseconds(_i) {}
    	uint32_t ticks() const { return m_nMicroseconds; }
		timediff operator-(timestamp _rhs) const { return timediff(m_nMicroseconds-_rhs.m_nMicroseconds); }
		timestamp & operator+=(timediff _t) { m_nMicroseconds+=_t; return *this; }
		timestamp & operator-=(timediff _t) { m_nMicroseconds-=_t; return *this; }
		timestamp operator+(timediff _t) const { return timestamp(m_nMicroseconds+_t); }
		timestamp operator-(timediff _t) const { return timestamp(m_nMicroseconds-_t); }
		bool operator==(timestamp _rhs) const { return m_nMicroseconds==_rhs.m_nMicroseconds; }
		bool operator!=(timestamp _rhs) const { return m_nMicroseconds!=_rhs.m_nMicroseconds; }
		bool operator< (timestamp _rhs) const { return m_nMicroseconds-_rhs.m_nMicroseconds >= ms_knWraparoundThreshold; }
        static timestamp null() { return timestamp(0); }
        bool is_null() const { return m_nMicroseconds==0; }
	};
	//--------------------------------------------------------
#ifdef __APPLE__
	bool FindNetInterfaceByIPAddress(const char *sIP, char *sInterface);
#endif // MACOS
	//--------------------------------------------------------
	timestamp now();
	//--------------------------------------------------------
	DllExport void sleep(timediff);
	DllExport void sleep_milliseconds(unsigned int nMillisecs);
	//--------------------------------------------------------
    void yield();
    //--------------------------------------------------------



	typedef uintptr_t os_dependent_handle_type;

	//--------------------------------------------------------
	typedef int    ThreadFunctionReturnType;
	typedef void * ThreadFunctionArgument;
	//--------------------------------------------------------
	typedef ThreadFunctionReturnType (ThreadFunction)(ThreadFunctionArgument);
	//--------------------------------------------------------
	class ThreadHandle
	{
	public:
		class OSDependent;
	protected:
		uintptr_t m_oshandle;                                                                   // hopefully this is good enough for all systems
	public:
		static const ThreadHandle Invalid;
	protected:
		ThreadHandle(uintptr_t n) : m_oshandle(n) {} 
	public:
		ThreadHandle() : m_oshandle(Invalid.m_oshandle) {}
		bool is_invalid() const { return !m_oshandle || m_oshandle==Invalid.m_oshandle; }
	};
	//--------------------------------------------------------
	class ThreadPriority
	{
	public: enum value { BelowNormal=1, Normal=2, AboveNormal=3, TimeCritical=4 };
	protected: value m_value;
	public: ThreadPriority(value v) : m_value(v) {}
	public: operator value() const { return m_value; }
	};
	//--------------------------------------------------------
	void SetMyThreadPriority(ThreadPriority);
	//--------------------------------------------------------
	ThreadHandle StartThread(ThreadFunction, ThreadFunctionArgument, ThreadPriority=ThreadPriority::Normal);
	bool JoinThread(ThreadHandle, ThreadFunctionReturnType * = 0);
	bool KillThread(ThreadHandle);  // use only for abnormal termination
	void Close(ThreadHandle); // should be called once for every handle obtained from StartThread.
	//--------------------------------------------------------



    
	//--------------------------------------------------------
    class DllExport noncopyableobject
	{
	protected:
		noncopyableobject() {}
	private:
		noncopyableobject(const noncopyableobject &);
		noncopyableobject & operator=(const noncopyableobject &);
	};
	//--------------------------------------------------------


	//--------------------------------------------------------
	// Thread Mutex class that needs to be explicitly initialized
	class DllExport ThreadMutexInited : public noncopyableobject
	{
	protected:
		class OSDependentMutex;
		OSDependentMutex* m_osdmutex;

	public:
		ThreadMutexInited();
		~ThreadMutexInited();

		void init();
		void uninit();
		inline bool is_init() { return 0 != m_osdmutex; }
		void obtain();
		bool tryobtain();
		void release();
	
	private:
		ThreadMutexInited(const ThreadMutexInited&);            // cannot be copied
		ThreadMutexInited& operator=(const ThreadMutexInited&); // cannot be copied

	public:
		class lock : public noncopyableobject
		{
		protected:
			ThreadMutexInited &m_mutex;
		public:
			inline lock(ThreadMutexInited &mtx) : m_mutex(mtx) { m_mutex.obtain(); }
			inline ~lock() { m_mutex.release(); }
		};
		class trylock : public noncopyableobject
		{
		protected:
			ThreadMutexInited &m_mutex;
			bool         m_bObtained;
		public:
			inline trylock(ThreadMutexInited &mtx) : m_mutex(mtx), m_bObtained(false) { m_bObtained = m_mutex.tryobtain(); }
			inline ~trylock() { if (m_bObtained) m_mutex.release(); }
			inline operator bool() const { return m_bObtained; }
		};
	};
	//--------------------------------------------------------

	// Thread Mutex class that is automatically initialized
	class ThreadMutex : public ThreadMutexInited 
	{
	public:
		ThreadMutex() {init();}
	};
	
	//--------------------------------------------------------
	class DllExport ThreadConditionSignal : public noncopyableobject
	{
	protected:
		class OSDependentObject;
		OSDependentObject &m_osdepobj;

	protected:
		void obtain_mutex();
		bool tryobtain_mutex();
		void release_mutex();

	public:
		class lock : public noncopyableobject
		{
		protected:
			ThreadConditionSignal &m_tcs;
		public:
			lock(ThreadConditionSignal &tcs) : m_tcs(tcs) { m_tcs.obtain_mutex(); }
			~lock() { m_tcs.release_mutex(); }
		};
		class trylock : public noncopyableobject
		{
		protected:
			ThreadConditionSignal &m_tcs;
			bool                   m_bObtained;
		public:
			trylock(ThreadConditionSignal &tcs) : m_tcs(tcs), m_bObtained(false) { m_bObtained = m_tcs.tryobtain_mutex(); }
			~trylock() { if (m_bObtained) m_tcs.release_mutex(); }
			operator bool() const { return m_bObtained; }
		};

	public:
		ThreadConditionSignal();
		~ThreadConditionSignal();

		// IMPORTANT: All of the functions below MUST be called ONLY while holding a lock for this object !!!
		void await_condition();
		bool await_condition(timediff tdTimeout);
		void signal_condition_single();
		void signal_condition_broadcast();
	};
	//--------------------------------------------------------





	//--------------------------------------------------------
	// A doorbell is a simple communication mechanism that allows
	// one thread two wake another when there is some work to be done.
	// The signal is 'clear on read'. This class is not intended for
	// multi-way communication (i.e. more than two threads).
//#define XPLATFORMTHREADS_DOORBELL_INLINE_USING_COND_VAR (!XPLATFORMTHREADS_WINDOWS && !XPLATFORMOSSERVICES_MACOS)
#ifdef XPLATFORMTHREADS_DOORBELL_INLINE_USING_COND_VAR
#undef XPLATFORMTHREADS_DOORBELL_INLINE_USING_COND_VAR
#endif
#define XPLATFORMTHREADS_DOORBELL_INLINE_USING_COND_VAR 1
#if XPLATFORMTHREADS_DOORBELL_INLINE_USING_COND_VAR
	class doorbell_type
	{
	protected:
		ThreadConditionSignal m_signal;
		bool m_rang;
	protected:
		template<bool wait_forever> bool wait_for_ring_internal(timediff timeout)
		{// mutex
			ThreadConditionSignal::lock guard(m_signal);
			if (!m_rang)
			{
				if (wait_forever)
				{
					m_signal.await_condition();
				}
				else
				{
					m_signal.await_condition(timeout);
				}
			}
			const bool rang = m_rang;
			m_rang = false;
			return rang;
		}// mutex

	public:
		doorbell_type() : m_rang(false) {}
		inline ~doorbell_type() {}
		inline void ring()
		{// mutex
			ThreadConditionSignal::lock guard(m_signal);
			m_rang = true;
			m_signal.signal_condition_single();
		}// mutex
		inline bool wait_for_ring() { return wait_for_ring_internal<true>(0); }
		inline bool wait_for_ring(timediff timeout) { return wait_for_ring_internal<false>(timeout); }
	};
#else
	class doorbell_type : public noncopyableobject
	{
	protected:
		os_dependent_handle_type m_os_dependent_handle;
	protected:
		template<bool wait_forever> bool wait_for_ring_internal(timediff);
	public:
		doorbell_type();
		~doorbell_type();
		void ring();
		bool wait_for_ring();
		bool wait_for_ring(timediff timeout);
	};
#endif // XPLATFORMTHREADS_DOORBELL_INLINE_USING_COND_VAR
	//--------------------------------------------------------

	//---------------------------------------------------------------
	class DllExport WCThreadRef	// Class which holds the threadRef, DWORD in Windows and pthread_t in POSIX (Mac, Unix)
	{
	public:
		class OSDependent;  // the class which contains the OS Dependent implementation

		WCThreadRef() : m_osThreadRef(0) {}
		bool is_invalid() const { return m_osThreadRef == 0;}

		operator uintptr_t() const {return m_osThreadRef;}

	protected:
		uintptr_t m_osThreadRef;
		WCThreadRef(uintptr_t n) : m_osThreadRef(n) {} 

		friend DllExport bool operator==(const WCThreadRef& first, const WCThreadRef& second);
		friend DllExport bool operator!=(const WCThreadRef& first, const WCThreadRef& second);
		friend DllExport bool operator<(const WCThreadRef& first, const WCThreadRef& second);
		friend DllExport bool operator>(const WCThreadRef& first, const WCThreadRef& second);
	};

	DllExport WCThreadRef GetCurrentThreadRef();	// getting the current thread reference - cross-platform implemented
	bool IsThreadExists(const WCThreadRef& threadRef);	// correct to the very snapshot of time of execution

	//---------------------------------------------------------------

    class DllExport WCAtomicLock
    {
    public:    
        WCAtomicLock() : m_the_lock(0) {}
		bool obtain(const uint32_t in_num_trys = 1);
		void release();
	private:
	    int32_t m_the_lock;
    };

    //#include "BasicTypes/WavesAPIResetAligment.h"
#ifdef PLATFORM_WINDOWS
#pragma pack(pop)
#endif

#ifdef __APPLE__
#ifdef __GNUC__
#pragma pack(pop)
#endif
#endif

class WCStThreadMutexLocker
{
public:
    WCStThreadMutexLocker(wvNS::wvThread::ThreadMutexInited& in_mutex) : 
    m_mutex(in_mutex)
    {
        m_mutex.obtain();
    }
    
    ~WCStThreadMutexLocker()
    {
        m_mutex.release();
    }
protected:
    wvNS::wvThread::ThreadMutexInited& m_mutex;
    WCStThreadMutexLocker(const WCStThreadMutexLocker&);
    WCStThreadMutexLocker& operator=(const WCStThreadMutexLocker&);
};
    
} // namespace wvThread


} //namespace wvNS {
#endif // #ifndef __WCThreadSafe_h_
