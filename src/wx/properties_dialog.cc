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

#include <iomanip>
#include <boost/lexical_cast.hpp>
#include "lib/film.h"
#include "lib/config.h"
#include "properties_dialog.h"
#include "wx_util.h"

using namespace std;
using namespace boost;

PropertiesDialog::PropertiesDialog (wxWindow* parent, Film* film)
	: wxDialog (parent, wxID_ANY, _("Film Properties"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);

	add_label_to_sizer (table, this, "Frames");
	_frames = new wxStaticText (this, wxID_ANY, std_to_wx (""));
	table->Add (_frames, 1, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer (table, this, "Disk space for frames");
	_disk_for_frames = new wxStaticText (this, wxID_ANY, std_to_wx (""));
	table->Add (_disk_for_frames, 1, wxALIGN_CENTER_VERTICAL);
	
	add_label_to_sizer (table, this, "Total disk space");
	_total_disk = new wxStaticText (this, wxID_ANY, std_to_wx (""));
	table->Add (_total_disk, 1, wxALIGN_CENTER_VERTICAL);

	_frames->SetLabel (std_to_wx (lexical_cast<string> (film->length ())));
	double const disk = ((double) Config::instance()->j2k_bandwidth() / 8) * film->length() / (film->frames_per_second () * 1073741824);
	stringstream s;
	s << fixed << setprecision (1) << disk << "Gb";
	_disk_for_frames->SetLabel (std_to_wx (s.str ()));

	stringstream t;
	t << fixed << setprecision (1) << (disk * 2) << "Gb";
	_total_disk->SetLabel (std_to_wx (t.str ()));

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->SetSizeHints (this);
}
