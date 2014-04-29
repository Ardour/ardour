#include "Threads/WCThreadSafe.h"
    
#if XPLATFORMTHREADS_WINDOWS
    #define _WIN32_WINNT 0x0500   // need at least Windows2000 (for TryEnterCriticalSection() and SignalObjectAndWait()
    #include "IncludeWindows.h"
    #include <process.h>
#endif // XPLATFORMTHREADS_WINDOWS


#if defined(__MACOS__)
    #include <CoreServices/CoreServices.h>
    #include <stdio.h>
#endif // __MACOS__

#if XPLATFORMTHREADS_POSIX
    #include </usr/include/unistd.h>   // avoid the framework version and use the /usr/include version
    #include <pthread.h>
    #include <sched.h>
    #include <sys/time.h>
    #include <errno.h>
    #include <signal.h>
// We do this externs because <stdio.h> comes from MSL
extern "C" FILE *popen(const char *command, const char *type);
extern "C" int pclose(FILE *stream);
static int (*BSDfread)( void *, size_t, size_t, FILE * ) = 0;

#include <string.h>

#endif //XPLATFORMTHREADS_POSIX

#include "Akupara/threading/atomic_ops.hpp"
namespace wvNS {
static const unsigned int knMicrosecondsPerSecond = 1000*1000;
static const unsigned int knNanosecondsPerMicrosecond = 1000;
static const unsigned int knNanosecondsPerSecond = knMicrosecondsPerSecond*knNanosecondsPerMicrosecond;

namespace wvThread
{

    //--------------------------------------------------------------------------------
    static inline bool EnsureThreadingInitialized()
    {
        bool bRetval = true;

        return bRetval;
    }
    //--------------------------------------------------------------------------------




    //--------------------------------------------------------------------------------
    static uint32_t CalculateTicksPerMicrosecond();
    static uint32_t CalculateTicksPerMicrosecond()
    {
        uint32_t nTicksPerMicrosecond=0;
#if defined(_WIN32)
        LARGE_INTEGER TSC;
        ::QueryPerformanceFrequency(&TSC);
        nTicksPerMicrosecond = uint32_t (TSC.QuadPart / knMicrosecondsPerSecond);
#elif defined(__linux__) && defined(__i386__)
        static const timediff sktd_TSC_MeasurementPeriod = 40*1000; // delay for CalculateTicksPerMicrosecond() to measure the TSC frequency
        uint64_t Tstart, Tend;
        timeval tvtmp, tvstart, tvend;

        //--------------------- begin measurement code
        // poll to align to a tick of gettimeofday
        ::gettimeofday(&tvtmp,0);
        do { 
            ::gettimeofday(&tvstart,0);
            __asm__ __volatile__ (".byte 0x0f, 0x31" : "=A" (Tstart));  // RDTSC
        } while (tvtmp.tv_usec!=tvstart.tv_usec);
        // delay some
        ::usleep(sktd_TSC_MeasurementPeriod);
        //
        ::gettimeofday(&tvtmp,0);
        do { 
            ::gettimeofday(&tvend,0);
            __asm__ __volatile__ (".byte 0x0f, 0x31" : "=A" (Tend));    // RDTSC
        } while (tvtmp.tv_usec!=tvend.tv_usec);
        //--------------------- end measurement code

        suseconds_t elapsed_usec = (tvend.tv_sec-tvstart.tv_sec)*knMicrosecondsPerSecond + (tvend.tv_usec-tvstart.tv_usec);
        uint64_t elapsed_ticks = Tend-Tstart;
        nTicksPerMicrosecond = uint32_t (elapsed_ticks/elapsed_usec);
#endif
        return nTicksPerMicrosecond;
    }
    
#if defined(__MACOS__) //&& !defined(__MACH__)

    
    bool FindNetInterfaceByIPAddress(const char *sIP, char *sInterface) // sIP and sInterface are both char[16]
    {
        FILE *fProcess , *pSubcall;
        char sLine[256]="", *pToken, sCommand[150];
        bool res = false;
        int iret;

        fProcess = popen("ifconfig -l inet", "r");
        if (fProcess)
        {
            memset(sInterface, '\0', 16);
            iret = BSDfread(sLine, sizeof(char), sizeof(sLine), fProcess);
            pToken = strtok(sLine, " ");
            while (pToken)
            {
                sprintf(sCommand, "ifconfig %s | grep \"inet %s \"", pToken, sIP);
                
                pSubcall = popen(sCommand, "r");
                if (pSubcall)
                {
                    char sSubline[100]="";
                    if (BSDfread(sSubline, sizeof(char), sizeof(sSubline), pSubcall))
                    {
                        // found
                        strcpy(sInterface, pToken);
                        res = true;
                        pclose(pSubcall);
                        break;
                    }
                }
                pclose(pSubcall);
                pToken = strtok(NULL, " ");    
            }
            
        }
        pclose(fProcess);
        
        return res;
    }
#endif // MACOS

