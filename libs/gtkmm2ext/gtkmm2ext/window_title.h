#ifndef WINDOW_TITLE_INCLUDED
#define WINDOW_TITLE_INCLUDED

#include <string>

namespace Gtkmm2ext {

using std::string;

/**
 * \class The WindowTitle class can be used to maintain the 
 * consistancy of window titles between windows and dialogs.
 *
 * Each string element that is added to the window title will
 * be separated by a hyphen.
 */
class WindowTitle
{
public:

	/**
	 * \param title The first string/element of the window title 
	 * which will may be the application name or the document 
	 * name in a document based application.
	 */
	WindowTitle(const string& title);

	/**
	 * Add an string element to the window title.
	 */
	void operator+= (const string&);

	/// @return The window title string.
	const string& get_string () { return m_title;}

private:

	string                         m_title;

};

} // Gtkmm2ext

#endif // WINDOW_TITLE_INCLUDED
