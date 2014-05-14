//
//  OpenFileDialogProxy.h
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
    std::string SaveFileDialog(std::string title = "Save");
    std::string OpenFileDialog(std::string title = "Open");
	#endif
	
	// OS Windows specific functions
	#ifdef _WIN32
	bool SaveFileDialog(std::string& fileName, std::string title = "Save");
    bool OpenFileDialog(std::string& fileName, std::string title = "Open");
	#endif
}
#endif
