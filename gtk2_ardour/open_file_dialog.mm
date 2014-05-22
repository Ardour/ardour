//
//  OpenFileDialog.m
//  Tracks
//
//  Created by User on 5/8/14.
//
//

#import "open_file_dialog.h"
#import <Cocoa/Cocoa.h>

#include <string>

#include <iostream>

using namespace std;

@implementation FileDialog 

namespace ARDOUR
{
    // ====== C "trampoline" functions to invoke Objective-C method ====== //
    string open_file_dialog(std::string initial_path, string title)
    {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        //NP: we should find some gentle way to do this
        NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
        // Call the Objective-C method using Objective-C syntax
        NSString *nsPath = [FileDialog class_open_file_dialog:nsTitle withArg2:nsDefaultPath];
        string stdPath = [nsPath UTF8String];
        
        return stdPath;
    }

    string save_file_dialog(std::string initial_path, string title)
    {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        //NP: we should find some gentle way to do this
        NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
        // Call the Objective-C method using Objective-C syntax
        NSString *nsPath = [FileDialog class_save_file_dialog:nsTitle withArg2:nsDefaultPath];
        string stdPath = [nsPath UTF8String];    
        
        return stdPath;
    }
    
    string choose_folder_dialog(std::string initial_path, string title)
    {
        NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
        
        //NP: we should find some gentle way to do this
        NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
        // Call the Objective-C method using Objective-C syntax
        NSString *nsPath = [FileDialog class_choose_folder_dialog:nsTitle withArg2:nsDefaultPath];
            
        string stdPath = [nsPath UTF8String];
        
        return stdPath;
    }  
}// namespace ARDOUR

// ====== Objective-C functions called from C++ functions ====== //

// On open saved session
+ (NSString*) class_open_file_dialog:(NSString *)title withArg2:(NSString *)initial_path
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
    BOOL exists = [fm fileExistsAtPath:initial_path isDirectory:&isDir];
    
    if(!exists)
        initial_path = NSHomeDirectory();
    
    [openDlg setDirectoryURL : [NSURL fileURLWithPath:initial_path]];
    
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
+ (NSString*) class_save_file_dialog:(NSString *)title withArg2:(NSString *)initial_path
{    
    // Create a File Open Dialog class.
    NSSavePanel* saveDlg = [NSSavePanel savePanel];
    [saveDlg setTitle:title];
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:initial_path isDirectory:&isDir];
    
    if(!exists)
        initial_path = NSHomeDirectory();
    
    [saveDlg setDirectoryURL : [NSURL fileURLWithPath:initial_path]];
    
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

+ (NSString*) class_choose_folder_dialog:(NSString *)title withArg2:(NSString *)initial_path
{
    // Create a File Open Dialog class.
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    [openDlg setCanChooseDirectories:YES];
    [openDlg setAllowsMultipleSelection:FALSE];
    [openDlg setTitle:title];
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:initial_path isDirectory:&isDir];
    
    if(!exists)
        initial_path = NSHomeDirectory();

    [openDlg setDirectoryURL : [NSURL fileURLWithPath:initial_path]];
    
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
