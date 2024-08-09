/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "dcp_text_track_dialog.h"
#include "language_tag_widget.h"
#include <boost/algorithm/string.hpp>


using std::string;


DCPTextTrackDialog::DCPTextTrackDialog (wxWindow* parent)
	: TableDialog (parent, _("DCP Text Track"), 2, 1, true)
{
	add (_("Name"), true);
	add (_name = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxSize(300, -1)));
	add (_("Language"), true);
	_language = new LanguageTagWidget(this, {}, boost::none);
	add (_language->sizer());

	_language->Changed.connect(boost::bind(&DCPTextTrackDialog::set_sensitivity, this));

	set_sensitivity();
	layout ();
}


DCPTextTrack
DCPTextTrackDialog::get () const
{
	DCPOMATIC_ASSERT (_language->get());
	return DCPTextTrack(wx_to_std(_name->GetValue()), _language->get());
}


void
DCPTextTrackDialog::set_sensitivity()
{
	if (auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this))) {
		ok->Enable(static_cast<bool>(_language->get()));
	}
}

