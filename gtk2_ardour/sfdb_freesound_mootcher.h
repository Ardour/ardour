/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*sfdb_freesound_mootcher.h****************************************************************************

	Adapted for Ardour by Ben Loftis, March 2008
	Updated to new Freesound API by Colin Fletcher, November 2011

	Mootcher Online Access to thefreesoundproject website
	http://freesound.iua.upf.edu/

	GPL 2005 Jorn Lemon
	mail for questions/remarks: mootcher@twistedlemon.nl
	or go to the freesound website forum

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

#include "sfdb_ui.h"

#include "curl/curl.h"

//--- struct to store XML file
struct MemoryStruct {
	char *memory;
	size_t size;
};

enum sortMethod {
	sort_none,		// no sort
	sort_duration_desc,	// Sort by the duration of the sounds, longest sounds first.
	sort_duration_asc, 	// Same as above, but shortest sounds first.
	sort_created_desc, 	// Sort by the date of when the sound was added. newest sounds first.
	sort_created_asc, 	// Same as above, but oldest sounds first.
	sort_downloads_desc, 	// Sort by the number of downloads, most downloaded sounds first.
	sort_downloads_asc, 	// Same as above, but least downloaded sounds first.
	sort_rating_desc, 	// Sort by the average rating given to the sounds, highest rated first.
	sort_rating_asc 	// Same as above, but lowest rated sounds first.
};


class Mootcher
{
public:
	Mootcher();
	~Mootcher();

	bool		checkAudioFile(std::string originalFileName, std::string ID);
	bool		fetchAudioFile(std::string originalFileName, std::string ID, std::string audioURL, SoundFileBrowser *caller);
	std::string	searchText(std::string query, int page, std::string filter, enum sortMethod sort);
	std::string	searchSimilar(std::string id);
	void *		threadFunc();
	SoundFileBrowser *sfb; 
	std::string	audioFileName;
	std::string	ID;

private:

	void		ensureWorkingDir();

	std::string	doRequest(std::string uri, std::string params);
	void		setcUrlOptions();

	static size_t	WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);
	static int	progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
	std::string	sortMethodString(enum sortMethod sort);
	std::string	getSoundResourceFile(std::string ID);

	CURL *curl;
	char errorBuffer[CURL_ERROR_SIZE];	// storage for cUrl error message

	FILE* theFile;

	Gtk::HBox progress_hbox;
	Gtk::ProgressBar progress_bar;
	Gtk::Button cancel_download_btn;

	bool cancel_download;
	void cancelDownload() { 
		cancel_download = true;
		progress_hbox.hide();
	}

	std::string basePath;
	std::string xmlLocation;
};

