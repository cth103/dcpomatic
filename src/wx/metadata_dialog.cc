/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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
#include "wx_util.h"
#include "lib/film.h"
#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>
#include <wx/notebook.h>
#include <wx/wx.h>


using std::weak_ptr;


MetadataDialog::MetadataDialog (wxWindow* parent, weak_ptr<Film> weak_film)
	: wxDialog (parent, wxID_ANY, _("Metadata"))
	, WeakFilm (weak_film)
{

}


void
MetadataDialog::setup ()
{
	auto notebook = new wxNotebook (this, wxID_ANY);

	auto prepare = [notebook](std::function<void (wxPanel*, wxSizer*)> setup, wxString name) {
		auto panel = new wxPanel (notebook, wxID_ANY);
		auto sizer = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		sizer->AddGrowableCol (1, 1);
		setup (panel, sizer);
		auto overall_sizer = new wxBoxSizer (wxVERTICAL);
		overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
		panel->SetSizer (overall_sizer);
		notebook->AddPage (panel, name);
	};

	prepare (boost::bind(&MetadataDialog::setup_standard, this, _1, _2), _("Standard"));
	prepare (boost::bind(&MetadataDialog::setup_advanced, this, _1, _2), _("Advanced"));

	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (notebook, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

