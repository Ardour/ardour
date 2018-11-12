/* sfdb_freesound_mootcher.cpp **********************************************************************

	Adapted for Ardour by Ben Loftis, March 2008
	Updated to new Freesound API by Colin Fletcher, November 2011

	Mootcher 23-8-2005

	Mootcher Online Access to thefreesoundproject website
	http://freesound.iua.upf.edu/

	GPL 2005 Jorn Lemon
	mail for questions/remarks: mootcher@twistedlemon.nl
	or go to the freesound website forum

	-----------------------------------------------------------------

	Includes:
		curl.h    (version 7.14.0)
	Librarys:
		libcurl.lib

	-----------------------------------------------------------------
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
#include "sfdb_freesound_mootcher.h"

#include "pbd/xml++.h"
#include "pbd/error.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "pbd/i18n.h"

#include "ardour/audio_library.h"
#include "ardour/rc_configuration.h"
#include "pbd/pthread_utils.h"
#include "gui_thread.h"

using namespace PBD;

static const std::string base_url = "http://www.freesound.org/api";
static const std::string api_key = "9d77cb8d841b4bcfa960e1aae62224eb"; // ardour3

//------------------------------------------------------------------------
Mootcher::Mootcher()
	: curl(curl_easy_init())
{
	cancel_download_btn.set_label (_("Cancel"));
	progress_hbox.pack_start (progress_bar, true, true);
	progress_hbox.pack_end (cancel_download_btn, false, false);
	progress_bar.show();
	cancel_download_btn.show();
	cancel_download_btn.signal_clicked().connect(sigc::mem_fun (*this, &Mootcher::cancelDownload));
};
//------------------------------------------------------------------------
Mootcher:: ~Mootcher()
{
	curl_easy_cleanup(curl);
}

//------------------------------------------------------------------------

void Mootcher::ensureWorkingDir ()
{
	std::string p = ARDOUR::Config->get_freesound_download_dir();

	if (!Glib::file_test (p, Glib::FILE_TEST_IS_DIR)) {
		if (g_mkdir_with_parents (p.c_str(), 0775) != 0) {
			PBD::error << "Unable to create Mootcher working dir" << endmsg;
		}
	}
	basePath = p;
#ifdef PLATFORM_WINDOWS
	std::string replace = "/";
	size_t pos = basePath.find("\\");
	while( pos != std::string::npos ){
		basePath.replace(pos, 1, replace);
		pos = basePath.find("\\");
	}
#endif
}


//------------------------------------------------------------------------
size_t Mootcher::WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	int realsize = (int)(size * nmemb);
	struct SfdbMemoryStruct *mem = (struct SfdbMemoryStruct *)data;

	mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);

	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}


//------------------------------------------------------------------------

std::string Mootcher::sortMethodString(enum sortMethod sort)
{
// given a sort type, returns the string value to be passed to the API to
// sort the results in the requested way.

	switch (sort) {
		case sort_duration_desc:	return "duration_desc";
		case sort_duration_asc: 	return "duration_asc";
		case sort_created_desc:		return "created_desc";
		case sort_created_asc:		return "created_asc";
		case sort_downloads_desc:	return "downloads_desc";
		case sort_downloads_asc:	return "downloads_asc";
		case sort_rating_desc:		return "rating_desc";
		case sort_rating_asc:		return "rating_asc";
		default:			return "";
	}
}

//------------------------------------------------------------------------
void Mootcher::setcUrlOptions()
{
	// some servers don't like requests that are made without a user-agent field, so we provide one
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	// setup curl error buffer
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
	// Allow redirection
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

	// Allow connections to time out (without using signals)
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);


}

std::string Mootcher::doRequest(std::string uri, std::string params)
{
	std::string result;
	struct SfdbMemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = 0;

	setcUrlOptions();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &xml_page);

	// curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
	// curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postMessage.c_str());
	// curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1);

	// the url to get
	std::string url = base_url + uri + "?";
	if (params != "") {
		url += params + "&api_key=" + api_key + "&format=xml";
	} else {
		url += "api_key=" + api_key + "&format=xml";
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str() );

	// perform online request
	CURLcode res = curl_easy_perform(curl);
	if( res != 0 ) {
		error << string_compose (_("curl error %1 (%2)"), res, curl_easy_strerror(res)) << endmsg;
		return "";
	}

	// free the memory
	if (xml_page.memory) {
		result = xml_page.memory;
	}

	free (xml_page.memory);
	xml_page.memory = NULL;
	xml_page.size = 0;

	return result;
}


std::string Mootcher::searchSimilar(std::string id)
{
	std::string params = "";

	params += "&fields=id,original_filename,duration,filesize,samplerate,license,serve";
	params += "&num_results=100";

	return doRequest("/sounds/" + id + "/similar", params);
}

//------------------------------------------------------------------------

std::string Mootcher::searchText(std::string query, int page, std::string filter, enum sortMethod sort)
{
	std::string params = "";
	char buf[24];

	if (page > 1) {
		snprintf(buf, 23, "p=%d&", page);
		params += buf;
	}

	char *eq = curl_easy_escape(curl, query.c_str(), query.length());
	params += "q=\"" + std::string(eq) + "\"";
	free(eq);

	if (filter != "") {
		char *ef = curl_easy_escape(curl, filter.c_str(), filter.length());
		params += "&f=" + std::string(ef);
		free(ef);
	}

	if (sort)
		params += "&s=" + sortMethodString(sort);

	params += "&fields=id,original_filename,duration,filesize,samplerate,license,serve";
	params += "&sounds_per_page=100";

	return doRequest("/sounds/search", params);
}

//------------------------------------------------------------------------

std::string Mootcher::getSoundResourceFile(std::string ID)
{

	std::string originalSoundURI;
	std::string audioFileName;
	std::string xml;


	// download the xmlfile into xml_page
	xml = doRequest("/sounds/" + ID, "");

	XMLTree doc;
	doc.read_buffer( xml.c_str() );
	XMLNode *freesound = doc.root();

	// if the page is not a valid xml document with a 'freesound' root
	if (freesound == NULL) {
		error << _("getSoundResourceFile: There is no valid root in the xml file") << endmsg;
		return "";
	}

	if (strcmp(doc.root()->name().c_str(), "response") != 0) {
		error << string_compose (_("getSoundResourceFile: root = %1, != response"), doc.root()->name()) << endmsg;
		return "";
	}

	XMLNode *name = freesound->child("original_filename");

	// get the file name and size from xml file
	if (name) {

		audioFileName = Glib::build_filename (basePath, ID + "-" + name->child("text")->content());

		//store all the tags in the database
		XMLNode *tags = freesound->child("tags");
		if (tags) {
			XMLNodeList children = tags->children();
			XMLNodeConstIterator niter;
			std::vector<std::string> strings;
			for (niter = children.begin(); niter != children.end(); ++niter) {
				XMLNode *node = *niter;
				if( strcmp( node->name().c_str(), "resource") == 0 ) {
					XMLNode *text = node->child("text");
					if (text) {
						// std::cerr << "tag: " << text->content() << std::endl;
						strings.push_back(text->content());
					}
				}
			}
			ARDOUR::Library->set_tags (std::string("//")+audioFileName, strings);
			ARDOUR::Library->save_changes ();
		}
	}

	return audioFileName;
}

int audioFileWrite(void *buffer, size_t size, size_t nmemb, void *file)
{
	return (int)fwrite(buffer, size, nmemb, (FILE*) file);
};

//------------------------------------------------------------------------

void *
Mootcher::threadFunc() {
CURLcode res;

	res = curl_easy_perform (curl);
	fclose (theFile);
	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 1); // turn off the progress bar

	if (res != CURLE_OK) {
		/* it's not an error if the user pressed the stop button */
		if (res != CURLE_ABORTED_BY_CALLBACK) {
			error <<  string_compose (_("curl error %1 (%2)"), res, curl_easy_strerror(res)) << endmsg;
		}
		remove ( (audioFileName+".part").c_str() );
	} else {
		rename ( (audioFileName+".part").c_str(), audioFileName.c_str() );
		// now download the tags &c.
		getSoundResourceFile(ID);
	}

	return (void *) res;
}

