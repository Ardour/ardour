/*
    Copyright (C) 2011 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_export_pointers_h__
#define __ardour_export_pointers_h__

#include <boost/operators.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include "ardour/libardour_visibility.h"
#include "ardour/comparable_shared_ptr.h"

namespace AudioGrapher {
	class BroadcastInfo;
}

namespace ARDOUR {

class ExportTimespan;
class ExportChannel;
class ExportChannelConfiguration;
class ExportFormat;
class ExportFormatBase;
class ExportFormatSpecification;
class ExportFormatCompatibility;
class ExportFilename;
class ExportStatus;
class ExportPreset;

typedef ComparableSharedPtr<ExportChannel> ExportChannelPtr;
typedef ComparableSharedPtr<ExportTimespan> ExportTimespanPtr;

typedef boost::shared_ptr<ExportChannelConfiguration> ExportChannelConfigPtr;
typedef boost::shared_ptr<ExportFormatBase> ExportFormatBasePtr;
typedef boost::shared_ptr<ExportFormat> ExportFormatPtr;
typedef boost::shared_ptr<ExportFormatSpecification> ExportFormatSpecPtr;
typedef boost::shared_ptr<ExportFormatCompatibility> ExportFormatCompatibilityPtr;
typedef boost::shared_ptr<ExportFilename> ExportFilenamePtr;
typedef boost::shared_ptr<ExportStatus> ExportStatusPtr;
typedef boost::shared_ptr<ExportPreset> ExportPresetPtr;

typedef boost::weak_ptr<ExportFormatCompatibility> WeakExportFormatCompatibilityPtr;
typedef boost::weak_ptr<ExportFormat> WeakExportFormatPtr;

typedef boost::shared_ptr<AudioGrapher::BroadcastInfo> BroadcastInfoPtr;

} // namespace ARDOUR

#endif // __ardour_export_pointers_h__
