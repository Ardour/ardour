#ifdef PLATFORM_WINDOWS
    #include "IncludeWindows.h"
#endif
#if defined(__linux__) || defined(__APPLE__)
	#include <sys/time.h>
#endif

#include "UMicroseconds.h"

namespace wvNS { 
UMicroseconds& UMicroseconds::ReadTime()
{
	// Note: g_get_monotonic_time() may be a viable alternative
	// (it is on Linux and OSX); if not, this code should really go into libpbd
#ifdef PLATFORM_WINDOWS
	LARGE_INTEGER Frequency, Count ;

	QueryPerformanceFrequency(&Frequency) ;
	QueryPerformanceCounter(&Count);
	theTime = uint64_t((Count.QuadPart * 1000000.0 / Frequency.QuadPart));

#elif defined __MACH__ // OSX, BSD..

	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	theTime = (uint64_t)mts.tv_sec * 1e6 + (uint64_t)mts.tv_nsec / 1000;

#else // Linux, POSIX

	struct timespec *ts
	clock_gettime(CLOCK_MONOTONIC, ts);
	theTime = (uint64_t)ts.tv_sec * 1e6 + (uint64_t)buf.tv_nsec / 1000;

#endif

	return *this;
}
/*
 Removed in favor of the posix implementation. 
#ifdef __APPLE__
	uint32_t UMicroseconds::hi() {return reinterpret_cast<UnsignedWide*>(&theTime)->hi;}
	uint32_t UMicroseconds::lo() {return reinterpret_cast<UnsignedWide*>(&theTime)->lo;}
#endif
*/
void UMicrosecondsAccumulator::Start()
{
	m_start_time.ReadTime();
}

void UMicrosecondsAccumulator::Stop()
{
	UMicroseconds stop_time;
	
	m_accumulator += stop_time.GetNativeTime() - m_start_time.GetNativeTime();
}

void UMicrosecondsAccumulator::Clear()
{
	m_start_time = 0;
	m_accumulator = 0;
}

UMicroseconds UMicrosecondsAccumulator::GetAccumulatedTime() const
{
	return m_accumulator;
}

UMicrosecondsAccumulator& UMicrosecondsAccumulator::operator+=(const UMicrosecondsAccumulator& inaccum_to_add)
{
	m_accumulator += inaccum_to_add.GetAccumulatedTime();
	return *this;
}
	
} // namespace wvNS { 
