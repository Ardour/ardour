//
//  OpenFileDialog.h
//  Tracks
//
//  Created by User on 5/8/14.
//
//

#import <Foundation/Foundation.h>
#import "OpenFileDialogProxy.h"

// An Objective-C class that needs to be accessed from C++
@interface FileDialog : NSObject
{

}

// The Objective-C member function you want to call from C++
+ (NSString*) ClassSaveFileDialog:(NSString *) title;
+ (NSString*) ClassOpenFileDialog:(NSString *) title;

@end


