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


#include "interop_metadata_dialog.h"
#include "language_tag_widget.h"
#include "rating_dialog.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/gbsizer.h>
LIBDCP_ENABLE_WARNINGS


using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
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

	sizer->Add (_ratings, 1, wxEXPAND);

	add_label_to_sizer (sizer, panel, _("Content version"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_content_version = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_content_version, 1, wxEXPAND);

	auto cv = film()->content_versions();
	_content_version->SetValue (std_to_wx(cv.empty() ? "" : cv[0]));

	_content_version->Bind (wxEVT_TEXT, boost::bind(&InteropMetadataDialog::content_version_changed, this));
	_content_version->SetFocus ();
}


void
InteropMetadataDialog::content_version_changed ()
{
	auto version = wx_to_std(_content_version->GetValue());
	if (version.empty()) {
		film()->set_content_versions({});
	} else {
		film()->set_content_versions({version});
	}
}
