/* soundcloud_export.cpp **********************************************************************

	Adapted for Ardour by Ben Loftis, March 2012

	Licence GPL:

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


*************************************************************************************/
#include "soundcloud_export.h"

#include "pbd/xml++.h"
#include <pbd/error.h>
//#include "pbd/filesystem.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <glib/gstdio.h>

#include "i18n.h"

using namespace PBD;

static const std::string base_url = "http://api.soundcloud.com/tracks/13158665?client_id=";

size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register int realsize = (int)(size * nmemb);
	struct MemoryStruct *mem = (struct MemoryStruct *)data;

	mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);

	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}

SoundcloudUploader::SoundcloudUploader()
{
	curl_handle = curl_easy_init();
	multi_handle = curl_multi_init();
}

std::string
SoundcloudUploader::Get_Auth_Token( std::string username, std::string password )
{
	struct MemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = 0;

	setcUrlOptions();

	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &xml_page);

	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;

	/* Fill in the filename field */ 
	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "client_id",
			CURLFORM_COPYCONTENTS, "e7ac891eef866f139773cf8102b7a719",
			CURLFORM_END);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "client_secret",
			CURLFORM_COPYCONTENTS, "d78f34d19f09d26731801a0cb0f382c4",
			CURLFORM_END);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "grant_type",
			CURLFORM_COPYCONTENTS, "password",
			CURLFORM_END);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "username",
			CURLFORM_COPYCONTENTS, username.c_str(),
			CURLFORM_END);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "password",
			CURLFORM_COPYCONTENTS, password.c_str(),
			CURLFORM_END);

	struct curl_slist *headerlist=NULL;
	headerlist = curl_slist_append(headerlist, "Expect:");
	headerlist = curl_slist_append(headerlist, "Accept: application/xml");
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headerlist);

	/* what URL that receives this POST */ 
	std::string url = "https://api.soundcloud.com/oauth2/token";
	curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl_handle, CURLOPT_HTTPPOST, formpost);

	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

	// perform online request
	CURLcode res = curl_easy_perform(curl_handle);
	if( res != 0 ) {
		std::cerr << "curl error " << res << " (" << curl_easy_strerror(res) << ")" << std::endl;
		return "";
	}

	if(xml_page.memory){
		//cheesy way to parse the json return value.  find access_token, then advance 3 quotes

		if ( strstr ( xml_page.memory , "access_token" ) == NULL) {
			error << _("Upload to Soundcloud failed.  Perhaps your email or password are incorrect?\n") << endmsg;
			return "";
		}

		std::string token = strtok( xml_page.memory, "access_token" );
		token = strtok( NULL, "\"" );
		token = strtok( NULL, "\"" );
		token = strtok( NULL, "\"" );

		free( xml_page.memory );
		return token;
	}

	return "";
}

std::string
SoundcloudUploader::Upload(std::string file_path, std::string title, std::string auth_token, bool ispublic, curl_progress_callback progress_callback, void *caller )
{
	int still_running;

	struct MemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = 0;

	setcUrlOptions();

	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) &xml_page);

	struct curl_httppost *formpost=NULL;
	struct curl_httppost *lastptr=NULL;

	/* Fill in the file upload field. This makes libcurl load data from
	   the given file name when curl_easy_perform() is called. */ 
	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "track[asset_data]",
			CURLFORM_FILE, file_path.c_str(),
			CURLFORM_END);

	/* Fill in the filename field */ 
	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "oauth_token",
			CURLFORM_COPYCONTENTS, auth_token.c_str(),
			CURLFORM_END);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "track[title]",
			CURLFORM_COPYCONTENTS, title.c_str(),
			CURLFORM_END);

	curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "track[sharing]",
			CURLFORM_COPYCONTENTS, ispublic ? "public" : "private",
			CURLFORM_END);

	/* initalize custom header list (stating that Expect: 100-continue is not
	   wanted */ 
	struct curl_slist *headerlist=NULL;
	static const char buf[] = "Expect:";
	headerlist = curl_slist_append(headerlist, buf);


	if(curl_handle && multi_handle) {

		/* what URL that receives this POST */ 
		std::string url = "https://api.soundcloud.com/tracks";
		curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);

		curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headerlist);
		curl_easy_setopt(curl_handle, CURLOPT_HTTPPOST, formpost);

		curl_easy_setopt (curl_handle, CURLOPT_NOPROGRESS, 0); // turn on the progress bar
		curl_easy_setopt (curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
		curl_easy_setopt (curl_handle, CURLOPT_PROGRESSDATA, caller);

		curl_multi_add_handle(multi_handle, curl_handle);

		curl_multi_perform(multi_handle, &still_running);


		while(still_running) {
			struct timeval timeout;
			int rc; /* select() return code */ 

			fd_set fdread;
			fd_set fdwrite;
			fd_set fdexcep;
			int maxfd = -1;

			long curl_timeo = -1;

			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexcep);

			/* set a suitable timeout to play around with */ 
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			curl_multi_timeout(multi_handle, &curl_timeo);
			if(curl_timeo >= 0) {
				timeout.tv_sec = curl_timeo / 1000;
				if(timeout.tv_sec > 1)
					timeout.tv_sec = 1;
				else
					timeout.tv_usec = (curl_timeo % 1000) * 1000;
			}

			/* get file descriptors from the transfers */ 
			curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

			/* In a real-world program you OF COURSE check the return code of the
			   function calls.  On success, the value of maxfd is guaranteed to be
			   greater or equal than -1.  We call select(maxfd + 1, ...), specially in
			   case of (maxfd == -1), we call select(0, ...), which is basically equal
			   to sleep. */ 

			rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

			switch(rc) {
				case -1:
					/* select error */ 
					break;
				case 0:
				default:
					/* timeout or readable/writable sockets */ 
					curl_multi_perform(multi_handle, &still_running);
					break;
			}
		} 

		/* then cleanup the formpost chain */ 
		curl_formfree(formpost);

		/* free slist */ 
		curl_slist_free_all (headerlist);
	}

	curl_easy_setopt (curl_handle, CURLOPT_NOPROGRESS, 1); // turn off the progress bar

	if(xml_page.memory){

		std::cout << xml_page.memory << std::endl;

		XMLTree doc;
		doc.read_buffer( xml_page.memory );
		XMLNode *root = doc.root();

		if (!root) {
			std::cout << "no root XML node!" << std::endl;
			return "";
		}

		XMLNode *url_node = root->child("permalink-url");
		if (!url_node) {
			std::cout << "no child node \"permalink-url\" found!" << std::endl;
			return "";
		}

		XMLNode *text_node = url_node->child("text");
		if (!text_node) {
			std::cout << "no text node found!" << std::endl;
			return "";
		}

		free( xml_page.memory );
		return text_node->content();
	}

	return "";
};


SoundcloudUploader:: ~SoundcloudUploader()
{
	curl_easy_cleanup(curl_handle);
	curl_multi_cleanup(multi_handle);
}


void
SoundcloudUploader::setcUrlOptions()
{
	// basic init for curl
	curl_global_init(CURL_GLOBAL_ALL);
	// some servers don't like requests that are made without a user-agent field, so we provide one
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	// setup curl error buffer
	curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errorBuffer);
	// Allow redirection
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);
	
	// Allow connections to time out (without using signals)
	curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 30);

	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
}
