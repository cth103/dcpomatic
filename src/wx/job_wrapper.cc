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

using namespace std;

void
JobWrapper::make_dcp (wxWindow* parent, Film* film, bool transcode)
{
	if (!film) {
		return;
	}

	try {
		film->make_dcp (transcode);
	} catch (BadSettingError& e) {
		stringstream s;
		s << "Bad setting for " << e.setting() << "(" << e.what() << ")";
		error_dialog (parent, s.str ());
	} catch (std::exception& e) {
		stringstream s;
		s << "Could not make DCP: " << e.what () << ".";
		error_dialog (parent, s.str ());
	}
}
