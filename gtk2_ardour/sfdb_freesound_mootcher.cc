/* sfdb_freesound_mootcher.cpp **********************************************************************
	
	Adapted for Ardour by Ben Loftis, March 2008
		
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

#include <sys/stat.h>
#include <sys/types.h>

#include "ardour/audio_library.h"

#define TRUE 1

//------------------------------------------------------------------------
Mootcher::	Mootcher(const char *saveLocation)
	: curl( NULL )
	, connection( 0 )
{
	changeWorkingDir(saveLocation);
};
//------------------------------------------------------------------------
Mootcher::	~Mootcher()
{
	remove( "cookiejar.txt" );
}
//------------------------------------------------------------------------
const char* Mootcher::changeWorkingDir(const char *saveLocation)
{
	basePath = saveLocation;
#ifdef __WIN32__
	std::string replace = "/";
	int pos = (int)basePath.find("\\");
	while( pos != std::string::npos ){
		basePath.replace(pos, 1, replace);
		pos = (int)basePath.find("\\");
	}
#endif
	// 
	int pos2 = basePath.find_last_of("/");
	if(basePath.length() != (pos2+1)) basePath += "/";
	
	// create Freesound directory and sound dir
	std::string sndLocation = basePath;
	mkdir(sndLocation.c_str(), 0777);        
	sndLocation += "snd";
	mkdir(sndLocation.c_str(), 0777);        

	return basePath.c_str();
}

//------------------------------------------------------------------------
size_t		Mootcher::WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	register int realsize = (int)(size * nmemb);
	struct MemoryStruct *mem = (struct MemoryStruct *)data;

	// There might be a realloc() out there that doesn't like 
	// reallocing NULL pointers, so we take care of it here
	if(mem->memory)	mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
	else			mem->memory = (char *)malloc(mem->size + realsize + 1);

	if (mem->memory) {
		memcpy(&(mem->memory[mem->size]), ptr, realsize);
		mem->size += realsize;
		mem->memory[mem->size] = 0;
	}
	return realsize;
}


//------------------------------------------------------------------------
void		Mootcher::toLog(std::string input)
{
printf("%s\n", input.c_str());// for debugging
}


//------------------------------------------------------------------------
void		Mootcher::setcUrlOptions()
{
	// basic init for curl
	curl_global_init(CURL_GLOBAL_ALL);
	// some servers don't like requests that are made without a user-agent field, so we provide one
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	// setup curl error buffer
	CURLcode res = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
	// always use the cookie with session id which is received at the login
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "cookiejar.txt");
	// Allow redirection
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
}

//------------------------------------------------------------------------
int			Mootcher::doLogin(std::string login, std::string password)
{
	if(connection==1)
		return 1;
	
	struct MemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = 0;

	// create the post message from the login and password
	std::string postMessage;
	postMessage += "username=";
	postMessage += curl_escape(login.c_str(), 0);
	postMessage += "&password=";
	postMessage += curl_escape(password.c_str(), 0);
	postMessage += "&login=";
	postMessage += curl_escape("1", 0);
	postMessage += "&redirect=";
	postMessage += curl_escape("../tests/login.php", 0);

	// Do the setup for libcurl
	curl = curl_easy_init();

	if(curl)
	{
		setcUrlOptions();
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&xml_page);
		// save the sessoin id that is given back by the server in a cookie
		curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "cookiejar.txt");
		// use POST for login variables
		curl_easy_setopt(curl, CURLOPT_POST, TRUE);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postMessage.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1);

		// the url to get
		std::string login_url = "http://www.freesound.org/forum/login.php";
		curl_easy_setopt(curl, CURLOPT_URL, login_url.c_str() );

		// perform online request
		connection = 1;
		CURLcode res = curl_easy_perform(curl);
		if( res != 0 ) {
			toLog("curl login error\n");
			toLog(curl_easy_strerror(res));
			connection = 0;
		}
		
		if (connection == 1){
			std::string check_page = xml_page.memory;
			int test = (int)check_page.find("login");   //logged
			if(	strcmp(xml_page.memory, "login") == 0 )
				toLog("Logged in.\n");
			else {
				toLog("Login failed: Check username and password.\n");
				connection = 0;
			}
		}

		// free the memory
		if(xml_page.memory){		
			free( xml_page.memory );
			xml_page.memory = NULL;
			xml_page.size = 0;
		}

		std::cerr << "Login was cool, connection = "  << connection << std::endl;
		return connection;
	}
	else return 3; // will be returned if a curl related problem ocurrs
}
//------------------------------------------------------------------------
std::string	Mootcher::searchText(std::string word)
{
	struct MemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = 0;
	
	std::string result;

	if(connection != 0)
	{
		// create a url encoded post message
		std::string postMessage;
		char tempString[ 128 ];
		char *tempPointer = &tempString[0];

		postMessage = "search=";
		postMessage += curl_escape(word.c_str(), 0);
		sprintf( tempPointer, "&searchDescriptions=1");
		postMessage += tempPointer;
		sprintf( tempPointer, "&searchtags=1");
		postMessage += tempPointer;

		// Ref: http://www.freesound.org/forum/viewtopic.php?p=19081
		// const ORDER_DEFAULT = 0;
		// const ORDER_DOWNLOADS_DESC = 1;
		// const ORDER_DOWNLOADS_ASC = 2;
		// const ORDER_USERNAME_DESC = 3;
		// const ORDER_USERNAME_ASC = 4;
		// const ORDER_DATE_DESC = 5;
		// const ORDER_DATE_ASC = 6;
		// const ORDER_DURATION_DESC = 7;
		// const ORDER_DURATION_ASC = 8;
		// const ORDER_FILEFORMAT_DESC = 9;
		// const ORDER_FILEFORMAT_ASC = 10;
		sprintf( tempPointer, "&order=1");
		postMessage += tempPointer;
		sprintf( tempPointer, "&start=0");
		postMessage += tempPointer;
		sprintf( tempPointer, "&limit=10");
		postMessage += tempPointer;
		// The limit of 10 samples is arbitrary, but seems
		// reasonable in light of the fact that all of the returned
		// samples get downloaded, and downloads are s-l-o-w.
		
		if(curl)
		{
			// basic init for curl 
			setcUrlOptions();	
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); 
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&xml_page);
			// setup the post message
			curl_easy_setopt(curl, CURLOPT_POST, TRUE);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postMessage.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1);
			
			// the url to get
			std::string search_url = "http://www.freesound.org/searchTextXML.php";
			curl_easy_setopt(curl, CURLOPT_URL, search_url.c_str());

			// perform the online search 
			connection = 1;
			CURLcode res = curl_easy_perform(curl);
			if( res != 0 ) {
				toLog("curl login error\n");
				toLog(curl_easy_strerror(res));
				connection = 0;
			}
			
			result = xml_page.memory;
			toLog( result.c_str() );

			// free the memory
			if(xml_page.memory){
				free( xml_page.memory );
				xml_page.memory = NULL;
				xml_page.size = 0;
			}

		}
	}

	return result;
}

//------------------------------------------------------------------------
std::string Mootcher::changeExtension(std::string filename)
{
	std::string aiff = ".aiff";
	std::string aif = ".aif";
	std::string wav = ".wav";
	std::string mp3 = ".mp3";
	std::string ogg = ".ogg";
	std::string flac = ".flac";

	std::string replace = ".xml";
	int pos = 0;

 	pos = (int)filename.find(aiff);
	if(pos != std::string::npos) filename.replace(pos, aiff.size(), replace); 
 	pos = (int)filename.find(aif);
	if(pos != std::string::npos) filename.replace(pos, aif.size(), replace); 
 	pos = (int)filename.find(wav);
	if(pos != std::string::npos) filename.replace(pos, wav.size(), replace); 
 	pos = (int)filename.find(mp3);
	if(pos != std::string::npos) filename.replace(pos, mp3.size(), replace); 
 	pos = (int)filename.find(ogg);
	if(pos != std::string::npos) filename.replace(pos, ogg.size(), replace); 
 	pos = (int)filename.find(flac);
	if(pos != std::string::npos) filename.replace(pos, flac.size(), replace); 

	return filename;
}
//------------------------------------------------------------------------
void		Mootcher::GetXml(std::string ID, struct MemoryStruct &xml_page)
{

	if(curl) {
		setcUrlOptions();
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&xml_page);

		// URL to get
		std::string getxml_url = "http://www.freesound.org/samplesViewSingleXML.php?id=";
		getxml_url += ID;

		curl_easy_setopt(curl, CURLOPT_URL, getxml_url.c_str() );
 		
		// get it!
		connection = 1;
		CURLcode res = curl_easy_perform(curl);
		if( res != 0 ) {
			toLog("curl login error\n");
			toLog(curl_easy_strerror(res));
			connection = 0;
		}
	}
}
//------------------------------------------------------------------------
std::string	Mootcher::getXmlFile(std::string ID, int &length)
{
	struct MemoryStruct xml_page;
	xml_page.memory = NULL;
	xml_page.size = NULL;

	std::string xmlFileName;
	std::string audioFileName;
	std::string filename;
	
	if(connection != 0) {
		// download the xmlfile into xml_page
		GetXml(ID, xml_page);

		// if sample ID does not exist on the freesound website
		if(strcmp(xml_page.memory, "sample non existant") == 0){
			free( xml_page.memory );
			sprintf(message, "getXmlFile: sample with ID:%s does not exist!\n", ID.c_str() );
			toLog(message);
			return filename;
		} else {
			XMLTree doc;
			doc.read_buffer( xml_page.memory );
			XMLNode *freesound = doc.root();
			
			// if the page is not a valid xml document with a 'freesound' root
			if( freesound == NULL){
				sprintf(message, "getXmlFile: There is no valid root in the xml file");
				toLog(message);
			} else {
				XMLNode *sample = freesound->child("sample");
				XMLNode *name = NULL;
				XMLNode *filesize = NULL;
				if (sample) {
					name = sample->child("originalFilename");
					filesize = sample->child("filesize");
				}
				
				// get the file name and size from xml file
				if (sample && name && filesize) {
					
					audioFileName = name->child("text")->content();
					sprintf( message, "getXmlFile: %s needs to be downloaded\n", audioFileName.c_str() );
					toLog(message);

					length = atoi(filesize->child("text")->content().c_str());

					// create new filename with the ID number
					filename = basePath;
					filename += "snd/";
					filename += sample->property("id")->value();
					filename += "-";
					filename += audioFileName;
					// change the extention into .xml
					xmlFileName = changeExtension( filename );

					sprintf(message, "getXmlFile: saving XML: %s\n", xmlFileName.c_str() );
					toLog(message);
					
					// save the xml file to disk
					doc.write(xmlFileName.c_str());

					//store all the tags in the database
					XMLNode *tags = sample->child("tags");
					if (tags) {
						XMLNodeList children = tags->children();
						XMLNodeConstIterator niter;
						std::vector<std::string> strings;
						for (niter = children.begin(); niter != children.end(); ++niter) {
							XMLNode *node = *niter;
							if( strcmp( node->name().c_str(), "tag") == 0 ) {
								XMLNode *text = node->child("text");
								if (text) strings.push_back(text->content());
							}
						}
						ARDOUR::Library->set_tags (std::string("//")+filename, strings);
						ARDOUR::Library->save_changes ();
					}
				}
				
				// clear the memory
				if(xml_page.memory){
					free( xml_page.memory );
					xml_page.memory = NULL;
					xml_page.size = 0;
				}
				return audioFileName;
			}
		}
	}
	else {
		return audioFileName;
	}

}

int audioFileWrite(void *buffer, size_t size, size_t nmemb, void *file)
{
	return (int)fwrite(buffer, size, nmemb, (FILE*) file);
};

//------------------------------------------------------------------------
std::string	Mootcher::getFile(std::string ID)
{
	CURLcode result_curl;

	std::string audioFileName;

	if(connection != 0)
	{
		int length;
		std::string name = getXmlFile(ID, length);
		if( name != "" ){

			// create new filename with the ID number
			audioFileName += basePath;
			audioFileName += "snd/";
			audioFileName += ID;
			audioFileName += "-";			
			audioFileName += name;
			
			//check to see if audio file already exists
			FILE *testFile = fopen(audioFileName.c_str(), "r");
			if (testFile) {  //TODO:  should also check length to see if file is complete
				fseek (testFile , 0 , SEEK_END);
				if (ftell (testFile) == length) {
					sprintf(message, "%s already exists\n", audioFileName.c_str() );
					toLog(message);
					fclose (testFile);
					return audioFileName;
				} else {
					remove( audioFileName.c_str() );  //file was not correct length, delete it and try again
				}					
			}
			

			//now download the actual file
			if (curl) {

				FILE* theFile;
				theFile = fopen( audioFileName.c_str(), "wb" );

				// create the download url, this url will also update the download statics on the site
				std::string audioURL;
				audioURL += "http://www.freesound.org/samplesDownload.php?id=";
				audioURL += ID;

				setcUrlOptions();
				curl_easy_setopt(curl, CURLOPT_URL, audioURL.c_str() );
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, audioFileWrite);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, theFile);

				connection = 1;
				CURLcode res = curl_easy_perform(curl);
				if( res != 0 ) {
					toLog("curl login error\n");
					toLog(curl_easy_strerror(res));
					connection = 0;
				}

				fclose(theFile);
			}
	
/*
			bar.dlnowMoo = 0;
			bar.dltotalMoo = 0;
			curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0); // turn on the process bar thingy
			curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
			curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, &bar);
*/				
		}
	}

	return audioFileName;
}

//---------
int Mootcher::progress_callback(void *bar, double dltotal, double dlnow, double ultotal, double ulnow)
{
	struct dlprocess *lbar = (struct dlprocess *) bar;
	lbar->dltotalMoo = dltotal;
	lbar->dlnowMoo = dlnow;
	return 0;
}
