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

using namespace std;

void
DCIMetadata::write (ostream& f) const
{
	f << "audio_language " << audio_language << "\n";
	f << "subtitle_language " << subtitle_language << "\n";
	f << "territory " << territory << "\n";
	f << "rating " << rating << "\n";
	f << "studio " << studio << "\n";
	f << "facility " << facility << "\n";
	f << "package_type " << package_type << "\n";
}

void
DCIMetadata::read (string k, string v)
{
	if (k == "audio_language") {
		audio_language = v;
	} else if (k == "subtitle_language") {
		subtitle_language = v;
	} else if (k == "territory") {
		territory = v;
	} else if (k == "rating") {
		rating = v;
	} else if (k == "studio") {
		studio = v;
	} else if (k == "facility") {
		facility = v;
	} else if (k == "package_type") {
		package_type = v;
	}
}	
