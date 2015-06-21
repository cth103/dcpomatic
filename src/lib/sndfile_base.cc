/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifdef DCPOMATIC_WINDOWS
#include <windows.h>
#define ENABLE_SNDFILE_WINDOWS_PROTOTYPES 1
#endif
#include <sndfile.h>

#include "sndfile_base.h"
#include "sndfile_content.h"
#include "exceptions.h"

#include "i18n.h"

using boost::shared_ptr;

Sndfile::Sndfile (shared_ptr<const SndfileContent> c)
	: _sndfile_content (c)
{
	_info.format = 0;

	/* Here be monsters.  See fopen_boost for similar shenanigans */
#ifdef DCPOMATIC_WINDOWS
	_sndfile = sf_wchar_open (_sndfile_content->path(0).c_str(), SFM_READ, &_info);
#else
	_sndfile = sf_open (_sndfile_content->path(0).string().c_str(), SFM_READ, &_info);
#endif

	if (!_sndfile) {
		throw DecodeError (_("could not open audio file for reading"));
	}
}

Sndfile::~Sndfile ()
{
	sf_close (_sndfile);
}
