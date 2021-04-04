/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "editable_list.h"
#include "interop_metadata_dialog.h"
#include "language_tag_widget.h"
#include "rating_dialog.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <wx/gbsizer.h>


using std::string;
using std::vector;
using std::weak_ptr;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


InteropMetadataDialog::InteropMetadataDialog (wxWindow* parent, weak_ptr<Film> film)
	: MetadataDialog (parent, film)
{

}

void
InteropMetadataDialog::setup_standard (wxPanel* panel, wxSizer* sizer)
{
	MetadataDialog::setup_standard (panel, sizer);

	{
		int flags = wxALIGN_TOP | wxLEFT | wxRIGHT | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
#endif
		auto m = create_label (panel, _("Ratings"), true);
		sizer->Add (m, 0, flags, DCPOMATIC_SIZER_GAP);
	}

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn(_("Agency"), 200, true));
	columns.push_back (EditableListColumn(_("Label"), 50, true));
	_ratings = new EditableList<dcp::Rating, RatingDialog> (
		panel,
		columns,
		boost::bind(&InteropMetadataDialog::ratings, this),
		boost::bind(&InteropMetadataDialog::set_ratings, this, _1),
		[](dcp::Rating r, int c) {
			if (c == 0) {
				return r.agency;
			}
			return r.label;
		},
		true,
		false
		);
	sizer->Add (_ratings, 1, wxEXPAND);

	add_label_to_sizer (sizer, panel, _("Content version"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_content_version = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_content_version, 1, wxEXPAND);

	auto cv = film()->content_versions();
	_content_version->SetValue (std_to_wx(cv.empty() ? "" : cv[0]));

	_content_version->Bind (wxEVT_TEXT, boost::bind(&InteropMetadataDialog::content_version_changed, this));
	_content_version->SetFocus ();
}


vector<dcp::Rating>
InteropMetadataDialog::ratings () const
{
	return film()->ratings ();
}


void
InteropMetadataDialog::set_ratings (vector<dcp::Rating> r)
{
	film()->set_ratings (r);
}


void
InteropMetadataDialog::content_version_changed ()
{
	film()->set_content_versions ({ wx_to_std(_content_version->GetValue()) });
}
