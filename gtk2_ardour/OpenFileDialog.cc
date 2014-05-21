#include "OpenFileDialogProxy.h"

#include <Windows.h>
#include <commdlg.h>
#include <ShlObj.h>

#include <iostream>

#include <string>

using namespace std;
namespace ARDOUR
{
bool SaveFileDialog(std::string& fileName, std::string path, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_OVERWRITEPROMPT;

	if( !path.empty() )
		ofn.lpstrInitialDir = path.c_str();

	// Run dialog
	if(GetSaveFileName(&ofn))
	{
		fileName = ofn.lpstrFile;
		return true;
	}

	return false;
}

bool OpenFileDialog(std::string& fileName, std::string path, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_PATHMUSTEXIST;

	if( !path.empty() )
		ofn.lpstrInitialDir = path.c_str();

	if( GetOpenFileName(&ofn) )
	{
		fileName = ofn.lpstrFile;
	    return true;
	}

	return false;
}

bool ChooseFolderDialog(std::string& selectedPath, std::string title)
{
	BROWSEINFO bi;
    memset(&bi, 0, sizeof(bi));

	bi.lpszTitle = title.c_str();

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
    selectedPath = buffer;

    CoTaskMemFree(pIDL);
    OleUninitialize();
    return true;
}

} // namespace ARDOUR