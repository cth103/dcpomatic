/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <sstream>
#include "lib/film.h"
#include "lib/exceptions.h"
#include "job_wrapper.h"
#include "wx_util.h"

using boost::shared_ptr;

void
JobWrapper::make_dcp (wxWindow* parent, shared_ptr<Film> film, bool transcode)
{
	if (!film) {
		return;
	}

	try {
		film->make_dcp (transcode);
	} catch (BadSettingError& e) {
		error_dialog (parent, String::compose ("Bad setting for %1 (%2)", e.setting(), e.what ()));
	} catch (std::exception& e) {
		error_dialog (parent, String::compose ("Could not make DCP: %1", e.what ()));
	}
}
