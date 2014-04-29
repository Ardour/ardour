#ifdef _WINDOWS
    #include "IncludeWindows.h"
#endif
#if defined(__linux__) || defined(__MACOS__)
	#include <sys/time.h>
#endif

#include "UMicroseconds.h"

namespace wvNS { 
UMicroseconds& UMicroseconds::ReadTime()
{
#ifdef _WINDOWS
	LARGE_INTEGER Frequency, Count ;

	QueryPerformanceFrequency(&Frequency) ;
	QueryPerformanceCounter(&Count);
	theTime = uint64_t((Count.QuadPart * 1000000.0 / Frequency.QuadPart));
#endif

#if defined(__linux__) || defined(__MACOS__)
//	Mac code replaced by posix calls, to reduce Carbon dependency. 
	timeval buf;

	gettimeofday(&buf,NULL);

	// micro sec
  	theTime = uint64_t(buf.tv_sec) * 1000*1000 + buf.tv_usec;
#endif

	return *this;
}
/*
 Removed in favor of the posix implementation. 
#ifdef __MACOS__
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
