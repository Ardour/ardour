#include <iostream>
#include <string>

#include <sys/utsname.h>
#include <curl/curl.h>

#include "pbd/compose.h"
#include "ardour/callback.h"

using namespace std;

#define PING_URL "http://ardour.org/pingback/versioncheck"

static size_t
curl_write_data (char *bufptr, size_t size, size_t nitems, void *ptr)
{
        return size * nitems;
}

static string
watermark ()
{
        return "";
}

void
call_the_mothership (const string& version)
{
        CURL* c;
        struct utsname utb;

        if (uname (&utb)) {
                return;
        }
        
        curl_global_init (CURL_GLOBAL_NOTHING);

        c = curl_easy_init ();

        string data;
        string wm;

        data = string_compose ("version=%1&platform=%2 %3 %4", version, utb.sysname, utb.release, utb.machine);

        wm = watermark();
        if (!wm.empty()) {
                data += string_compose ("&watermark=%1", wm);
        }
        
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(c, CURLOPT_URL, PING_URL);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_data); 
        curl_easy_setopt(c, CURLOPT_WRITEDATA, 0); 
        
        std::cerr << "Callback to ardour.org ...\n";

        char errbuf[CURL_ERROR_SIZE];
        curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf); 

        if (curl_easy_perform (c) == 0) {

        }
        
        curl_easy_cleanup (c);
}
