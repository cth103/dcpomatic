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
#include "kdm_cpl_panel.h"
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
	_cpl = new KDMCPLPanel (this, film->cpls ());
	vertical->Add (_cpl);

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

	bool const sd = _cpl->has_selected ();

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	if (ok) {
		ok->Enable (!_screens->screens().empty() && sd);
	}
}

boost::filesystem::path
KDMDialog::cpl () const
{
	return _cpl->cpl ();
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
