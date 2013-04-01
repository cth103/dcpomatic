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
#include <libcxml/cxml.h>
#include "dci_metadata.h"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

DCIMetadata::DCIMetadata (shared_ptr<const cxml::Node> node)
{
	audio_language = node->string_child ("AudioLanguage");
	subtitle_language = node->string_child ("SubtitleLanguage");
	territory = node->string_child ("Territory");
	rating = node->string_child ("Rating");
	studio = node->string_child ("Studio");
	facility = node->string_child ("Facility");
	package_type = node->string_child ("PackageType");
}

void
DCIMetadata::as_xml (xmlpp::Node* root) const
{
	root->add_child("AudioLanguage")->add_child_text (audio_language);
	root->add_child("SubtitleLanguage")->add_child_text (subtitle_language);
	root->add_child("Territory")->add_child_text (territory);
	root->add_child("Rating")->add_child_text (rating);
	root->add_child("Studio")->add_child_text (studio);
	root->add_child("Facility")->add_child_text (facility);
	root->add_child("PackageType")->add_child_text (package_type);
}

void
DCIMetadata::read_old_metadata (string k, string v)
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
