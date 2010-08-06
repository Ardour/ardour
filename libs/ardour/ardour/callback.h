#ifndef __libardour_callback_h__
#define __libardour_callback_h__

#include <string>

void call_the_mothership (const std::string& version);
void block_mothership ();
void unblock_mothership ();
bool mothership_blocked ();

#endif /* __libardour_callback_h__ */
