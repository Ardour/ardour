#include "open_file_dialog_proxy.h"

#include <Windows.h>
#include <commdlg.h>
#include <ShlObj.h>

#include <iostream>

#include <string>

#include "glibmm/miscutils.h"

using namespace std;
namespace ARDOUR
{
bool save_file_dialog(std::string& file_name, std::string initial_path, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_OVERWRITEPROMPT;

	// Check on valid path
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(initial_path.c_str(), &FindFileData) ;
    int found = (handle != INVALID_HANDLE_VALUE);

	// if path is valid
	if( found )
		ofn.lpstrInitialDir = initial_path.c_str();
	else
	{
		initial_path = Glib::get_home_dir();
		ofn.lpstrInitialDir = initial_path.c_str();
	}

	// Run dialog
	if(GetSaveFileName(&ofn))
	{
		file_name = ofn.lpstrFile;
		return true;
	}

	return false;
}

bool open_file_dialog(std::string& file_name, std::string initial_path, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_PATHMUSTEXIST;

	// Check on valid path
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(initial_path.c_str(), &FindFileData) ;
    int found = (handle != INVALID_HANDLE_VALUE);

	// if path is valid
	if( found )
		ofn.lpstrInitialDir = initial_path.c_str();
	else
	{
		initial_path = Glib::get_home_dir();
		ofn.lpstrInitialDir = initial_path.c_str();
	}

	if( GetOpenFileName(&ofn) )
	{
		file_name = ofn.lpstrFile;
	    return true;
	}

	return false;
}

bool choose_folder_dialog(std::string& selected_path, std::string title)
{
	BROWSEINFO bi;
    memset(&bi, 0, sizeof(bi));

	bi.lpszTitle = title.c_str();
	bi.ulFlags = BIF_NEWDIALOGSTYLE;

    OleInitialize(NULL);

    LPITEMIDLIST pIDL = SHBrowseForFolder(&bi);
	 
    if (pIDL == NULL)
    {
        return false;
    }
    
    TCHAR *buffer = new TCHAR[MAX_PATH];
    if(!SHGetPathFromIDList(pIDL, buffer) != 0)
    {
        CoTaskMemFree(pIDL);
        return false;
    }
    selected_path = buffer;

    CoTaskMemFree(pIDL);
    OleUninitialize();
    return true;
}

} // namespace ARDOUR