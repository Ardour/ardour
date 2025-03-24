/*
 * Copyright (C) 2011 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2013 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <memory>

#include <boost/operators.hpp>

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

typedef std::shared_ptr<ExportChannelConfiguration> ExportChannelConfigPtr;
typedef std::shared_ptr<ExportFormatBase> ExportFormatBasePtr;
typedef std::shared_ptr<ExportFormat> ExportFormatPtr;
typedef std::shared_ptr<ExportFormatSpecification> ExportFormatSpecPtr;
typedef std::shared_ptr<ExportFormatCompatibility> ExportFormatCompatibilityPtr;
typedef std::shared_ptr<ExportFilename> ExportFilenamePtr;
typedef std::shared_ptr<ExportStatus> ExportStatusPtr;
typedef std::shared_ptr<ExportPreset> ExportPresetPtr;

typedef std::weak_ptr<ExportFormatCompatibility> WeakExportFormatCompatibilityPtr;
typedef std::weak_ptr<ExportFormat> WeakExportFormatPtr;

typedef std::shared_ptr<AudioGrapher::BroadcastInfo> BroadcastInfoPtr;

} // namespace ARDOUR

