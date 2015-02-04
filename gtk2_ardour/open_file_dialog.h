//
//  OpenFileDialog.h
//  Tracks
//
//  Created by User on 5/8/14.
//
//

#import <Foundation/Foundation.h>
#import "open_file_dialog_proxy.h"

// An Objective-C class that needs to be accessed from C++
@interface FileDialog : NSObject
{

}

// The Objective-C member function you want to call from C++
+ (NSString*) class_save_file_dialog:(NSString *) title withArg2:(NSString *)path;
+ (NSString*) class_save_as_file_dialog:(NSString *) title withArg2:(NSString *)path withArg3: (BOOL*) copy_media;
+ (NSString*) class_open_file_dialog:(NSString *) title withArg2:(NSString *)path;
+ (NSString*) class_choose_folder_dialog:(NSString *) title withArg2:(NSString *)path;

@end


