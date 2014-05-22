//
//  open_file_dialog_proxy.h
//  Tracks
//
//  Created by User on 5/12/14.
//
//

#ifndef Tracks_OpenFileDialogProxy_h
#define Tracks_OpenFileDialogProxy_h

#include <string>

namespace ARDOUR
{
    // This is the C "trampoline" function that will be used
    // to invoke a specific Objective-C method FROM C++
    #ifdef __APPLE__
    std::string save_file_dialog(std::string initial_path = "", std::string title = "Save");
    std::string open_file_dialog(std::string initial_path = "", std::string title = "Open");
    std::string choose_folder_dialog(std::string initial_path = "", std::string title = "Choose Folder");
	#endif
	
	// OS Windows specific functions
	#ifdef _WIN32
	bool save_file_dialog(std::string& file_name, std::string initial_path = "", std::string title = "Save");
    bool open_file_dialog(std::string& file_name, std::string initial_path = "", std::string title = "Open");
    bool choose_folder_dialog(std::string& selected_path, std::string title = "Choose Folder");
	#endif
}
#endif