    timestamp now(void)
    {
        EnsureThreadingInitialized();
        static const uint32_t nTicksPerMicrosecond = CalculateTicksPerMicrosecond();
#if defined(_WIN32)
        if (nTicksPerMicrosecond)
        {
            LARGE_INTEGER TSC;
            ::QueryPerformanceCounter(&TSC);
            return timestamp(uint32_t(TSC.QuadPart/nTicksPerMicrosecond));
        }
        else return timestamp(0);
#elif defined(__MACOS__)
        if (nTicksPerMicrosecond) {} // prevent 'unused' warnings
        UnsignedWide usecs;
        ::Microseconds(&usecs);
        return timestamp(usecs.lo);
#elif defined(__linux__) && defined(__i386__) && defined(__gnu_linux__)
        uint64_t TSC;
        __asm__ __volatile__ (".byte 0x0f, 0x31" : "=A" (TSC));  // RDTSC
        return timestamp(TSC/nTicksPerMicrosecond);
#elif defined(__linux__) && defined(__PPC__) && defined(__gnu_linux__)
    #warning need to implement maybe
#else
    #error Dont know how to get microseconds timer !
#endif // defined(_WIN32)
    }


    void sleep_milliseconds(unsigned int nMillisecs)
    {
        EnsureThreadingInitialized();
#if XPLATFORMTHREADS_WINDOWS
        ::Sleep(nMillisecs);
#elif XPLATFORMTHREADS_POSIX
        ::usleep(nMillisecs*1000);
#else
    #error Not implemented for your OS
#endif
    }


#if XPLATFORMTHREADS_WINDOWS
    inline DWORD win32_milliseconds(timediff td) { return (td+499)/1000; }
#endif

    void sleep(timediff _td)
    {
        if (_td>0)
        {
            EnsureThreadingInitialized();
#if XPLATFORMTHREADS_WINDOWS
            ::Sleep(win32_milliseconds(_td));    // This is the best we can do in windows
#elif XPLATFORMTHREADS_POSIX
            ::usleep(_td);
#else
    #error Not implemented for your OS
#endif
        }
    }


#if XPLATFORMTHREADS_WINDOWS
    void yield()   {  ::Sleep(0);   }
#elif XPLATFORMTHREADS_POSIX
    void yield()   {  ::sched_yield();  }
#endif
 



    class  ThreadMutexInited::OSDependentMutex : public noncopyableobject
    {
#if defined (XPLATFORMTHREADS_WINDOWS)
    protected:
        CRITICAL_SECTION m_critsec;
    public:

