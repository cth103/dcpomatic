/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "kdm_dialog.h"
#include "cinema_dialog.h"
#include "screen_dialog.h"
#include "wx_util.h"
#include "screens_panel.h"
#include "kdm_timing_panel.h"
#include "kdm_output_panel.h"
#include "lib/cinema.h"
#include "lib/config.h"
#include "lib/film.h"
#include "lib/screen.h"
#include <libcxml/cxml.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <iostream>

using std::string;
using std::map;
using std::list;
using std::pair;
using std::cout;
using std::vector;
using std::make_pair;
using boost::shared_ptr;

KDMDialog::KDMDialog (wxWindow* parent, boost::shared_ptr<const Film> film)
	: wxDialog (parent, wxID_ANY, _("Make KDMs"))
{
	/* Main sizer */
	wxBoxSizer* vertical = new wxBoxSizer (wxVERTICAL);

	/* Font for sub-headings */
	wxFont subheading_font (*wxNORMAL_FONT);
	subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

	/* Sub-heading: Screens */
	wxStaticText* h = new wxStaticText (this, wxID_ANY, _("Screens"));
	h->SetFont (subheading_font);
	vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL);
	_screens = new ScreensPanel (this);
	vertical->Add (_screens, 1, wxEXPAND);

	/* Sub-heading: Timing */
	h = new wxStaticText (this, wxID_ANY, S_("KDM|Timing"));
	h->SetFont (subheading_font);
	vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
	_timing = new KDMTimingPanel (this);
	vertical->Add (_timing);

	/* Sub-heading: CPL */
	h = new wxStaticText (this, wxID_ANY, _("CPL"));
	h->SetFont (subheading_font);
	vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);

	/* CPL choice */
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (s, this, _("CPL"), true);
	_cpl = new wxChoice (this, wxID_ANY);
	s->Add (_cpl, 1, wxEXPAND);
	_cpl_browse = new wxButton (this, wxID_ANY, _("Browse..."));
	s->Add (_cpl_browse, 0);
	vertical->Add (s, 0, wxEXPAND | wxTOP, DCPOMATIC_SIZER_GAP + 2);

	/* CPL details */
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	add_label_to_sizer (table, this, _("DCP directory"), true);
	_dcp_directory = new wxStaticText (this, wxID_ANY, "");
	table->Add (_dcp_directory);
	add_label_to_sizer (table, this, _("CPL ID"), true);
	_cpl_id = new wxStaticText (this, wxID_ANY, "");
	table->Add (_cpl_id);
	add_label_to_sizer (table, this, _("CPL annotation text"), true);
	_cpl_annotation_text = new wxStaticText (this, wxID_ANY, "");
	table->Add (_cpl_annotation_text);
	vertical->Add (table, 0, wxEXPAND | wxTOP, DCPOMATIC_SIZER_GAP + 2);

	_cpls = film->cpls ();
	update_cpl_choice ();

	/* Sub-heading: Output */
	h = new wxStaticText (this, wxID_ANY, _("Output"));
	h->SetFont (subheading_font);
	vertical->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
	_output = new KDMOutputPanel (this, film->interop ());
	vertical->Add (_output, 0, wxEXPAND | wxTOP, DCPOMATIC_SIZER_GAP);

	/* Make an overall sizer to get a nice border, and put some buttons in */

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (vertical, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_Y_GAP);
	}

	/* Bind */

	_screens->ScreensChanged.connect (boost::bind (&KDMDialog::setup_sensitivity, this));

	_cpl->Bind           (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&KDMDialog::update_cpl_summary, this));
	_cpl_browse->Bind    (wxEVT_COMMAND_BUTTON_CLICKED,  boost::bind (&KDMDialog::cpl_browse_clicked, this));

	setup_sensitivity ();

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

void
KDMDialog::setup_sensitivity ()
{
	_screens->setup_sensitivity ();
	_output->setup_sensitivity ();

	bool const sd = _cpl->GetSelection() != -1;

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (!_screens->screens().empty() && sd);
	}
}

boost::filesystem::path
KDMDialog::cpl () const
{
	int const item = _cpl->GetSelection ();
	DCPOMATIC_ASSERT (item >= 0);
	return _cpls[item].cpl_file;
}

void
KDMDialog::update_cpl_choice ()
{
	_cpl->Clear ();

	for (vector<CPLSummary>::const_iterator i = _cpls.begin(); i != _cpls.end(); ++i) {
		_cpl->Append (std_to_wx (i->cpl_id));

		if (_cpls.size() > 0) {
			_cpl->SetSelection (0);
		}
	}

	update_cpl_summary ();
}

void
KDMDialog::update_cpl_summary ()
{
	int const n = _cpl->GetSelection();
	if (n == wxNOT_FOUND) {
		return;
	}

	_dcp_directory->SetLabel (std_to_wx (_cpls[n].dcp_directory));
	_cpl_id->SetLabel (std_to_wx (_cpls[n].cpl_id));
	_cpl_annotation_text->SetLabel (std_to_wx (_cpls[n].cpl_annotation_text));
}

void
KDMDialog::cpl_browse_clicked ()
{
	wxFileDialog* d = new wxFileDialog (this, _("Select CPL XML file"), wxEmptyString, wxEmptyString, "*.xml");
	if (d->ShowModal() == wxID_CANCEL) {
		d->Destroy ();
		return;
	}

	boost::filesystem::path cpl_file (wx_to_std (d->GetPath ()));
	boost::filesystem::path dcp_dir = cpl_file.parent_path ();

	d->Destroy ();

	/* XXX: hack alert */
	cxml::Document cpl_document ("CompositionPlaylist");
	cpl_document.read_file (cpl_file);

	try {
		_cpls.push_back (
			CPLSummary (
				dcp_dir.filename().string(),
				cpl_document.string_child("Id").substr (9),
				cpl_document.string_child ("ContentTitleText"),
				cpl_file
				)
			);
	} catch (cxml::Error) {
		error_dialog (this, _("This is not a valid CPL file"));
		return;
	}

	update_cpl_choice ();
	_cpl->SetSelection (_cpls.size() - 1);
	update_cpl_summary ();
}

list<shared_ptr<Screen> >
KDMDialog::screens () const
{
	return _screens->screens ();
}

boost::posix_time::ptime
KDMDialog::from () const
{
	return _timing->from ();
}

boost::posix_time::ptime
KDMDialog::until () const
{
	return _timing->until ();
}

boost::filesystem::path
KDMDialog::directory () const
{
	return _output->directory ();
}

bool
KDMDialog::write_to () const
{
	return _output->write_to ();
}

dcp::Formulation
KDMDialog::formulation () const
{
	return _output->formulation ();
}
