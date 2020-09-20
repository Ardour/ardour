/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include <vector>

#include "pbd/error.h"

#include "ardour/audiofilesource.h"
#include "ardour/readable.h"
#include "ardour/session.h"
#include "ardour/srcfilesource.h"
#include "ardour/source_factory.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

std::vector<boost::shared_ptr<AudioReadable> >
AudioReadable::load (Session& session, std::string const& path)
{
	std::vector<boost::shared_ptr<AudioReadable> > readables;

	ARDOUR::SoundFileInfo sf_info;
	std::string error_msg;

	if (!AudioFileSource::get_soundfile_info (path, sf_info, error_msg)) {
		PBD::error << string_compose(_("Cannot open File \"%1\": %2"), path, error_msg) << endmsg;
		throw failed_constructor ();
	}

	for (unsigned int n = 0; n < sf_info.channels; ++n) {
		try {
			boost::shared_ptr<AudioFileSource> afs;
			afs = boost::dynamic_pointer_cast<AudioFileSource> (
					SourceFactory::createExternal (DataType::AUDIO, session,
						path, n,
						Source::Flag (ARDOUR::AudioFileSource::NoPeakFile), false));

			if (afs->sample_rate() != session.nominal_sample_rate()) {
				boost::shared_ptr<SrcFileSource> sfs (new SrcFileSource(session, afs, ARDOUR::SrcBest));
				readables.push_back(sfs);
			} else {
				readables.push_back (afs);
			}
		} catch (failed_constructor& err) {
			PBD::error << string_compose(_("Could not read file \"%1\"."), path) << endmsg;
			throw failed_constructor ();
		}
	}
	return readables;
}