        inline OSDependentMutex()  { EnsureThreadingInitialized(); ::InitializeCriticalSection(&m_critsec); }
        inline ~OSDependentMutex() { EnsureThreadingInitialized(); ::DeleteCriticalSection    (&m_critsec); }
        inline void obtain()       { EnsureThreadingInitialized(); ::EnterCriticalSection     (&m_critsec); }
        inline void release()      { EnsureThreadingInitialized(); ::LeaveCriticalSection     (&m_critsec); }
        inline bool tryobtain()    { EnsureThreadingInitialized(); return TryEnterCriticalSection(&m_critsec)!=FALSE; }
        
#elif defined (XPLATFORMTHREADS_POSIX)
    protected:
        pthread_mutex_t  m_ptmutex;
    public:
        inline OSDependentMutex()  
        { 
            EnsureThreadingInitialized(); 
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            ::pthread_mutex_init (&m_ptmutex, &attr);
        }
        inline ~OSDependentMutex() { EnsureThreadingInitialized(); ::pthread_mutex_destroy(&m_ptmutex); }
        inline void obtain()       { EnsureThreadingInitialized(); ::pthread_mutex_lock       (&m_ptmutex); }
        inline void release()      { EnsureThreadingInitialized(); ::pthread_mutex_unlock     (&m_ptmutex); }
        inline bool tryobtain()    { EnsureThreadingInitialized(); return ::pthread_mutex_trylock(&m_ptmutex)!=EBUSY; }

#endif
    };

    ThreadMutexInited::ThreadMutexInited() :
                        m_osdmutex(0) {}
    
    void ThreadMutexInited::init()
    {
        if (! is_init())
        {
            m_osdmutex = new OSDependentMutex;
        }
    }
    
    void ThreadMutexInited::uninit()
    {
        if (is_init())
        {
            delete m_osdmutex;
            m_osdmutex = 0;
        }
    }
    
    ThreadMutexInited::~ThreadMutexInited()
    {
        uninit();
    }    
    
    void ThreadMutexInited::obtain()
    {
        if (is_init())
        {
            m_osdmutex->obtain(); 
        }
    }    

    void ThreadMutexInited::release()
    {
        if (is_init())
        {
            m_osdmutex->release(); 
        }
    }    
    
    bool ThreadMutexInited::tryobtain()
    {
        bool retVal = true;
        if (is_init())
        {
            retVal = m_osdmutex->tryobtain(); 
        }
        return retVal;
    }    
    
    class ThreadConditionSignal::OSDependentObject : public noncopyableobject
    {
#if defined (XPLATFORMTHREADS_POSIX)

    protected:
        pthread_cond_t  m_ptcond;
        pthread_mutex_t m_ptmutex;
    public:
        inline OSDependentObject()      
        {
            EnsureThreadingInitialized(); 
            ::pthread_mutex_init(&m_ptmutex,0);
            ::pthread_cond_init(&m_ptcond, 0); 
        }
        inline ~OSDependentObject()     { ::pthread_cond_destroy(&m_ptcond), ::pthread_mutex_destroy(&m_ptmutex); }
        inline void signal_unicast()    { ::pthread_cond_signal(&m_ptcond);    }
        inline void signal_broadcast()  { ::pthread_cond_broadcast(&m_ptcond); }
        inline void await_signal()      { ::pthread_cond_wait(&m_ptcond, &m_ptmutex); }
        inline bool await_signal(timediff td) 
        {
            timespec tspecDeadline;
            timeval  tvNow;
            ::gettimeofday(&tvNow,0);
            tspecDeadline.tv_nsec = (tvNow.tv_usec + td%knMicrosecondsPerSecond)*knNanosecondsPerMicrosecond;
            tspecDeadline.tv_sec  = tvNow.tv_sec  + td/knMicrosecondsPerSecond;
            if (!(tspecDeadline.tv_nsec < suseconds_t(knNanosecondsPerSecond)))
                ++tspecDeadline.tv_sec, tspecDeadline.tv_nsec-=knNanosecondsPerSecond;
            return ::pthread_cond_timedwait(&m_ptcond, &m_ptmutex, &tspecDeadline) != ETIMEDOUT;
        }

