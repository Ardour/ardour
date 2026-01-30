#pragma once
#define strncasecmp _strnicmp
#define PATH_MAX _MAX_PATH

#ifdef LoadString
#undef LoadString
#endif