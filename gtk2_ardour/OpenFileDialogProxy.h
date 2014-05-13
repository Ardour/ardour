//
//  OpenFileDialogProxy.h
//  Tracks
//
//  Created by User on 5/12/14.
//
//

#include <string>

#ifndef Tracks_OpenFileDialogProxy_h
#define Tracks_OpenFileDialogProxy_h

// This is the C "trampoline" function that will be used
// to invoke a specific Objective-C method FROM C++
namespace ARDOUR
{
    std::string SaveFileDialog(std::string title = "");
    std::string OpenFileDialog(std::string title = "");
}
#endif