void
Mootcher::doneWithMootcher()
{

	// update the sound info pane if the selection in the list box is still us
	sfb->refresh_display(ID, audioFileName);

	delete this; // this should be OK to do as long as Progress and Finished signals are always received in the order in which they are emitted
}

static void *
freesound_download_thread_func(void *arg)
{
	Mootcher *thisMootcher = (Mootcher *) arg;
	void *res;

	// std::cerr << "freesound_download_thread_func(" << arg << ")" << std::endl;
	res = thisMootcher->threadFunc();

	thisMootcher->Finished(); /* EMIT SIGNAL */
	return res;
}


//------------------------------------------------------------------------

bool Mootcher::checkAudioFile(std::string originalFileName, std::string theID)
{
	ensureWorkingDir();
	ID = theID;
	audioFileName = Glib::build_filename (basePath, ID + "-" + originalFileName);

	// check to see if audio file already exists
	FILE *testFile = g_fopen(audioFileName.c_str(), "r");
	if (testFile) {
		fseek (testFile , 0 , SEEK_END);
		if (ftell (testFile) > 256) {
			fclose (testFile);
			return true;
		}

		// else file was small, probably an error, delete it
		fclose(testFile);
		remove( audioFileName.c_str() );
	}
	return false;
}


bool Mootcher::fetchAudioFile(std::string originalFileName, std::string theID, std::string audioURL, SoundFileBrowser *caller)
{
	ensureWorkingDir();
	ID = theID;
	audioFileName = Glib::build_filename (basePath, ID + "-" + originalFileName);

	if (!curl) {
		return false;
	}
	// now download the actual file
	theFile = g_fopen( (audioFileName + ".part").c_str(), "wb" );

	if (!theFile) {
		return false;
	}

	// create the download url
	audioURL += "?api_key=" + api_key;

	setcUrlOptions();
	curl_easy_setopt(curl, CURLOPT_URL, audioURL.c_str() );
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, audioFileWrite);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, theFile);

	std::string prog;
	prog = string_compose (_("%1"), originalFileName);
	progress_bar.set_text(prog);

	Gtk::VBox *freesound_vbox = dynamic_cast<Gtk::VBox *> (caller->notebook.get_nth_page(2));
	freesound_vbox->pack_start(progress_hbox, Gtk::PACK_SHRINK);
	progress_hbox.show();
	cancel_download = false;
	sfb = caller;

	curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0); // turn on the progress bar
	curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
	curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, this);

	Progress.connect(*this, invalidator (*this), boost::bind(&Mootcher::updateProgress, this, _1, _2), gui_context());
	Finished.connect(*this, invalidator (*this), boost::bind(&Mootcher::doneWithMootcher, this), gui_context());
	pthread_t freesound_download_thread;
	pthread_create_and_store("freesound_import", &freesound_download_thread, freesound_download_thread_func, this);

	return true;
}

//---------

void
Mootcher::updateProgress(double dlnow, double dltotal)
{
	if (dltotal > 0) {
		double fraction = dlnow / dltotal;
		// std::cerr << "progress idle: " << progress->bar->get_text() << ". " << progress->dlnow << " / " << progress->dltotal << " = " << fraction << std::endl;
		if (fraction > 1.0) {
			fraction = 1.0;
		} else if (fraction < 0.0) {
			fraction = 0.0;
		}
		progress_bar.set_fraction(fraction);
	}
}

int
Mootcher::progress_callback(void *caller, double dltotal, double dlnow, double /*ultotal*/, double /*ulnow*/)
{
	// It may seem curious to pass a pointer to an instance of an object to a static
	// member function, but we can't use a normal member function as a curl progress callback,
	// and we want access to some private members of Mootcher.

	Mootcher *thisMootcher = (Mootcher *) caller;

	if (thisMootcher->cancel_download) {
		return -1;
	}

	thisMootcher->Progress(dlnow, dltotal); /* EMIT SIGNAL */
	return 0;
}

