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

using std::string;
using std::stringstream;
using std::fixed;
using std::setprecision;
using boost::shared_ptr;
using boost::lexical_cast;

PropertiesDialog::PropertiesDialog (wxWindow* parent, shared_ptr<Film> film)
	: wxDialog (parent, wxID_ANY, _("Film Properties"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
	, _film (film)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);

	add_label_to_sizer (table, this, _("Frames"), true);
	_frames = new wxStaticText (this, wxID_ANY, wxT (""));
	table->Add (_frames, 1, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer (table, this, _("Disk space required"), true);
	_disk = new wxStaticText (this, wxID_ANY, wxT (""));
	table->Add (_disk, 1, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer (table, this, _("Frames already encoded"), true);
	_encoded = new ThreadedStaticText (this, _("counting..."), boost::bind (&PropertiesDialog::frames_already_encoded, this));
	table->Add (_encoded, 1, wxALIGN_CENTER_VERTICAL);

	_frames->SetLabel (std_to_wx (lexical_cast<string> (_film->time_to_video_frames (_film->length()))));
	double const disk = ((double) _film->j2k_bandwidth() / 8) * _film->length() / (TIME_HZ * 1073741824.0f);
	stringstream s;
	s << fixed << setprecision (1) << disk << wx_to_std (_("Gb"));
	_disk->SetLabel (std_to_wx (s.str ()));

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (table, 0, wxALL, DCPOMATIC_DIALOG_BORDER);
	
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
	} catch (boost::thread_interrupted &) {
		return "";
	}
	
	if (_film->length()) {
		/* XXX: encoded_frames() should check which frames have been encoded */
		u << " (" << (_film->encoded_frames() * 100 / _film->time_to_video_frames (_film->length())) << "%)";
	}
	return u.str ();
}
