/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "metadata_dialog.h"
#include "editable_list.h"
#include "rating_dialog.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <wx/gbsizer.h>

using std::string;
using std::vector;
using boost::weak_ptr;
using boost::shared_ptr;

static string
column (dcp::Rating r, int c)
{
	if (c == 0) {
		return r.agency;
	}

	return r.label;
}

MetadataDialog::MetadataDialog (wxWindow* parent, weak_ptr<Film> film)
	: wxDialog (parent, wxID_ANY, _("Metadata"))
	, _film (film)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxFlexGridSizer* sizer = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	sizer->AddGrowableCol (1, 1);

	{
		int flags = wxALIGN_TOP | wxLEFT | wxRIGHT | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
#endif
		wxStaticText* m = create_label (this, _("Ratings"), true);
		sizer->Add (m, 0, flags, DCPOMATIC_SIZER_GAP);
	}

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(_("Agency"), 200, true));
	columns.push_back (EditableListColumn(_("Label"), 50, true));
	_ratings = new EditableList<dcp::Rating, RatingDialog> (
		this,
		columns,
		boost::bind(&MetadataDialog::ratings, this),
		boost::bind(&MetadataDialog::set_ratings, this, _1),
		boost::bind(&column, _1, _2),
		true,
		false
		);
	sizer->Add (_ratings, 1, wxEXPAND);

	add_label_to_sizer (sizer, this, _("Content version"), true);
	_content_version = new wxTextCtrl (this, wxID_ANY);
	sizer->Add (_content_version, 1, wxEXPAND);

	shared_ptr<Film> f = _film.lock();
	DCPOMATIC_ASSERT (f);
	_content_version->SetValue (std_to_wx(f->content_version()));

	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_content_version->Bind (wxEVT_TEXT, boost::bind(&MetadataDialog::content_version_changed, this));
	_content_version->SetFocus ();
}

vector<dcp::Rating>
MetadataDialog::ratings () const
{
	shared_ptr<Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return film->ratings ();
}

void
MetadataDialog::set_ratings (vector<dcp::Rating> r)
{
	shared_ptr<Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	film->set_ratings (r);
}

void
MetadataDialog::content_version_changed ()
{
	shared_ptr<Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	film->set_content_version (wx_to_std(_content_version->GetValue()));
}
