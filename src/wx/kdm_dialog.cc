/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "kdm_dialog.h"
#include "wx_util.h"
#include "screens_panel.h"
#include "kdm_timing_panel.h"
#include "kdm_output_panel.h"
#include "kdm_cpl_panel.h"
#include "confirm_kdm_email_dialog.h"
#include "lib/film.h"
#include "lib/screen.h"
#include "lib/screen_kdm.h"
#include "lib/send_kdm_email_job.h"
#include "lib/job_manager.h"
#include "lib/cinema_kdms.h"
#include "lib/config.h"
#include "lib/cinema.h"
#include <libcxml/cxml.h>
#include <dcp/exceptions.h>
#include <wx/treectrl.h>
#include <wx/listctrl.h>
#include <iostream>

using std::string;
using std::exception;
using std::map;
using std::list;
using std::pair;
using std::cout;
using std::vector;
using std::make_pair;
using boost::shared_ptr;
using boost::bind;

KDMDialog::KDMDialog (wxWindow* parent, shared_ptr<const Film> film)
	: wxDialog (parent, wxID_ANY, _("Make KDMs"))
	, _film (film)
{
	/* Main sizers */
	wxBoxSizer* horizontal = new wxBoxSizer (wxHORIZONTAL);
	wxBoxSizer* left = new wxBoxSizer (wxVERTICAL);
	wxBoxSizer* right = new wxBoxSizer (wxVERTICAL);

	horizontal->Add (left, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP * 4);
	horizontal->Add (right, 1, wxEXPAND);

	/* Font for sub-headings */
	wxFont subheading_font (*wxNORMAL_FONT);
	subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

	/* Sub-heading: Screens */
	wxStaticText* h = new wxStaticText (this, wxID_ANY, _("Screens"));
	h->SetFont (subheading_font);
	left->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);
	_screens = new ScreensPanel (this);
	left->Add (_screens, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_SIZER_Y_GAP);

	/* Sub-heading: Timing */
	/// TRANSLATORS: translate the word "Timing" here; do not include the "KDM|" prefix
	h = new wxStaticText (this, wxID_ANY, S_("KDM|Timing"));
	h->SetFont (subheading_font);
	right->Add (h, 0, wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_Y_GAP * 2);
	_timing = new KDMTimingPanel (this);
	right->Add (_timing);

	/* Sub-heading: CPL */
	h = new wxStaticText (this, wxID_ANY, _("CPL"));
	h->SetFont (subheading_font);
	right->Add (h, 0, wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_Y_GAP * 2);
	_cpl = new KDMCPLPanel (this, film->cpls ());
	right->Add (_cpl, 0, wxEXPAND);

	/* Sub-heading: Output */
	h = new wxStaticText (this, wxID_ANY, _("Output"));
	h->SetFont (subheading_font);
	right->Add (h, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_SIZER_Y_GAP * 2);
	_output = new KDMOutputPanel (this, film->interop ());
	right->Add (_output, 0, wxEXPAND | wxTOP, DCPOMATIC_SIZER_GAP);

	_make = new wxButton (this, wxID_ANY, _("Make KDMs"));
	right->Add (_make, 0, wxTOP | wxBOTTOM, DCPOMATIC_SIZER_GAP);

	/* Make an overall sizer to get a nice border */

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (horizontal, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, DCPOMATIC_DIALOG_BORDER);

	/* Bind */

	_screens->ScreensChanged.connect (boost::bind (&KDMDialog::setup_sensitivity, this));
	_timing->TimingChanged.connect (boost::bind (&KDMDialog::setup_sensitivity, this));
	_make->Bind (wxEVT_BUTTON, boost::bind (&KDMDialog::make_clicked, this));

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
	_make->Enable (!_screens->screens().empty() && _timing->valid() && _cpl->has_selected());
}

bool
KDMDialog::confirm_overwrite (boost::filesystem::path path)
{
	return confirm_dialog (
		this,
		wxString::Format (_("File %s already exists.  Do you want to overwrite it?"), std_to_wx(path.string()).data())
		);
}

void
KDMDialog::make_clicked ()
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	list<ScreenKDM> screen_kdms = film->make_kdms (
		_screens->screens(), _cpl->cpl(), _timing->from(), _timing->until(), _output->formulation(),
		!_output->forensic_mark_video(), _output->forensic_mark_audio() ? boost::optional<int>() : 0
		);

	pair<shared_ptr<Job>, int> result = _output->make (screen_kdms, film->name(), _timing, bind (&KDMDialog::confirm_overwrite, this, _1), film->log());
	if (result.first) {
		JobManager::instance()->add (result.first);
	}

	if (result.second > 0) {
		/* XXX: proper plural form support in wxWidgets? */
		wxString s = result.second == 1 ? _("%d KDM written to %s") : _("%d KDMs written to %s");
		message_dialog (
			this,
			wxString::Format (s, result.second, std_to_wx(_output->directory().string()).data())
			);
	}
}
