//
//  AppDelegate.h
//  CrashReporter
//
//  Created by Taybin Rutkin on 6/10/09.
//  Copyright 2009 Penguin Sounds. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface AppDelegate : NSObject {
    IBOutlet NSFormCell *nameField;
    IBOutlet NSFormCell *emailField;
    IBOutlet NSTextView  *descriptionView;
    IBOutlet NSTextView  *stackTrackView;
}

- (IBAction)sendButton:(id)sender;

@end
