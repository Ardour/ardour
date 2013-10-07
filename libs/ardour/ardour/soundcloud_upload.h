/* soundcloud_upload.h ******************************************************

	Adapted for Ardour by Ben Loftis, March 2012

*****************************************************************************/

#ifndef __ardour_soundcloud_upload_h__
#define __ardour_soundcloud_upload_h__

#include <string>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>

#include "curl/curl.h"
#include "ardour/session_handle.h"
#include "ardour/export_handler.h"
#include "pbd/signals.h"

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

	std::string	Get_Auth_Token(std::string username, std::string password);
	std::string Upload (std::string file_path, std::string title, std::string token, bool ispublic, ARDOUR::ExportHandler *caller);
	static int progress_callback(void *caller, double dltotal, double dlnow, double ultotal, double ulnow);


private:

	void		setcUrlOptions();

	CURL *curl_handle;
	CURLM *multi_handle;
	char errorBuffer[CURL_ERROR_SIZE];	// storage for cUrl error message

	std::string title;
	ARDOUR::ExportHandler *caller;

};

#endif /* __ardour_soundcloud_upload_h__ */
