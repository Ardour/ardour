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

// This is the C "trampoline" function that will be used
// to invoke a specific Objective-C method FROM C++
namespace ARDOUR
{
    #ifdef __APPLE__
    std::string SaveFileDialog(std::string title = "");
    std::string OpenFileDialog(std::string title = "");
	#endif
		
	#ifdef _WIN32
	bool SaveFileDialog(std::string& fileName, std::string title = "Save");
    bool OpenFileDialog(std::string& fileName, std::string title = "Open");
	#endif
}
#endif
