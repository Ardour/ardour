#define PATH_MAX _MAX_PATH
#undef DEBUG
#define _USE_MATH_DEFINES
#define W_OK 2


#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef long ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern void* __imp_lstrcmpiA;
extern void* __imp_CompareStringA;
extern void* __imp_GetSystemTimeAsFileTime;

#ifdef __cplusplus
}
#endif


// Defining strcasecmp without including any headers
static inline int strcasecmp(const char* s1, const char* s2) {
    // Handle NULL pointers defensively (POSIX undefined, but common practice)
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    
    // lstrcmpiA returns -1/0/1 directly (kernel32.dll)
    int (__stdcall *lstrcmpiA)(const char*, const char*) = 
        (int (__stdcall*)(const char*, const char*))__imp_lstrcmpiA;
    return lstrcmpiA(s1, s2);
}

// Defining strncasecmp without including any headers
static inline int strncasecmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return 0;
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    
    // CompareStringA expects int count (truncate size_t values > INT_MAX)
    int (__stdcall *CompareStringA)(unsigned long, unsigned long, const char*, int, const char*, int) = 
        (int (__stdcall*)(unsigned long, unsigned long, const char*, int, const char*, int))__imp_CompareStringA;
    
    // LOCALE_INVARIANT (0x7F) + NORM_IGNORECASE (0x1) for C-locale behavior
    int result = CompareStringA(0x7F, 0x1, s1, (int)n, s2, (int)n);
    
    // Map CSTR_* values: 1->-1, 2->0, 3->1; error returns 0
    return (result == 1) ? -1 : (result == 2) ? 0 : (result == 3) ? 1 : 0;
}

// gettimeofday - Avoids defining timeval, FILETIME, or timezone due to redefinition errors.
// Expects tp to point to a struct with standard layout: { long tv_sec; long tv_usec; }
static inline int gettimeofday(void* tp, void* tzp) {

    static const unsigned long long epoch_diff = 116444736000000000ULL;     // Windows epoch (1601) to Unix epoch (1970) difference in 100-nanosecond ticks

    using Fn = void(__stdcall*)(void*);
    Fn GetSystemTimeAsFileTime = (Fn)__imp_GetSystemTimeAsFileTime;

    // 64-bit buffer for FILETIME value (cannot name the type)
    unsigned long long ft_val = 0;
    GetSystemTimeAsFileTime(&ft_val);

    if (tp) {
        // Convert to microseconds since Unix epoch and fill timeval layout
        unsigned long long total_us = (ft_val - epoch_diff) / 10ULL;
        long* tv = static_cast<long*>(tp);
        tv[0] = static_cast<long>(total_us / 1000000ULL); // tv_sec
        tv[1] = static_cast<long>(total_us % 1000000ULL); // tv_usec
    }

    // tzp is obsolete and cannot be handled without defining 'timezone'
    (void)tzp;
    return 0; // Success
}