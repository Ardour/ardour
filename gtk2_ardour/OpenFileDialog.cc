#include "OpenFileDialogProxy.h"

#include <string>

#ifdef _WIN32 
#include <Windows.h>
#include <commdlg.h>
#endif

using namespace std;
namespace ARDOUR
{
bool SaveFileDialog(std::string& fileName, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if(GetSaveFileName(&ofn))
	{
		fileName = ofn.lpstrFile;
		return true;
	}

	return false;
}

bool OpenFileDialog(std::string& fileName, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_PATHMUSTEXIST;

	if( GetOpenFileName(&ofn) )
	{
		fileName = ofn.lpstrFile;
	    return true;
	}

	return false;
}

}