        void obtain_mutex()    { ::pthread_mutex_lock(&m_ptmutex); }
        bool tryobtain_mutex() { return ::pthread_mutex_trylock(&m_ptmutex)!=EBUSY; }
        void release_mutex()   { ::pthread_mutex_unlock(&m_ptmutex); }


#elif XPLATFORMTHREADS_WINDOWS
    protected:
        unsigned int     m_nWaiterCount;
        CRITICAL_SECTION m_csectWaiterCount;

        HANDLE m_hndSemaphoreSignaller;        // We keep this semaphore always at 0 count (non-signalled). We use it to release a controlled number of threads.
        HANDLE m_hndEventAllWaitersReleased;   // auto-reset
        HANDLE m_hndMutex;                     // the mutex associated with the condition
        bool   m_bBroadcastSignalled;          // means that the last waiter must signal m_hndEventAllWaitersReleased when done waiting

    protected:
        // - - - - - - - - - - - - - - - - - - - - - - - -
        bool await_signal_win32(DWORD dwTimeout)
        {
            ::EnterCriticalSection(&m_csectWaiterCount);
            ++m_nWaiterCount;
            ::LeaveCriticalSection(&m_csectWaiterCount);
            // This is the actual wait for the signal
            bool bWaitSucceeded = ::SignalObjectAndWait(m_hndMutex, m_hndSemaphoreSignaller, dwTimeout, FALSE) == WAIT_OBJECT_0;
            //
            ::EnterCriticalSection(&m_csectWaiterCount);
            bool bLastWaiter = --m_nWaiterCount==0 && m_bBroadcastSignalled;
            ::LeaveCriticalSection(&m_csectWaiterCount);

            // re-acquire the mutex
            if (bLastWaiter)
                ::SignalObjectAndWait(m_hndEventAllWaitersReleased, m_hndMutex, INFINITE, FALSE);
            else
                ::WaitForSingleObject(m_hndMutex, INFINITE);
            return bWaitSucceeded;
        }


    public:

        inline bool await_signal(timediff td)  { return await_signal_win32((win32_milliseconds(td))); }
        inline void await_signal()             { await_signal_win32(INFINITE); }

        OSDependentObject() : m_nWaiterCount(0), m_bBroadcastSignalled(false)
        {
            EnsureThreadingInitialized();
            ::InitializeCriticalSection(&m_csectWaiterCount);
            m_hndEventAllWaitersReleased = ::CreateEvent(
                    0,      // security
                    FALSE,  // auto-reset
                    FALSE,  // initial state non-sognalled
                    0);     // name
            m_hndSemaphoreSignaller = ::CreateSemaphore(
                    0,         // security
                    0,         // initial count (and will stay this way)
                    0x100000,  // maximum count (should be as large as the maximum number of waiting threads)
                    0);        // name
            m_hndMutex = ::CreateMutex(
                    0,         // security
                    FALSE,     // not owned initially
                    0);        // name
            //if (m_hndEventAllWaitersReleased==INVALID_HANDLE_VALUE || m_hndSemaphoreSignaller==INVALID_HANDLE_VALUE)
            //    throw something();
        }

        ~OSDependentObject()
        {
            ::CloseHandle(m_hndMutex);
            ::CloseHandle(m_hndSemaphoreSignaller);
            ::CloseHandle(m_hndEventAllWaitersReleased);
            ::DeleteCriticalSection(&m_csectWaiterCount);
        }

        inline void signal_unicast()
        {
            ::EnterCriticalSection(&m_csectWaiterCount);
            unsigned int nWaiters = m_nWaiterCount;
            ::LeaveCriticalSection(&m_csectWaiterCount);
            if (nWaiters)
                ::ReleaseSemaphore(m_hndSemaphoreSignaller, 1, 0);  // release 1 semaphore credit to release one waiting thread
        }

