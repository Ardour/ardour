/*soundcloud_export.h****************************************************************************

	Adapted for Ardour by Ben Loftis, March 2012

*****************************************************************************/

#include <string>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <gtkmm/progressbar.h>
//#include <ctime>

#include "curl/curl.h"

//--- struct to store XML file
struct MemoryStruct {
	char *memory;
	size_t size;
};


class SoundcloudUploader
{
public:
	SoundcloudUploader();
	~SoundcloudUploader();

	std::string Get_Auth_Token( std::string username, std::string password );
	std::string Upload( std::string file_path, std::string title, std::string auth_token, bool ispublic, curl_progress_callback, void* caller );

private:

	void		setcUrlOptions();

	CURL *curl_handle;
	CURLM *multi_handle;
	char errorBuffer[CURL_ERROR_SIZE];	// storage for cUrl error message

	std::string basePath;
	std::string xmlLocation;
};

