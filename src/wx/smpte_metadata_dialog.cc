/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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

#include "content_version_dialog.h"
#include "editable_list.h"
#include "language_tag_dialog.h"
#include "language_tag_widget.h"
#include "smpte_metadata_dialog.h"
#include "rating_dialog.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <wx/gbsizer.h>
#include <wx/spinctrl.h>

using std::string;
using std::vector;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static string
ratings_column (dcp::Rating r, int c)
{
	if (c == 0) {
		return r.agency;
	}

	return r.label;
}


static string
content_versions_column (string v, int)
{
	return v;
}


SMPTEMetadataDialog::SMPTEMetadataDialog (wxWindow* parent, weak_ptr<Film> weak_film)
	: wxDialog (parent, wxID_ANY, _("Metadata"))
	, _film (weak_film)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxFlexGridSizer* sizer = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	sizer->AddGrowableCol (1, 1);

	_name_language = new LanguageTagWidget(
		this,
		sizer,
		_("Title language"),
		wxString::Format(_("The language that the film's title (\"%s\") is in"), std_to_wx(film()->name())),
		film()->name_language()
		);

	_audio_language = new LanguageTagWidget(
		this,
		sizer,
		_("Audio language"),
		_("The main language that is spoken in the film's soundtrack"),
		film()->audio_language()
		);

	Button* edit_release_territory = 0;
	add_label_to_sizer (sizer, this, _("Release territory"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	{
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_release_territory = new wxStaticText (this, wxID_ANY, wxT(""));
		s->Add (_release_territory, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
		edit_release_territory = new Button (this, _("Edit..."));
		s->Add (edit_release_territory, 0, wxLEFT, DCPOMATIC_SIZER_GAP);
		sizer->Add (s, 0, wxEXPAND);
	}

	add_label_to_sizer (sizer, this, _("Version number"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_version_number = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 1000);
	sizer->Add (_version_number, 0);

	add_label_to_sizer (sizer, this, _("Status"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_status = new wxChoice (this, wxID_ANY);
	sizer->Add (_status, 0);

	add_label_to_sizer (sizer, this, _("Chain"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_chain = new wxTextCtrl (this, wxID_ANY);
	sizer->Add (_chain, 1, wxEXPAND);

	add_label_to_sizer (sizer, this, _("Distributor"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_distributor = new wxTextCtrl (this, wxID_ANY);
	sizer->Add (_distributor, 1, wxEXPAND);

	add_label_to_sizer (sizer, this, _("Facility"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_facility = new wxTextCtrl (this, wxID_ANY);
	sizer->Add (_facility, 1, wxEXPAND);

	add_label_to_sizer (sizer, this, _("Luminance"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	{
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_luminance_value = new wxSpinCtrlDouble (this, wxID_ANY);
		_luminance_value->SetDigits (1);
		_luminance_value->SetIncrement (0.1);
		s->Add (_luminance_value, 0);
		_luminance_unit = new wxChoice (this, wxID_ANY);
		s->Add (_luminance_unit, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		sizer->Add (s, 1, wxEXPAND);
	}

	{
		int flags = wxALIGN_TOP | wxLEFT | wxRIGHT | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
#endif
		wxStaticText* m = create_label (this, _("Ratings"), true);
		sizer->Add (m, 0, flags, DCPOMATIC_SIZER_GAP);
	}

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn("Agency", 200, true));
	columns.push_back (EditableListColumn("Label", 50, true));
	_ratings = new EditableList<dcp::Rating, RatingDialog> (
		this,
		columns,
		boost::bind(&SMPTEMetadataDialog::ratings, this),
		boost::bind(&SMPTEMetadataDialog::set_ratings, this, _1),
		boost::bind(&ratings_column, _1, _2),
		true,
		false
		);
	sizer->Add (_ratings, 1, wxEXPAND);

	{
		int flags = wxALIGN_TOP | wxLEFT | wxRIGHT | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
#endif
		wxStaticText* m = create_label (this, _("Content versions"), true);
		sizer->Add (m, 0, flags, DCPOMATIC_SIZER_GAP);
	}

	columns.clear ();
	columns.push_back (EditableListColumn("Version", 350, true));
	_content_versions = new EditableList<string, ContentVersionDialog> (
		this,
		columns,
		boost::bind(&SMPTEMetadataDialog::content_versions, this),
		boost::bind(&SMPTEMetadataDialog::set_content_versions, this, _1),
		boost::bind(&content_versions_column, _1, _2),
		true,
		false
		);
	sizer->Add (_content_versions, 1, wxEXPAND);

	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_status->Append (_("Temporary"));
	_status->Append (_("Pre-release"));
	_status->Append (_("Final"));

	_luminance_unit->Append (_("candela per m²"));
	_luminance_unit->Append (_("foot lambert"));

	_name_language->Changed.connect (boost::bind(&SMPTEMetadataDialog::name_language_changed, this, _1));
	_audio_language->Changed.connect (boost::bind(&SMPTEMetadataDialog::audio_language_changed, this, _1));
	edit_release_territory->Bind (wxEVT_BUTTON, boost::bind(&SMPTEMetadataDialog::edit_release_territory, this));
	_version_number->Bind (wxEVT_SPINCTRL, boost::bind(&SMPTEMetadataDialog::version_number_changed, this));
	_status->Bind (wxEVT_CHOICE, boost::bind(&SMPTEMetadataDialog::status_changed, this));
	_chain->Bind (wxEVT_TEXT, boost::bind(&SMPTEMetadataDialog::chain_changed, this));
	_distributor->Bind (wxEVT_TEXT, boost::bind(&SMPTEMetadataDialog::distributor_changed, this));
	_facility->Bind (wxEVT_TEXT, boost::bind(&SMPTEMetadataDialog::facility_changed, this));
	_luminance_value->Bind (wxEVT_SPINCTRLDOUBLE, boost::bind(&SMPTEMetadataDialog::luminance_changed, this));
	_luminance_unit->Bind (wxEVT_CHOICE, boost::bind(&SMPTEMetadataDialog::luminance_changed, this));

	_version_number->SetFocus ();

	_film_changed_connection = film()->Change.connect(boost::bind(&SMPTEMetadataDialog::film_changed, this, _1, _2));

	film_changed (CHANGE_TYPE_DONE, Film::NAME_LANGUAGE);
	film_changed (CHANGE_TYPE_DONE, Film::RELEASE_TERRITORY);
	film_changed (CHANGE_TYPE_DONE, Film::VERSION_NUMBER);
	film_changed (CHANGE_TYPE_DONE, Film::STATUS);
	film_changed (CHANGE_TYPE_DONE, Film::CHAIN);
	film_changed (CHANGE_TYPE_DONE, Film::DISTRIBUTOR);
	film_changed (CHANGE_TYPE_DONE, Film::FACILITY);
	film_changed (CHANGE_TYPE_DONE, Film::CONTENT_VERSIONS);
	film_changed (CHANGE_TYPE_DONE, Film::LUMINANCE);
}


void
SMPTEMetadataDialog::film_changed (ChangeType type, Film::Property property)
{
	if (type != CHANGE_TYPE_DONE || film()->interop()) {
		return;
	}

	if (property == Film::NAME_LANGUAGE) {
		_name_language->set (film()->name_language());
	} else if (property == Film::RELEASE_TERRITORY) {
		checked_set (_release_territory, std_to_wx(*dcp::LanguageTag::get_subtag_description(dcp::LanguageTag::REGION, film()->release_territory().subtag())));
	} else if (property == Film::VERSION_NUMBER) {
		checked_set (_version_number, film()->version_number());
	} else if (property == Film::STATUS) {
		switch (film()->status()) {
		case dcp::TEMP:
			checked_set (_status, 0);
			break;
		case dcp::PRE:
			checked_set (_status, 1);
			break;
		case dcp::FINAL:
			checked_set (_status, 2);
			break;
		}
	} else if (property == Film::CHAIN) {
		checked_set (_chain, film()->chain());
	} else if (property == Film::DISTRIBUTOR) {
		checked_set (_distributor, film()->distributor());
	} else if (property == Film::FACILITY) {
		checked_set (_facility, film()->facility());
	} else if (property == Film::LUMINANCE) {
		checked_set (_luminance_value, film()->luminance().value());
		switch (film()->luminance().unit()) {
		case dcp::Luminance::CANDELA_PER_SQUARE_METRE:
			checked_set (_luminance_unit, 0);
			break;
		case dcp::Luminance::FOOT_LAMBERT:
			checked_set (_luminance_unit, 1);
			break;
		}
	}
}


vector<dcp::Rating>
SMPTEMetadataDialog::ratings () const
{
	return film()->ratings ();
}


void
SMPTEMetadataDialog::set_ratings (vector<dcp::Rating> r)
{
	film()->set_ratings (r);
}


vector<string>
SMPTEMetadataDialog::content_versions () const
{
	return film()->content_versions ();
}


void
SMPTEMetadataDialog::set_content_versions (vector<string> cv)
{
	film()->set_content_versions (cv);
}


void
SMPTEMetadataDialog::name_language_changed (dcp::LanguageTag tag)
{
	film()->set_name_language (tag);
}


void
SMPTEMetadataDialog::audio_language_changed (dcp::LanguageTag tag)
{
	film()->set_audio_language (tag);
}


void
SMPTEMetadataDialog::edit_release_territory ()
{
	RegionSubtagDialog* d = new RegionSubtagDialog(this, film()->release_territory());
	d->ShowModal ();
	optional<dcp::LanguageTag::RegionSubtag> tag = d->get();
	if (tag) {
		film()->set_release_territory (*tag);
	}
	d->Destroy ();
}


shared_ptr<Film>
SMPTEMetadataDialog::film () const
{
	shared_ptr<Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return film;
}


void
SMPTEMetadataDialog::version_number_changed ()
{
	film()->set_version_number (_version_number->GetValue());
}


void
SMPTEMetadataDialog::status_changed ()
{
	switch (_status->GetSelection()) {
	case 0:
		film()->set_status (dcp::TEMP);
		break;
	case 1:
		film()->set_status (dcp::PRE);
		break;
	case 2:
		film()->set_status (dcp::FINAL);
		break;
	}
}


void
SMPTEMetadataDialog::chain_changed ()
{
	film()->set_chain (wx_to_std(_chain->GetValue()));
}


void
SMPTEMetadataDialog::distributor_changed ()
{
	film()->set_distributor (wx_to_std(_distributor->GetValue()));
}


void
SMPTEMetadataDialog::facility_changed ()
{
	film()->set_facility (wx_to_std(_facility->GetValue()));
}


void
SMPTEMetadataDialog::luminance_changed ()
{
	dcp::Luminance::Unit unit;
	switch (_luminance_unit->GetSelection()) {
	case 0:
		unit = dcp::Luminance::CANDELA_PER_SQUARE_METRE;
		break;
	case 1:
		unit = dcp::Luminance::FOOT_LAMBERT;
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	film()->set_luminance (dcp::Luminance(_luminance_value->GetValue(), unit));
}