        void signal_broadcast()
        {
            ::EnterCriticalSection(&m_csectWaiterCount);
            unsigned int nWaiters = m_nWaiterCount;
            if (nWaiters)
            {
                m_bBroadcastSignalled = true;
                ::ReleaseSemaphore(m_hndSemaphoreSignaller, nWaiters, 0);  // release as many credits as there are waiting threads
                ::LeaveCriticalSection(&m_csectWaiterCount);
                ::WaitForSingleObject(m_hndEventAllWaitersReleased, INFINITE);
                // at this point all threads are waiting on m_hndMutex, which would be released outside this function call
                m_bBroadcastSignalled = false;
            }
            else
                // no one is waiting
                ::LeaveCriticalSection(&m_csectWaiterCount);
        }
        //------------------------------------------------
        inline void obtain_mutex()    { ::WaitForSingleObject(m_hndMutex, INFINITE); }
        inline bool tryobtain_mutex() { return ::WaitForSingleObject(m_hndMutex,0) == WAIT_OBJECT_0; }
        inline void release_mutex()   { ::ReleaseMutex(m_hndMutex); }
        //------------------------------------------------
#endif // OS switch
    };

    void ThreadConditionSignal::obtain_mutex()    
    { 
        m_osdepobj.obtain_mutex();    
    }
    bool ThreadConditionSignal::tryobtain_mutex() { return m_osdepobj.tryobtain_mutex(); }
    void ThreadConditionSignal::release_mutex()   
    { 
        m_osdepobj.release_mutex();   
    }

    void ThreadConditionSignal::await_condition()                   { m_osdepobj.await_signal();  }
    bool ThreadConditionSignal::await_condition(timediff tdTimeout) { return m_osdepobj.await_signal(tdTimeout);  }
    void ThreadConditionSignal::signal_condition_single()           { m_osdepobj.signal_unicast();    }
    void ThreadConditionSignal::signal_condition_broadcast()        { m_osdepobj.signal_broadcast();  }

    ThreadConditionSignal::ThreadConditionSignal() : m_osdepobj(*new OSDependentObject) {}
    ThreadConditionSignal::~ThreadConditionSignal() { delete &m_osdepobj; }








#if XPLATFORMTHREADS_POSIX
    namespace // anon
    {
        inline int max_FIFO_schedparam()
        {
            static const int max_priority = ::sched_get_priority_max(SCHED_FIFO);
            return max_priority;
        }
        inline int schedparam_by_percentage(unsigned short percentage)
        {
            return (max_FIFO_schedparam()*10*percentage+500)/1000;
        }
        class POSIXThreadPriority
        {
        public:
            int m_SchedPolicy;
            int m_SchedPriority;
            POSIXThreadPriority(ThreadPriority pri)
            {
                switch (pri)
                {
                case ThreadPriority::TimeCritical: m_SchedPolicy=SCHED_FIFO, m_SchedPriority=schedparam_by_percentage(80); break;
                case ThreadPriority::AboveNormal:  m_SchedPolicy=SCHED_FIFO, m_SchedPriority=schedparam_by_percentage(20); break;
                case ThreadPriority::BelowNormal:  // fall through to normal; nothing is below normal in POSIX
                case ThreadPriority::Normal: // fall through to default
                default: m_SchedPolicy=SCHED_OTHER, m_SchedPriority=0; break;
                }
            }
        };

    } // namespace anonymous
#endif // XPLATFORMTHREADS_POSIX

#if XPLATFORMTHREADS_WINDOWS
    namespace // anon
    {
        inline int WinThreadPriority(ThreadPriority pri)
        {
            switch (pri)
            {
            case ThreadPriority::BelowNormal:  return THREAD_PRIORITY_BELOW_NORMAL;
            case ThreadPriority::AboveNormal:  return THREAD_PRIORITY_ABOVE_NORMAL;
            case ThreadPriority::TimeCritical: return THREAD_PRIORITY_TIME_CRITICAL;
            case ThreadPriority::Normal: // fall through to default
            default: return THREAD_PRIORITY_NORMAL;
            }
        }
    } // namespace anon
#endif // XPLATFORMTHREADS_WINDOWS



