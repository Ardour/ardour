/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#include <Windows.h>
#include <commdlg.h>
#include <ShlObj.h>

#include <iostream>
#include <string>

#include <glibmm/miscutils.h>

#include <boost/algorithm/string.hpp>

#include "open_file_dialog_proxy.h"

using namespace std;

std::string
ARDOUR::save_file_dialog (std::string initial_path, std::string title)
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
	if (found) {
		ofn.lpstrInitialDir = initial_path.c_str();
        } else {
		initial_path = Glib::get_home_dir();
		ofn.lpstrInitialDir = initial_path.c_str();
	}
        
	// Run dialog
        if (GetSaveFileName(&ofn)) {
		return ofn.lpstrFile;
	}
        
	return string();
}

std::string
ARDOUR::open_file_dialog (std::string initial_path, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_PATHMUSTEXIST;
	ofn.lpstrFilter = " \0*.ardour\0";

	// Check on valid path
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(initial_path.c_str(), &FindFileData) ;
        int found = (handle != INVALID_HANDLE_VALUE);
        
	// if path is valid
	if (found) {
		ofn.lpstrInitialDir = initial_path.c_str();
        } else {
		initial_path = Glib::get_home_dir();
		ofn.lpstrInitialDir = initial_path.c_str();
	}
        
	if (GetOpenFileName(&ofn)) {
		return ofn.lpstrFile;
	}

	return string ();
}

std::vector<std::string>
ARDOUR::open_file_dialog (std::vector<std::string> extensions, std::string initial_path, std::string title)
{
	TCHAR szFilePathName[_MAX_PATH] = "";
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = szFilePathName;  // This will hold the file name
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrTitle = title.c_str();
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

	// Create filter for required file types
	std::string filter;
	for (int i = 0; i < extensions.size(); ++i) {
		filter += "*."+extensions[i]+";";
	}

	char c_filter[2+filter.size()+2];
	c_filter[0] = ' ';
	c_filter[1] = '\0';
	strcpy (c_filter+2, filter.c_str ());
	c_filter[filter.size()+3] = '\0';
	
	ofn.lpstrFilter = c_filter;
		
	// Check on valid path
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(initial_path.c_str(), &FindFileData) ;
        int found = (handle != INVALID_HANDLE_VALUE);
        
	// if path is valid
	if (found) {
		ofn.lpstrInitialDir = initial_path.c_str();
        } else {
		initial_path = Glib::get_home_dir();
		ofn.lpstrInitialDir = initial_path.c_str();
	}

	std::vector<std::string> file_pathes;
        
	if (GetOpenFileName(&ofn)) {

		std::string directory_path = ofn.lpstrFile;
		std::string path;
		char* ptr = ofn.lpstrFile;

		bool many_files = (ofn.lpstrFile [strlen (ofn.lpstrFile) + 1] != 0);

		if (ofn.lpstrFile [strlen (ofn.lpstrFile) + 1] != 0) { // Many files
			for (char *current_name = ofn.lpstrFile + strlen (ofn.lpstrFile) + 1;
				 *current_name;
				 current_name += strlen (current_name) + 1) {
					 file_pathes.push_back (ofn.lpstrFile);
					 std::string& current_file_path = file_pathes.back (); 
					 current_file_path += "\\";
					 current_file_path += current_name;
			}
		} else {
			file_pathes.push_back (ofn.lpstrFile); // single file selected
		}

		return file_pathes;
	}

	return file_pathes;
}

std::string 
ARDOUR::choose_folder_dialog (std::string /* initial_path */, std::string title)
{
	BROWSEINFO bi;
        memset(&bi, 0, sizeof(bi));
        
	bi.lpszTitle = title.c_str();
	bi.ulFlags = BIF_NEWDIALOGSTYLE;
        
        OleInitialize(NULL);
        
        LPITEMIDLIST pIDL = SHBrowseForFolder(&bi);
        
        if (pIDL == NULL) {
                return string ();
        }
        
        TCHAR *buffer = new TCHAR[MAX_PATH];
        if (!SHGetPathFromIDList(pIDL, buffer) != 0) {
                CoTaskMemFree(pIDL);
                return string ();
        }
        string selected_path = buffer;
        
        CoTaskMemFree(pIDL);
        OleUninitialize();

        return selected_path;
}
