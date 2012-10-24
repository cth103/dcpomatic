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
#include <boost/bind.hpp>
#include "lib/film.h"
#include "lib/config.h"
#include "properties_dialog.h"
#include "wx_util.h"

using namespace std;
using namespace boost;

PropertiesDialog::PropertiesDialog (wxWindow* parent, Film* film)
	: wxDialog (parent, wxID_ANY, _("Film Properties"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
	, _film (film)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 3, 6);

	add_label_to_sizer (table, this, "Frames");
	_frames = new wxStaticText (this, wxID_ANY, std_to_wx (""));
	table->Add (_frames, 1, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer (table, this, "Disk space required for frames");
	_disk_for_frames = new wxStaticText (this, wxID_ANY, std_to_wx (""));
	table->Add (_disk_for_frames, 1, wxALIGN_CENTER_VERTICAL);
	
	add_label_to_sizer (table, this, "Total disk space required");
	_total_disk = new wxStaticText (this, wxID_ANY, std_to_wx (""));
	table->Add (_total_disk, 1, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer (table, this, "Frames already encoded");
	_encoded = new ThreadedStaticText (this, "counting...", boost::bind (&PropertiesDialog::frames_already_encoded, this));
	table->Add (_encoded, 1, wxALIGN_CENTER_VERTICAL);

	if (_film->length()) {
		_frames->SetLabel (std_to_wx (lexical_cast<string> (_film->length().get())));
		double const disk = ((double) Config::instance()->j2k_bandwidth() / 8) * _film->length().get() / (_film->frames_per_second () * 1073741824);
		stringstream s;
		s << fixed << setprecision (1) << disk << "Gb";
		_disk_for_frames->SetLabel (std_to_wx (s.str ()));
		stringstream t;
		t << fixed << setprecision (1) << (disk * 2) << "Gb";
		_total_disk->SetLabel (std_to_wx (t.str ()));
	} else {
		_frames->SetLabel (_("unknown"));
		_disk_for_frames->SetLabel (_("unknown"));
		_total_disk->SetLabel (_("unknown"));
	}

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 0, wxALL, 6);
	
	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->SetSizeHints (this);
}

string
PropertiesDialog::frames_already_encoded () const
{
	stringstream u;
	try {
		u << _film->encoded_frames ();
	} catch (thread_interrupted &) {
		return "";
	}
	
	if (_film->dcp_length()) {
		u << " (" << (_film->encoded_frames() * 100 / _film->dcp_length().get()) << "%)";
	}
	return u.str ();
}