    void SetMyThreadPriority(ThreadPriority pri)
    {
#if XPLATFORMTHREADS_WINDOWS
        ::SetThreadPriority(::GetCurrentThread(), WinThreadPriority(pri));
#endif // XPLATFORMTHREADS_WINDOWS
#if XPLATFORMTHREADS_POSIX
        const POSIXThreadPriority posixpri(pri);
        sched_param sparam;
        ::memset(&sparam, 0, sizeof(sparam));
        sparam.sched_priority = posixpri.m_SchedPriority;
#if defined(__linux__)
        ::sched_setscheduler(0, posixpri.m_SchedPolicy, &sparam);  // linux uses this function instead of pthread_
#else
        pthread_setschedparam(pthread_self(), posixpri.m_SchedPolicy, &sparam);
#endif
#endif // XPLATFORMTHREADS_POSIX
    }


    struct ThreadWrapperData
    {
        ThreadFunction *func;
        ThreadFunctionArgument arg;
    };

#if XPLATFORMTHREADS_WINDOWS
    static unsigned int __stdcall ThreadWrapper(void * arg)
    {
        register ThreadWrapperData *twd = reinterpret_cast<ThreadWrapperData*>(arg);
        ThreadFunction        *func=twd->func;
        ThreadFunctionArgument farg=twd->arg;
        delete twd;
        return DWORD(func(farg));
    }
#elif XPLATFORMTHREADS_POSIX
    static void * ThreadWrapper(void *arg)
    {
        register ThreadWrapperData *twd = reinterpret_cast<ThreadWrapperData*>(arg);
        ThreadFunction        *func=twd->func;
        ThreadFunctionArgument farg=twd->arg;
        delete twd;
        return reinterpret_cast<void*>(func(farg));
    }
    typedef void*(ThreadWrapperFunction)(void*);

    static ThreadWrapperFunction *ThunkedThreadWrapper = ThreadWrapper;

#endif // OS switch





    class ThreadHandle::OSDependent
    {
    public:
        static void StartThread(ThreadWrapperData *, ThreadHandle &, ThreadPriority);
        static bool KillThread(ThreadHandle);
        static bool JoinThread(ThreadHandle, ThreadFunctionReturnType*);
        static void Close(ThreadHandle);
#if XPLATFORMTHREADS_WINDOWS
        static inline uintptr_t from_oshandle(HANDLE h) { return reinterpret_cast<uintptr_t>(h); }
        static inline HANDLE to_oshandle(uintptr_t h) { return reinterpret_cast<HANDLE>(h); }
#elif XPLATFORMTHREADS_POSIX
        static inline uintptr_t from_oshandle(pthread_t pt) { return uintptr_t(pt); }
        static inline pthread_t to_oshandle(uintptr_t h) { return pthread_t(h); }
#endif // OS switch
    };

#if XPLATFORMTHREADS_WINDOWS
    const ThreadHandle ThreadHandle::Invalid(OSDependent::from_oshandle(INVALID_HANDLE_VALUE));
#elif XPLATFORMTHREADS_POSIX
    const ThreadHandle ThreadHandle::Invalid(OSDependent::from_oshandle(0));
#endif // OS switch

    inline void ThreadHandle::OSDependent::StartThread(ThreadWrapperData *ptwdata, ThreadHandle &th, ThreadPriority pri)
    {
#if XPLATFORMTHREADS_WINDOWS
        uintptr_t h = ::_beginthreadex(
                0,                 // no security attributes, not inheritable
                0,                 // default stack size
                ThreadWrapper,     // function to call
                (void*)(ptwdata),   // argument for function
                0,                 // creation flags
                0                  // where to store thread ID
            );

        if (h) 
        {
            th.m_oshandle = h;
            if (pri!=ThreadPriority::Normal)
                ::SetThreadPriority(to_oshandle(h), WinThreadPriority(pri));
        }
        else
            th=Invalid;
#elif XPLATFORMTHREADS_POSIX
        pthread_attr_t my_thread_attr, *pmy_thread_attr = 0;
        sched_param my_schedparam;

        if (pri!=ThreadPriority::Normal)
        {
            pmy_thread_attr = &my_thread_attr;

            const POSIXThreadPriority posixpriority(pri);
            int result;
            result = pthread_attr_init          (pmy_thread_attr);
            result = pthread_attr_setschedpolicy(pmy_thread_attr, posixpriority.m_SchedPolicy);

            memset(&my_schedparam, 0, sizeof(my_schedparam));
            my_schedparam.sched_priority = posixpriority.m_SchedPriority;
            result = pthread_attr_setschedparam(pmy_thread_attr, &my_schedparam);
        }

        pthread_t pt;
        int anyerr = pthread_create(
                &pt,   // variable for thread handle
                pmy_thread_attr,     // default attributes
                ThunkedThreadWrapper,
                ptwdata
            );
            
        if (anyerr) 
            th=Invalid;
        else
            th.m_oshandle = OSDependent::from_oshandle(pt);
#endif
    }

