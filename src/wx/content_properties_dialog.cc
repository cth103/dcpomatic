/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "content_properties_dialog.h"
#include "wx_util.h"
#include "lib/raw_convert.h"
#include "lib/content.h"
#include "lib/video_content.h"
#include "lib/audio_content.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

using std::string;
using std::list;
using std::pair;
using std::map;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

ContentPropertiesDialog::ContentPropertiesDialog (wxWindow* parent, shared_ptr<Content> content)
	: TableDialog (parent, _("Content Properties"), 2, 1, false)
{
	map<UserProperty::Category, list<UserProperty> > grouped;
	BOOST_FOREACH (UserProperty i, content->user_properties()) {
		if (grouped.find(i.category) == grouped.end()) {
			grouped[i.category] = list<UserProperty> ();
		}
		grouped[i.category].push_back (i);
	}

	maybe_add_group (grouped, UserProperty::GENERAL);
	maybe_add_group (grouped, UserProperty::VIDEO);
	maybe_add_group (grouped, UserProperty::AUDIO);
	maybe_add_group (grouped, UserProperty::LENGTH);

	layout ();

	/* SetSizeHints() seems to get it slightly wrong (see #884),
	   so hack in a bit more height.
	*/
	wxSize const s = GetMinSize ();
	SetMinSize (wxSize (s.GetWidth(), s.GetHeight() + 32));
}

void
ContentPropertiesDialog::maybe_add_group (map<UserProperty::Category, list<UserProperty> > const & groups, UserProperty::Category category)
{
	map<UserProperty::Category, list<UserProperty> >::const_iterator i = groups.find (category);
	if (i == groups.end()) {
		return;
	}

	wxString category_name;
	switch (i->first) {
	case UserProperty::GENERAL:
		category_name = _("General");
		break;
	case UserProperty::VIDEO:
		category_name = _("Video");
		break;
	case UserProperty::AUDIO:
		category_name = _("Audio");
		break;
	case UserProperty::LENGTH:
		category_name = _("Length");
		break;
	}

	wxStaticText* m = new wxStaticText (this, wxID_ANY, category_name);
	wxFont font (*wxNORMAL_FONT);
	font.SetWeight (wxFONTWEIGHT_BOLD);
	m->SetFont (font);

	add_spacer ();
	add_spacer ();
	add (m, false);
	add_spacer ();

	BOOST_FOREACH (UserProperty j, i->second) {
		add (std_to_wx (j.key), true);
		add (new wxStaticText (this, wxID_ANY, std_to_wx (j.value + " " + j.unit)));
	}
}
