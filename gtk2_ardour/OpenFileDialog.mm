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

namespace ARDOUR
{
    // ====== C "trampoline" functions to invoke Objective-C method ====== //
    string OpenFileDialog(std::string path, string title)
    {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        //NP: we should find some gentle way to do this
        NSString *nsDefaultPath = [NSString stringWithUTF8String:path.c_str()];
        // Call the Objective-C method using Objective-C syntax
        NSString *nsPath = [FileDialog ClassOpenFileDialog:nsTitle withArg2:nsDefaultPath];
        string stdPath = [nsPath UTF8String];
        
        return stdPath;
    }

    string SaveFileDialog(std::string path, string title)
    {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        //NP: we should find some gentle way to do this
        NSString *nsDefaultPath = [NSString stringWithUTF8String:path.c_str()];
        // Call the Objective-C method using Objective-C syntax
        NSString *nsPath = [FileDialog ClassSaveFileDialog:nsTitle withArg2:nsDefaultPath];
        string stdPath = [nsPath UTF8String];    
        
        return stdPath;
    }
    
    string ChooseFolderDialog(std::string path, string title)
    {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        //NP: we should find some gentle way to do this
        NSString *nsDefaultPath = [NSString stringWithUTF8String:path.c_str()];
        // Call the Objective-C method using Objective-C syntax
        NSString *nsPath = [FileDialog ClassChooseFolderDialog:nsTitle withArg2:nsDefaultPath];
            
        string stdPath = [nsPath UTF8String];
        
        return stdPath;
    }  
}// namespace ARDOUR

// ====== Objective-C functions called from C++ functions ====== //

// On open saved session
+ (NSString*) ClassOpenFileDialog:(NSString *)title withArg2:(NSString *)path
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
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:path isDirectory:&isDir];
    
    if(!exists)
        path = NSHomeDirectory();
    
    [openDlg setDirectoryURL : [NSURL fileURLWithPath:path]];
    
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
+ (NSString*) ClassSaveFileDialog:(NSString *)title withArg2:(NSString *)path
{    
    // Create a File Open Dialog class.
    NSSavePanel* saveDlg = [NSSavePanel savePanel];
    [saveDlg setTitle:title];
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:path isDirectory:&isDir];
    
    if(!exists)
        path = NSHomeDirectory();
    
    [saveDlg setDirectoryURL : [NSURL fileURLWithPath:path]];
    
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

+ (NSString*) ClassChooseFolderDialog:(NSString *)title withArg2:(NSString *)path
{
    // Create a File Open Dialog class.
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    [openDlg setCanChooseDirectories:YES];
    [openDlg setAllowsMultipleSelection:FALSE];
    [openDlg setTitle:title];
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:path isDirectory:&isDir];
    
    if(!exists)
        path = NSHomeDirectory();

    [openDlg setDirectoryURL : [NSURL fileURLWithPath:path]];
    
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

@end
