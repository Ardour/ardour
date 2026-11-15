#include "folder_streamview.h"

using namespace ArdourCanvas;
using namespace ARDOUR;

FolderStreamView::FolderStreamView (Item* parent, std::shared_ptr<Folder> f)
	: Container (parent)
	, _folder (f)
{
}

