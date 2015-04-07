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

#import "open_file_dialog.h"
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include <iostream>

// An Objective-C class that needs to be accessed from C++
@interface FileDialog : NSObject
{
	
}

@end

using namespace std;

/* ====== "trampoline" functions to invoke Objective-C method ====== */

std::string
ARDOUR::open_file_dialog (std::string initial_path, std::string title)
{
	NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
    
	//NP: we should find some gentle way to do this
	NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
	// Call the Objective-C method using Objective-C syntax
	NSString *nsPath = [FileDialog class_open_file_dialog:nsTitle withArg2:nsDefaultPath];
	std::string stdPath = [nsPath UTF8String];
	
	return stdPath;
}

std::vector<std::string>
ARDOUR::open_file_dialog (std::vector<std::string> extensions, bool multi_selection, std::string initial_path, std::string title)
{
    NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
    //NP: we should find some gentle way to do this
    NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
    
    id fileTypesArray = [NSMutableArray new];
    
    for (std::vector<std::string>::iterator it = extensions.begin(); it != extensions.end(); ++it) {
		id nsstr = [NSString stringWithUTF8String:(*it).c_str()];
		[fileTypesArray addObject:nsstr];
	}
	
    NSArray *nsPathes = [FileDialog class_open_file_dialog:nsTitle withArg2:nsDefaultPath withArg3:fileTypesArray withArg4:multi_selection];
    
    std::vector<std::string> stdPathes;
    
    int count = [nsPathes count];
    for (int i=0; i<count; i++) {
        NSURL *saveURL = [nsPathes objectAtIndex:i];
        NSString *filePath = [saveURL path];
        std::string stdPath = [filePath UTF8String];
        stdPathes.push_back (stdPath);
    }
    
    // Returns pathes to selected files
    return stdPathes;
}


std::string
ARDOUR::save_file_dialog (std::vector<std::string> extensions,
						  std::string initial_path,
						  std::string title)
{
	NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
	
	//NP: we should find some gentle way to do this
	NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
	
	id fileTypesArray = [NSMutableArray new];

    for (std::vector<std::string>::iterator it = extensions.begin(); it != extensions.end(); ++it) {
		id nsstr = [NSString stringWithUTF8String:(*it).c_str()];
		[fileTypesArray addObject:nsstr];
	}
	
	// Call the Objective-C method using Objective-C syntax
	NSString *nsPath = [FileDialog class_save_file_dialog:nsTitle withArg2:nsDefaultPath withArg3:fileTypesArray];
	std::string stdPath = [nsPath UTF8String];
	
	return stdPath;
}

std::string
ARDOUR::save_file_dialog (std::string initial_path, std::string title)
{
	NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];

	id fileTypesArray = [NSMutableArray new];
	
	//NP: we should find some gentle way to do this
	NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
	// Call the Objective-C method using Objective-C syntax
	NSString *nsPath = [FileDialog class_save_file_dialog:nsTitle withArg2:nsDefaultPath withArg3:fileTypesArray];
	
	std::string stdPath = [nsPath UTF8String];
	return stdPath;
}

std::string
ARDOUR::save_as_file_dialog (std::string initial_path, std::string title, bool& copy_media)
{
    NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];
    
    //NP: we should find some gentle way to do this
    NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
    // Call the Objective-C method using Objective-C syntax
    NSString *nsPath = [FileDialog class_save_as_file_dialog:nsTitle withArg2:nsDefaultPath withArg3: &copy_media];
    
    std::string stdPath = [nsPath UTF8String];
    
    return stdPath;
}

std::string
ARDOUR::choose_folder_dialog(std::string initial_path, std::string title)
{
	NSString *nsTitle = [NSString stringWithUTF8String:title.c_str()];

	//NP: we should find some gentle way to do this
    NSString *nsDefaultPath = [NSString stringWithUTF8String:initial_path.c_str()];
  
	// Call the Objective-C method using Objective-C syntax
    NSString *nsPath = [FileDialog class_choose_folder_dialog:nsTitle withArg2:nsDefaultPath];
            
    std::string stdPath = [nsPath UTF8String];
        
    return stdPath;
}  

/* ====== Objective-C functions called from C++ functions ====== */

@implementation FileDialog 

/* On open saved session */
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

/* On choose many files */
+ (NSArray*) class_open_file_dialog:(NSString *)title
						   withArg2:(NSString *)initial_path
						   withArg3:(NSArray*) fileTypesArray
						   withArg4:(bool) multiSelection
{
    // Create a File Open Dialog class.
    NSOpenPanel* openDlg = [NSOpenPanel openPanel];
    
    [openDlg setCanChooseFiles:YES];
    [openDlg setAllowedFileTypes:fileTypesArray];
	[openDlg setAllowsMultipleSelection:multiSelection];
    [openDlg setTitle:title];

	NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:initial_path isDirectory:&isDir];
    
    if(!exists) {
        initial_path = NSHomeDirectory();
    }
	
    [openDlg setDirectoryURL : [NSURL fileURLWithPath:initial_path]];
    
    // Display the dialog box.  If the OK pressed,
    // process the files.
    if ( [openDlg runModal] == NSOKButton )
    {
        // Gets first selected file
        NSArray *files = [openDlg URLs];
        
        return files;
    }
    
    return nil;
}

/* On create new session */
+ (NSString*) class_save_file_dialog:(NSString *)title
							withArg2:(NSString *)initial_path
							withArg3:(NSArray*) fileTypesArray
{
    // Create a File Open Dialog class.
    NSSavePanel* saveDlg = [NSSavePanel savePanel];
    [saveDlg setTitle:title];
    [saveDlg setCanCreateDirectories:YES];
	
	if ([fileTypesArray count]) {
		[saveDlg setAllowedFileTypes:fileTypesArray];
	}
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:initial_path isDirectory:&isDir];
    
    if(!exists) {
        initial_path = NSHomeDirectory();
    }
	
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

/* On save-as session */
+ (NSString*) class_save_as_file_dialog:(NSString *)title withArg2:(NSString *)initial_path withArg3:(bool *)copy_media
{
    NSSavePanel* saveDlg = [NSSavePanel savePanel];
    [saveDlg setTitle:title];
    [saveDlg setCanCreateDirectories:YES];
    
    NSFileManager *fm = [[NSFileManager alloc] init];
    BOOL isDir;
    BOOL exists = [fm fileExistsAtPath:initial_path isDirectory:&isDir];
    
    if(!exists)
        initial_path = NSHomeDirectory();
    
    [saveDlg setDirectoryURL : [NSURL fileURLWithPath:initial_path]];
    
    // add checkBox
    NSButton *button = [[NSButton alloc] init];
    [button setButtonType:NSSwitchButton];
    button.title = NSLocalizedString(@"Copy external media", @"");
    [button sizeToFit];
    [saveDlg setAccessoryView:button];
    saveDlg.delegate = self;
    
    // Display the dialog box.  If the OK pressed,
    // process the files.
    if ( [saveDlg runModal] == NSOKButton )
    {
        // Gets list of all files selected
        NSURL *saveURL = [saveDlg URL];
        NSString *filePath = [saveURL path];
        *copy_media = (((NSButton*)saveDlg.accessoryView).state == NSOnState);
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