    inline bool ThreadHandle::OSDependent::KillThread(ThreadHandle h)
    {
#if XPLATFORMTHREADS_WINDOWS
        return ::TerminateThread(to_oshandle(h.m_oshandle), (DWORD)-1) != 0;
#elif XPLATFORMTHREADS_POSIX
        return pthread_cancel(to_oshandle(h.m_oshandle)) == 0;
#endif
    }

    bool ThreadHandle::OSDependent::JoinThread(ThreadHandle h, ThreadFunctionReturnType *pretval)
    {
#if XPLATFORMTHREADS_WINDOWS
        const bool kbReturnedOk = (WAIT_OBJECT_0 == ::WaitForSingleObject(OSDependent::to_oshandle(h.m_oshandle), INFINITE));
        if (kbReturnedOk && pretval)
        {
            DWORD dwExitCode;
            ::GetExitCodeThread(to_oshandle(h.m_oshandle), &dwExitCode);
            *pretval = (ThreadFunctionReturnType)(dwExitCode);
        }
        return kbReturnedOk;
#endif
#if XPLATFORMTHREADS_POSIX
        ThreadFunctionReturnType ptrExitCode = 0;
        int join_return_code = pthread_join(to_oshandle(h.m_oshandle), (void**)ptrExitCode);
        const bool kbReturnedOk = (0 == join_return_code);
        if (0 != pretval)
        {
            *pretval = ptrExitCode;
        }
        return kbReturnedOk;
#endif
    }

#if XPLATFORMTHREADS_WINDOWS
    inline void ThreadHandle::OSDependent::Close(ThreadHandle h)
    {
        ::CloseHandle(OSDependent::to_oshandle(h.m_oshandle));
    }
#endif // XPLATFORMTHREADS_WINDOWS
#if XPLATFORMTHREADS_POSIX
    inline void ThreadHandle::OSDependent::Close(ThreadHandle) {}
#endif // XPLATFORMTHREADS_POSIX

    //**********************************************************************************************

    class WCThreadRef::OSDependent
    {
    public:
        static void GetCurrentThreadRef(WCThreadRef& tid); 
#if XPLATFORMTHREADS_WINDOWS
        static inline uintptr_t from_os(DWORD thread_id) { return (uintptr_t)(thread_id); }
        static inline DWORD to_os(uintptr_t thread_id)   { return (DWORD)(thread_id); }
#elif XPLATFORMTHREADS_POSIX
    static inline uintptr_t from_os(pthread_t thread_id) { return (uintptr_t)(thread_id); }
    static inline pthread_t to_os(uintptr_t thread_id)   { return pthread_t(thread_id); }
#endif // OS switch
    };

    //**********************************************************************************************
    inline void WCThreadRef::OSDependent::GetCurrentThreadRef(WCThreadRef& tid)
    {
#if XPLATFORMTHREADS_WINDOWS
        DWORD thread_id = ::GetCurrentThreadId();
        tid.m_osThreadRef = OSDependent::from_os(thread_id);

#elif XPLATFORMTHREADS_POSIX
        pthread_t thread_id = ::pthread_self();
        tid.m_osThreadRef = OSDependent::from_os(thread_id);

#endif // OS switch
    }

