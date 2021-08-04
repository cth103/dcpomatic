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
#include "language_tag_widget.h"
#include "metadata_dialog.h"
#include "wx_util.h"
#include "lib/film.h"
#include <boost/bind.hpp>
#include <boost/weak_ptr.hpp>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
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

	_sign_language_video_language->Changed.connect (boost::bind(&MetadataDialog::sign_language_video_language_changed, this));
	_edit_release_territory->Bind (wxEVT_BUTTON, boost::bind(&MetadataDialog::edit_release_territory, this));
	_enable_release_territory->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_release_territory_changed, this));
	_enable_facility->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_facility_changed, this));
	_facility->Bind (wxEVT_TEXT, boost::bind(&MetadataDialog::facility_changed, this));
	_enable_studio->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_studio_changed, this));
	_studio->Bind (wxEVT_TEXT, boost::bind(&MetadataDialog::studio_changed, this));
	_enable_chain->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_chain_changed, this));
	_chain->Bind (wxEVT_TEXT, boost::bind(&MetadataDialog::chain_changed, this));
	_temp_version->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::temp_version_changed, this));
	_pre_release->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::pre_release_changed, this));
	_red_band->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::red_band_changed, this));
	_two_d_version_of_three_d->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::two_d_version_of_three_d_changed, this));
	_enable_luminance->Bind (wxEVT_CHECKBOX, boost::bind(&MetadataDialog::enable_luminance_changed, this));
	_luminance_value->Bind (wxEVT_SPINCTRLDOUBLE, boost::bind(&MetadataDialog::luminance_changed, this));
	_luminance_unit->Bind (wxEVT_CHOICE, boost::bind(&MetadataDialog::luminance_changed, this));

	_film_changed_connection = film()->Change.connect(boost::bind(&MetadataDialog::film_changed, this, _1, _2));

	film_changed (ChangeType::DONE, Film::Property::RELEASE_TERRITORY);
	film_changed (ChangeType::DONE, Film::Property::SIGN_LANGUAGE_VIDEO_LANGUAGE);
	film_changed (ChangeType::DONE, Film::Property::FACILITY);
	film_changed (ChangeType::DONE, Film::Property::STUDIO);
	film_changed (ChangeType::DONE, Film::Property::TEMP_VERSION);
	film_changed (ChangeType::DONE, Film::Property::PRE_RELEASE);
	film_changed (ChangeType::DONE, Film::Property::RED_BAND);
	film_changed (ChangeType::DONE, Film::Property::TWO_D_VERSION_OF_THREE_D);
	film_changed (ChangeType::DONE, Film::Property::CHAIN);
	film_changed (ChangeType::DONE, Film::Property::LUMINANCE);

	setup_sensitivity ();
}


