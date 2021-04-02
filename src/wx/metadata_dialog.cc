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


#include "dcpomatic_button.h"
#include "full_language_tag_dialog.h"
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

	_enable_facility->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_facility_changed, this));
	_facility->Bind (wxEVT_TEXT, boost::bind(&MetadataDialog::facility_changed, this));
	_enable_studio->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_studio_changed, this));
	_studio->Bind (wxEVT_TEXT, boost::bind(&MetadataDialog::studio_changed, this));

	_film_changed_connection = film()->Change.connect(boost::bind(&MetadataDialog::film_changed, this, _1, _2));

	film_changed (ChangeType::DONE, Film::Property::RELEASE_TERRITORY);
	film_changed (ChangeType::DONE, Film::Property::FACILITY);
	film_changed (ChangeType::DONE, Film::Property::STUDIO);

	setup_sensitivity ();
}


void
MetadataDialog::film_changed (ChangeType type, Film::Property property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (property == Film::Property::RELEASE_TERRITORY) {
		auto rt = film()->release_territory();
		checked_set (_enable_release_territory, static_cast<bool>(rt));
		if (rt) {
			_release_territory = *rt;
			checked_set (_release_territory_text, std_to_wx(*dcp::LanguageTag::get_subtag_description(*_release_territory)));
		}
	} else if (property == Film::Property::FACILITY) {
		checked_set (_enable_facility, static_cast<bool>(film()->facility()));
		if (film()->facility()) {
			checked_set (_facility, *film()->facility());
		}
	} else if (property == Film::Property::STUDIO) {
		checked_set (_enable_studio, static_cast<bool>(film()->studio()));
		if (film()->studio()) {
			checked_set (_studio, *film()->studio());
		}
	}
}


void
MetadataDialog::setup_standard (wxPanel* panel, wxSizer* sizer)
{
	_enable_release_territory = new wxCheckBox (panel, wxID_ANY, _("Release territory"));
	sizer->Add (_enable_release_territory, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		_release_territory_text = new wxStaticText (panel, wxID_ANY, wxT(""));
		s->Add (_release_territory_text, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
		_edit_release_territory = new Button (panel, _("Edit..."));
		s->Add (_edit_release_territory, 0, wxLEFT, DCPOMATIC_SIZER_GAP);
		sizer->Add (s, 0, wxEXPAND);
	}

	_edit_release_territory->Bind (wxEVT_BUTTON, boost::bind(&MetadataDialog::edit_release_territory, this));
	_enable_release_territory->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_release_territory_changed, this));
}


void
MetadataDialog::edit_release_territory ()
{
	DCPOMATIC_ASSERT (film()->release_territory());
	auto d = new RegionSubtagDialog(this, *film()->release_territory());
	d->ShowModal ();
	auto tag = d->get();
	if (tag) {
		_release_territory = *tag;
		film()->set_release_territory(*tag);
	}
	d->Destroy ();
}


void
MetadataDialog::setup_sensitivity ()
{
	auto const enabled = _enable_release_territory->GetValue();
	_release_territory_text->Enable (enabled);
	_edit_release_territory->Enable (enabled);
	_facility->Enable (_enable_facility->GetValue());
	_studio->Enable (_enable_studio->GetValue());
}


void
MetadataDialog::enable_release_territory_changed ()
{
	setup_sensitivity ();
	if (_enable_release_territory->GetValue()) {
		film()->set_release_territory (_release_territory.get_value_or(dcp::LanguageTag::RegionSubtag("US")));
	} else {
		film()->set_release_territory ();
	}
}


void
MetadataDialog::setup_advanced (wxPanel* panel, wxSizer* sizer)
{
	_enable_facility = new wxCheckBox (panel, wxID_ANY, _("Facility"));
	sizer->Add (_enable_facility, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_facility = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_facility, 1, wxEXPAND);

	_enable_studio = new wxCheckBox (panel, wxID_ANY, _("Studio"));
	sizer->Add (_enable_studio, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_studio = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_studio, 1, wxEXPAND);
}


void
MetadataDialog::facility_changed ()
{
	film()->set_facility (wx_to_std(_facility->GetValue()));
}


void
MetadataDialog::enable_facility_changed ()
{
	setup_sensitivity ();
	if (_enable_facility->GetValue()) {
		film()->set_facility (wx_to_std(_facility->GetValue()));
	} else {
		film()->set_facility ();
	}
}


void
MetadataDialog::studio_changed ()
{
	film()->set_studio (wx_to_std(_studio->GetValue()));
}


void
MetadataDialog::enable_studio_changed ()
{
	setup_sensitivity ();
	if (_enable_studio->GetValue()) {
		film()->set_studio (wx_to_std(_studio->GetValue()));
	} else {
		film()->set_studio ();
	}
}


