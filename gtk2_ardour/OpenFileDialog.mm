//
//  OpenFileDialog.m
//  Tracks
//
//  Created by User on 5/8/14.
//
//

#import "OpenFileDialog.h"
#import <Cocoa/Cocoa.h>

#include <string>

#include <iostream>

using namespace std;

@implementation FileDialog 

// ====== C "trampoline" functions to invoke Objective-C method ====== //
string OpenFileDialog(string title)
{
    if(title.size()==0)
        title = "Open";
    
    NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
    
    // Call the Objective-C method using Objective-C syntax
    NSString *path = [FileDialog ClassOpenFileDialog : nsTitle];
    string stdPath = [path UTF8String];
    
    return stdPath;
}

string SaveFileDialog(string title)
{
    if(title.size()==0)
        title = "Save";
    
    NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
    
    // Call the Objective-C method using Objective-C syntax
    NSString *path = [FileDialog ClassSaveFileDialog : nsTitle];
    string stdPath = [path UTF8String];    
    
    return stdPath;
}


// ====== Objective-C functions called from C++ functions ====== //

// On open saved session
+ (NSString*) ClassOpenFileDialog:(NSString*) title
{
    // Create a File Open Dialog class.
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    // Set array of file types
    NSArray *fileTypesArray;
    fileTypesArray = [NSArray arrayWithObjects:@"ardour", nil];
    
    [openDlg setCanChooseFiles:YES];
    [openDlg setAllowedFileTypes:fileTypesArray];
    [openDlg setAllowsMultipleSelection:FALSE];
    [openDlg setTitle:title];
    
    // Display the dialog box.  If the OK pressed,
    // process the files.
    if ( [openDlg runModal] == NSOKButton )
    {        
        // Gets first selected file
        NSArray *files = [openDlg URLs];
        NSURL *saveURL = [files objectAtIndex:0];
        NSString *filePath = [saveURL path];
        
        return filePath;
    }
    
    return @"";
}

// On create new session
+ (NSString*) ClassSaveFileDialog:(NSString*) title
{    
    // Create a File Open Dialog class.
    NSSavePanel* saveDlg = [NSSavePanel savePanel];
    [saveDlg setTitle:title];
    
    // Display the dialog box.  If the OK pressed,
    // process the files.
    if ( [saveDlg runModal] == NSOKButton )
    {
        // Gets list of all files selected
        NSURL *saveURL = [saveDlg URL];
        NSString *filePath = [saveURL path];
                
        return filePath;
    }
    
    return @"";
}

@end
