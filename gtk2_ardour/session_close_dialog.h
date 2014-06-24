//
//  session_close_dialog.h
//  Tracks
//
//  Created by User on 6/12/14.
//
//

#ifndef __Tracks__session_close_dialog__
#define __Tracks__session_close_dialog__

#include <string>
#include "waves_dialog.h"

class EngineControl;

class SessionCloseDialog : public WavesDialog {
public:
    
    SessionCloseDialog ();
    
private:
	WavesButton& _cancel_button;
    WavesButton& _dont_save_button;
    WavesButton& _save_button;
    
	void on_cancel(WavesButton*);
    void on_dont_save(WavesButton*);
    void on_save(WavesButton*);

public:
    Gtk::Label& _top_label;
    Gtk::Label& _bottom_label;
};


#endif /* defined(__Tracks__session_close_dialog__) */
