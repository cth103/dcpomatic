/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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
#include "rating_dialog.h"
#include "smpte_metadata_dialog.h"
#include "lib/film.h"
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/gbsizer.h>
#include <wx/spinctrl.h>
LIBDCP_ENABLE_WARNINGS


using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
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


void
SMPTEMetadataDialog::setup_standard (wxPanel* panel, wxSizer* sizer)
{
	MetadataDialog::setup_standard (panel, sizer);

	add_label_to_sizer (sizer, panel, _("Title language"), true, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_name_language = new LanguageTagWidget(
		panel,
		wxString::Format(_("The language that the film's title (\"%s\") is in"), std_to_wx(film()->name())),
		film()->name_language()
		);
	sizer->Add (_name_language->sizer(), 0, wxEXPAND);

	{
		int flags = wxALIGN_TOP | wxRIGHT | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
#endif
		auto m = create_label (panel, _("Ratings"), true);
		sizer->Add (m, 0, flags, DCPOMATIC_SIZER_GAP);
	}

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn("Agency", 200, true));
	columns.push_back (EditableListColumn("Label", 50, true));
	_ratings = new EditableList<dcp::Rating, RatingDialog> (
		panel,
		columns,
		boost::bind(&SMPTEMetadataDialog::ratings, this),
		boost::bind(&SMPTEMetadataDialog::set_ratings, this, _1),
		boost::bind(&ratings_column, _1, _2),
		true,
		EditableListButton::NEW | EditableListButton::EDIT | EditableListButton::REMOVE
		);
	sizer->Add (_ratings, 1, wxEXPAND);
}


void
SMPTEMetadataDialog::setup_advanced (wxPanel* panel, wxSizer* sizer)
{
	MetadataDialog::setup_advanced (panel, sizer);

	add_label_to_sizer (sizer, panel, _("Version number"), true, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_version_number = new wxSpinCtrl (panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 1000);
	sizer->Add (_version_number, 0);

	add_label_to_sizer (sizer, panel, _("Status"), true, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_status = new wxChoice (panel, wxID_ANY);
	sizer->Add (_status, 0);

	_enable_distributor = new wxCheckBox (panel, wxID_ANY, _("Distributor"));
	sizer->Add (_enable_distributor, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_distributor = new wxTextCtrl (panel, wxID_ANY);
	sizer->Add (_distributor, 1, wxEXPAND);

	{
		int flags = wxALIGN_TOP | wxRIGHT | wxTOP;
#ifdef __WXOSX__
		flags |= wxALIGN_RIGHT;
#endif
		auto m = create_label (panel, _("Content versions"), true);
		sizer->Add (m, 0, flags, DCPOMATIC_SIZER_GAP);
	}

	vector<EditableListColumn> columns;
	columns.push_back (EditableListColumn("Version", 350, true));
	_content_versions = new EditableList<string, ContentVersionDialog> (
		panel,
		columns,
		boost::bind(&SMPTEMetadataDialog::content_versions, this),
		boost::bind(&SMPTEMetadataDialog::set_content_versions, this, _1),
		boost::bind(&content_versions_column, _1, _2),
		true,
		false
		);
	sizer->Add (_content_versions, 1, wxEXPAND);
}


SMPTEMetadataDialog::SMPTEMetadataDialog (wxWindow* parent, weak_ptr<Film> weak_film)
	: MetadataDialog (parent, weak_film)
{

}


void
SMPTEMetadataDialog::setup ()
{
	MetadataDialog::setup ();

	_status->Append (_("Temporary"));
	_status->Append (_("Pre-release"));
	_status->Append (_("Final"));

	_name_language->Changed.connect (boost::bind(&SMPTEMetadataDialog::name_language_changed, this, _1));
	_version_number->Bind (wxEVT_SPINCTRL, boost::bind(&SMPTEMetadataDialog::version_number_changed, this));
	_status->Bind (wxEVT_CHOICE, boost::bind(&SMPTEMetadataDialog::status_changed, this));
	_enable_distributor->Bind (wxEVT_CHECKBOX, boost::bind(&SMPTEMetadataDialog::enable_distributor_changed, this));
	_distributor->Bind (wxEVT_TEXT, boost::bind(&SMPTEMetadataDialog::distributor_changed, this));

	film_changed (ChangeType::DONE, Film::Property::NAME_LANGUAGE);
	film_changed (ChangeType::DONE, Film::Property::VERSION_NUMBER);
	film_changed (ChangeType::DONE, Film::Property::STATUS);
	film_changed (ChangeType::DONE, Film::Property::DISTRIBUTOR);
	film_changed (ChangeType::DONE, Film::Property::CONTENT_VERSIONS);

	setup_sensitivity ();
}


void
SMPTEMetadataDialog::film_changed (ChangeType type, Film::Property property)
{
	MetadataDialog::film_changed (type, property);

	if (type != ChangeType::DONE || film()->interop()) {
		return;
	}

	if (property == Film::Property::NAME_LANGUAGE) {
		_name_language->set (film()->name_language());
	} else if (property == Film::Property::VERSION_NUMBER) {
		checked_set (_version_number, film()->version_number());
	} else if (property == Film::Property::STATUS) {
		switch (film()->status()) {
		case dcp::Status::TEMP:
			checked_set (_status, 0);
			break;
		case dcp::Status::PRE:
			checked_set (_status, 1);
			break;
		case dcp::Status::FINAL:
			checked_set (_status, 2);
			break;
		}
	} else if (property == Film::Property::DISTRIBUTOR) {
		checked_set (_enable_distributor, static_cast<bool>(film()->distributor()));
		if (film()->distributor()) {
			checked_set (_distributor, *film()->distributor());
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
SMPTEMetadataDialog::version_number_changed ()
{
	film()->set_version_number (_version_number->GetValue());
}


void
SMPTEMetadataDialog::status_changed ()
{
	switch (_status->GetSelection()) {
	case 0:
		film()->set_status(dcp::Status::TEMP);
		break;
	case 1:
		film()->set_status(dcp::Status::PRE);
		break;
	case 2:
		film()->set_status(dcp::Status::FINAL);
		break;
	}
}


void
SMPTEMetadataDialog::distributor_changed ()
{
	film()->set_distributor (wx_to_std(_distributor->GetValue()));
}


void
SMPTEMetadataDialog::setup_sensitivity ()
{
	MetadataDialog::setup_sensitivity ();

	_distributor->Enable (_enable_distributor->GetValue());
}


void
SMPTEMetadataDialog::enable_distributor_changed ()
{
	setup_sensitivity ();
	if (_enable_distributor->GetValue()) {
		film()->set_distributor (wx_to_std(_distributor->GetValue()));
	} else {
		film()->set_distributor ();
	}
}


