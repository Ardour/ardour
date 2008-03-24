/*sfdb_freesound_mootcher.h****************************************************************************
	
	Adapted for Ardour by Ben Loftis, March 2008

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
//#include <ctime>

#include "curl/curl.h"


// mootcher version
#define ___VERSION___ 1.3

//--- struct to store XML file 
struct MemoryStruct {
	char *memory;
	size_t size;
};

//--- for download process viewing
struct dlprocess {
	double dltotalMoo;
	double dlnowMoo;
};

class Mootcher
{
public:
	Mootcher(const char *saveLocation);
	~Mootcher();

	int			doLogin(std::string login, std::string password);
	std::string	getFile(std::string ID);
	std::string	searchText(std::string word);


	struct dlprocess bar;
	
private:

	const char*	changeWorkingDir(const char *saveLocation);
	void		createResourceLocation();

	std::string	getXmlFile(std::string ID, int &length);
	void		GetXml(std::string ID, struct MemoryStruct &xml_page);
	std::string	changeExtension(std::string filename);

	void		toLog(std::string input);

	void		setcUrlOptions();

    static size_t		WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);
	static int			progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);

	CURL *curl;
	char errorBuffer[CURL_ERROR_SIZE];	// storage for cUrl error message

	int connection;		// is 0 if no connection
	char message[128];	// storage for messages that are send to the logfile

	std::string basePath;
	std::string xmlLocation;
};
