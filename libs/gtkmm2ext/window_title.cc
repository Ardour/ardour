#include "gtkmm2ext/window_title.h"

#include "i18n.h"

namespace {
	
// I don't know if this should be translated.
const char* const title_separator = X_(" - ");

} // anonymous namespace

namespace Gtkmm2ext {

WindowTitle::WindowTitle(const string& title)
	: m_title(title)
{

}

void
WindowTitle::operator+= (const string& element)
{
	m_title = m_title + title_separator + element;
}

}