void
MetadataDialog::film_changed (ChangeType type, Film::Property property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (property == Film::Property::SIGN_LANGUAGE_VIDEO_LANGUAGE) {
		_sign_language_video_language->set (film()->sign_language_video_language());
	} else if (property == Film::Property::RELEASE_TERRITORY) {
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
	} else if (property == Film::Property::CHAIN) {
		checked_set (_enable_chain, static_cast<bool>(film()->chain()));
		if (film()->chain()) {
			checked_set (_chain, *film()->chain());
		}
	} else if (property == Film::Property::TEMP_VERSION) {
		checked_set (_temp_version, film()->temp_version());
	} else if (property == Film::Property::PRE_RELEASE) {
		checked_set (_pre_release, film()->pre_release());
	} else if (property == Film::Property::RED_BAND) {
		checked_set (_red_band, film()->red_band());
	} else if (property == Film::Property::TWO_D_VERSION_OF_THREE_D) {
		checked_set (_two_d_version_of_three_d, film()->two_d_version_of_three_d());
	} else if (property == Film::Property::LUMINANCE) {
		auto lum = film()->luminance();
		checked_set (_enable_luminance, static_cast<bool>(lum));
		if (lum) {
			checked_set (_luminance_value, lum->value());
			switch (lum->unit()) {
			case dcp::Luminance::Unit::CANDELA_PER_SQUARE_METRE:
				checked_set (_luminance_unit, 0);
				break;
			case dcp::Luminance::Unit::FOOT_LAMBERT:
				checked_set (_luminance_unit, 1);
				break;
			}
		} else {
			checked_set (_luminance_unit, 1);
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
	_sign_language_video_language->enable (film()->has_sign_language_video_channel());
	auto const enabled = _enable_release_territory->GetValue();
	_release_territory_text->Enable (enabled);
	_edit_release_territory->Enable (enabled);
	_facility->Enable (_enable_facility->GetValue());
	_chain->Enable (_enable_chain->GetValue());
	_studio->Enable (_enable_studio->GetValue());
	_luminance_value->Enable (_enable_luminance->GetValue());
	_luminance_unit->Enable (_enable_luminance->GetValue());
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
	add_label_to_sizer (sizer, panel, _("Sign language video language"), true, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT);
	_sign_language_video_language = new LanguageTagWidget (panel, _("Language used for any sign language video track"), {}, {});
	sizer->Add (_sign_language_video_language->sizer(), 1, wxEXPAND);

	_enable_facility = new wxCheckBox (panel, wxID_ANY, _("Facility"));
	sizer->Add (_enable_facility, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_facility = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_facility, 1, wxEXPAND);

	_enable_studio = new wxCheckBox (panel, wxID_ANY, _("Studio"));
	sizer->Add (_enable_studio, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_studio = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_studio, 1, wxEXPAND);

	_enable_chain = new wxCheckBox (panel, wxID_ANY, _("Chain"));
	sizer->Add (_enable_chain, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_chain = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_chain, 1, wxEXPAND);

	_temp_version = new wxCheckBox (panel, wxID_ANY, _("Temporary version"));
	sizer->Add (_temp_version, 0, wxALIGN_CENTER_VERTICAL);
	sizer->AddSpacer (0);

	_pre_release = new wxCheckBox (panel, wxID_ANY, _("Pre-release"));
	sizer->Add (_pre_release, 0, wxALIGN_CENTER_VERTICAL);
	sizer->AddSpacer (0);

	_red_band = new wxCheckBox (panel, wxID_ANY, _("Red band"));
	sizer->Add (_red_band, 0, wxALIGN_CENTER_VERTICAL);
	sizer->AddSpacer (0);

	_two_d_version_of_three_d = new wxCheckBox (panel, wxID_ANY, _("2D version of 3D DCP"));
	sizer->Add (_two_d_version_of_three_d, 0, wxALIGN_CENTER_VERTICAL);
	sizer->AddSpacer (0);

	_enable_luminance = new wxCheckBox (panel, wxID_ANY, _("Luminance"));
	sizer->Add (_enable_luminance, 0, wxALIGN_CENTER_VERTICAL);
	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		_luminance_value = new wxSpinCtrlDouble (panel, wxID_ANY);
		_luminance_value->SetDigits (1);
		_luminance_value->SetIncrement (0.1);
		s->Add (_luminance_value, 0);
		_luminance_unit = new wxChoice (panel, wxID_ANY);
		s->Add (_luminance_unit, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		sizer->Add (s, 1, wxEXPAND);
	}

	_luminance_unit->Append (wxString::FromUTF8(_("candela per m²")));
	_luminance_unit->Append (_("foot lambert"));

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


void
MetadataDialog::temp_version_changed ()
{
	film()->set_temp_version(_temp_version->GetValue());
}


void
MetadataDialog::pre_release_changed ()
{
	film()->set_pre_release(_pre_release->GetValue());
}


void
MetadataDialog::red_band_changed ()
{
	film()->set_red_band(_red_band->GetValue());
}


void
MetadataDialog::two_d_version_of_three_d_changed ()
{
	film()->set_two_d_version_of_three_d(_two_d_version_of_three_d->GetValue());
}


void
MetadataDialog::chain_changed ()
{
	film()->set_chain (wx_to_std(_chain->GetValue()));
}


void
MetadataDialog::enable_chain_changed ()
{
	setup_sensitivity ();
	if (_enable_chain->GetValue()) {
		chain_changed ();
	} else {
		film()->set_chain ();
	}
}


void
MetadataDialog::enable_luminance_changed ()
{
	setup_sensitivity ();
	if (_enable_luminance->GetValue()) {
		luminance_changed ();
	} else {
		film()->set_luminance ();
	}
}


void
MetadataDialog::luminance_changed ()
{
	dcp::Luminance::Unit unit;
	switch (_luminance_unit->GetSelection()) {
	case 0:
		unit = dcp::Luminance::Unit::CANDELA_PER_SQUARE_METRE;
		break;
	case 1:
		unit = dcp::Luminance::Unit::FOOT_LAMBERT;
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	film()->set_luminance (dcp::Luminance(_luminance_value->GetValue(), unit));
}


void
MetadataDialog::sign_language_video_language_changed ()
{
	film()->set_sign_language_video_language(_sign_language_video_language->get());
}

