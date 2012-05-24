#include <iostream>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <sys/utsname.h>
#include <curl/curl.h>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/strsplit.h"
#include "pbd/convert.h"

#include "ardour/filesystem_paths.h"

using namespace std;

#define PING_URL "http://ardour.org/pingback/versioncheck"
#define OFF_THE_HOOK ".offthehook"

static size_t
curl_write_data (char *bufptr, size_t size, size_t nitems, void *ptr)
{
        /* we know its a string */

        string* sptr = (string*) ptr;

        for (size_t i = 0; i < nitems; ++i) {
                for (size_t n = 0; n < size; ++n) {
                        if (*bufptr == '\n') {
                                break;
                        }

                        (*sptr) += *bufptr++;
                }
        }

        return size * nitems;
}

static string
watermark ()
{
        return string();
}

void
block_mothership ()
{
        string hangup = Glib::build_filename (ARDOUR::user_config_directory().to_string(), OFF_THE_HOOK);
        int fd;
        if ((fd = ::open (hangup.c_str(), O_RDWR|O_CREAT, 0600)) >= 0) {
                close (fd);
        }
}

void
unblock_mothership ()
{
        string hangup = Glib::build_filename (ARDOUR::user_config_directory().to_string(), OFF_THE_HOOK);
        ::unlink (hangup.c_str());
}

bool
mothership_blocked ()
{
        string hangup = Glib::build_filename (ARDOUR::user_config_directory().to_string(), OFF_THE_HOOK);
        return Glib::file_test (hangup, Glib::FILE_TEST_EXISTS);
}

void
call_the_mothership (const string& version)
{
        /* check if the user says never to do this
         */

        if (mothership_blocked()) {
                return;
        }

        CURL* c;
        struct utsname utb;
        std::string versionstr;

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
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &versionstr);

        std::cerr << "Callback to ardour.org ...\n";

        char errbuf[CURL_ERROR_SIZE];
        curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);

        if (curl_easy_perform (c) == 0) {
                cerr << "Current release is " << versionstr << endl;

                vector<string> ours;
                vector<string> current;

                split (version, ours, '.');
                split (versionstr, current, '.');

                if (ours.size() == 3 && current.size() == 3) {

                        int ours_n[3];
                        int current_n[3];

                        using namespace PBD;

                        ours_n[0] = atoi (ours[0]);
                        ours_n[1] = atoi (ours[1]);
                        ours_n[2] = atoi (ours[2]);

                        current_n[0] = atoi (current[0]);
                        current_n[1] = atoi (current[1]);
                        current_n[2] = atoi (current[2]);

                        if (ours_n[0] < current_n[0] ||
                            ((ours_n[0] == current_n[0]) && (ours_n[1] < current_n[1])) ||
                            ((ours_n[0] == current_n[0]) && (ours_n[1] == current_n[1]) && (ours_n[2] < current_n[2]))) {
                                cerr << "TOO OLD\n";
                        } else {
                                cerr << "CURRENT\n";
                        }
                } else {
                        cerr << "Unusual local version: " << version << endl;
                }
        }

        curl_easy_cleanup (c);
}
