/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "dcp_subtitle.h"
#include "exceptions.h"
#include <dcp/interop_subtitle_content.h>
#include <dcp/smpte_subtitle_content.h>

#include "i18n.h"

using boost::shared_ptr;

shared_ptr<dcp::SubtitleContent>
DCPSubtitle::load (boost::filesystem::path file) const
{
	shared_ptr<dcp::SubtitleContent> sc;
	
	try {
		sc.reset (new dcp::InteropSubtitleContent (file));
	} catch (...) {
		
	}

	if (!sc) {
		try {
			sc.reset (new dcp::SMPTESubtitleContent (file, true));
		} catch (...) {

		}
	}

	if (!sc) {
		try {
			sc.reset (new dcp::SMPTESubtitleContent (file, false));
		} catch (...) {

		}
	}

	if (!sc) {
		throw FileError (_("Could not read subtitles"), file);
	}

	return sc;
}
