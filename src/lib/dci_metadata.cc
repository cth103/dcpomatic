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

#include <iostream>
#include "dci_metadata.h"

#include "i18n.h"

using namespace std;

void
DCIMetadata::write (ostream& f) const
{
	f << N_("audio_language ") << audio_language << N_("\n");
	f << N_("subtitle_language ") << subtitle_language << N_("\n");
	f << N_("territory ") << territory << N_("\n");
	f << N_("rating ") << rating << N_("\n");
	f << N_("studio ") << studio << N_("\n");
	f << N_("facility ") << facility << N_("\n");
	f << N_("package_type ") << package_type << N_("\n");
}

void
DCIMetadata::read (string k, string v)
{
	if (k == N_("audio_language")) {
		audio_language = v;
	} else if (k == N_("subtitle_language")) {
		subtitle_language = v;
	} else if (k == N_("territory")) {
		territory = v;
	} else if (k == N_("rating")) {
		rating = v;
	} else if (k == N_("studio")) {
		studio = v;
	} else if (k == N_("facility")) {
		facility = v;
	} else if (k == N_("package_type")) {
		package_type = v;
	}
}	