    //**********************************************************************************************

    ThreadHandle StartThread(ThreadFunction func, ThreadFunctionArgument arg, ThreadPriority thpri)
    {
        EnsureThreadingInitialized();
        ThreadWrapperData *ptwdata = new ThreadWrapperData;
        ptwdata->func = func;
        ptwdata->arg  = arg;
        ThreadHandle thToReturn;
        ThreadHandle::OSDependent::StartThread(ptwdata, thToReturn, thpri);
        return thToReturn;
    }

    bool KillThread(ThreadHandle h)
    {
        EnsureThreadingInitialized();
        return ThreadHandle::OSDependent::KillThread(h);
    }

    bool JoinThread(ThreadHandle h, ThreadFunctionReturnType *pretval)
    {
        EnsureThreadingInitialized();
        return ThreadHandle::OSDependent::JoinThread(h, pretval);
    }

    void Close(ThreadHandle h)
    {
        EnsureThreadingInitialized();
        return ThreadHandle::OSDependent::Close(h);
    }

    //*******************************************************************************************

    WCThreadRef GetCurrentThreadRef()
    {
        EnsureThreadingInitialized(); // Is it necessary?  
        WCThreadRef tRefToReturn;
        WCThreadRef::OSDependent::GetCurrentThreadRef(tRefToReturn);
        return tRefToReturn;
    }

    //*******************************************************************************************

    bool IsThreadExists(const WCThreadRef& threadRef)
    {
#if XPLATFORMTHREADS_WINDOWS
        DWORD dwThreadId = WCThreadRef::OSDependent::to_os((uintptr_t)threadRef);
        HANDLE handle = ::OpenThread(SYNCHRONIZE, // dwDesiredAccess - use of the thread handle in any of the wait functions
                                     FALSE,          // bInheritHandle  - processes do not inherit this handle
                                     dwThreadId);

        // Now we have the handle, check if the associated thread exists:
        DWORD retVal = WaitForSingleObject(handle, 0);
        if (retVal == WAIT_FAILED)
            return false;    // the thread does not exists
        else
            return true;    // the thread exists

#elif XPLATFORMTHREADS_POSIX
        pthread_t pthreadRef = WCThreadRef::OSDependent::to_os((uintptr_t)threadRef);
        int retVal = pthread_kill(pthreadRef, 0);    // send a signal to the thread, but do nothing
        if (retVal == ESRCH)
            return false;    // the thread does not exists
        else
            return true;    // the thread exists

#endif // OS switch
    }

    //*******************************************************************************************

    bool operator==(const WCThreadRef& first, const WCThreadRef& second)
    {
        return (first.m_osThreadRef == second.m_osThreadRef);
    }

    bool operator!=(const WCThreadRef& first, const WCThreadRef& second)
    {
        return (first.m_osThreadRef != second.m_osThreadRef);
    }

    bool operator<(const WCThreadRef& first, const WCThreadRef& second)
    {
        return (first.m_osThreadRef < second.m_osThreadRef);
    }

    bool operator>(const WCThreadRef& first, const WCThreadRef& second)
    {
        return (first.m_osThreadRef > second.m_osThreadRef);
    }

    bool WCAtomicLock::obtain(const uint32_t in_num_trys)
    {
        bool retVal = false;
        
        uint32_t timeOut = in_num_trys;
        while (true)
        {
            retVal = Akupara::threading::atomic::compare_and_store<int32_t>(&m_the_lock, int32_t(0), int32_t(1));
            if (retVal)
            {
                break;
            }
            else
            {
                if (--timeOut == 0)
                {
                    break;
                }
                sleep_milliseconds(1000);
            }
        }
        
        return retVal;
    }

    void WCAtomicLock::release()
    {
        m_the_lock = 0;
    }

} //    namespace wvThread
} // namespace wvNS {

