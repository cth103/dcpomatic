/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "hints_dialog.h"
#include "wx_util.h"
#include "static_text.h"
#include "check_box.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/config.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/richtext/richtextctrl.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/foreach.hpp>

using std::max;
using std::vector;
using std::string;
using std::cout;
using std::shared_ptr;
using boost::optional;
using boost::bind;
using std::dynamic_pointer_cast;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

HintsDialog::HintsDialog (wxWindow* parent, std::weak_ptr<Film> film, bool ok)
	: wxDialog (parent, wxID_ANY, _("Hints"))
	, _film (film)
	, _hints (0)
	, _finished (false)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	_gauge = new wxGauge (this, wxID_ANY, 100);
	sizer->Add (_gauge, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);
	_gauge_message = new StaticText (this, wxT(""));
	sizer->Add (_gauge_message, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_text = new wxRichTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (400, 300), wxRE_READONLY);
	sizer->Add (_text, 1, wxEXPAND | wxALL, 6);

	if (!ok) {
		wxCheckBox* b = new CheckBox (this, _("Don't show hints again"));
		sizer->Add (b, 0, wxALL, 6);
		b->Bind (wxEVT_CHECKBOX, bind (&HintsDialog::shut_up, this, _1));
	}

	wxStdDialogButtonSizer* buttons = CreateStdDialogButtonSizer (0);
	sizer->Add (CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	if (ok) {
		buttons->SetAffirmativeButton (new wxButton (this, wxID_OK));
	} else {
		buttons->SetAffirmativeButton (new wxButton (this, wxID_OK, _("Make DCP")));
		buttons->SetNegativeButton (new wxButton (this, wxID_CANCEL, _("Go back")));
	}

	buttons->Realize ();

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_text->GetCaret()->Hide ();

	std::shared_ptr<Film> locked_film = _film.lock ();
	if (locked_film) {
		_film_change_connection = locked_film->Change.connect (boost::bind (&HintsDialog::film_change, this, _1));
		_film_content_change_connection = locked_film->ContentChange.connect (boost::bind (&HintsDialog::film_content_change, this, _1));
	}

	film_change (CHANGE_TYPE_DONE);
}

void
HintsDialog::film_change (ChangeType type)
{
	if (type != CHANGE_TYPE_DONE) {
		return;
	}

	_text->Clear ();
	_current.clear ();

	std::shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	_gauge->Show ();
	_gauge_message->Show ();
	Layout ();
	_gauge->SetValue (0);
	update ();
	_finished = false;

	_hints.reset (new Hints (_film));
	_hints->Hint.connect (bind (&HintsDialog::hint, this, _1));
	_hints->Progress.connect (bind (&HintsDialog::progress, this, _1));
	_hints->Pulse.connect (bind (&HintsDialog::pulse, this));
	_hints->Finished.connect (bind (&HintsDialog::finished, this));
	_hints->start ();
}

void
HintsDialog::film_content_change (ChangeType type)
{
	film_change (type);
}

void
HintsDialog::update ()
{
	_text->Clear ();
	if (_current.empty ()) {
		if (_finished) {
			_text->WriteText (_("There are no hints: everything looks good!"));
		} else {
			_text->WriteText (_("There are no hints yet: project check in progress."));
		}
	} else {
		_text->BeginStandardBullet (N_("standard/circle"), 1, 50);
		BOOST_FOREACH (string i, _current) {
			_text->WriteText (std_to_wx (i));
			_text->Newline ();
		}
		_text->EndSymbolBullet ();
	}
}

void
HintsDialog::hint (string text)
{
	_current.push_back (text);
	update ();
}

void
HintsDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_show_hints_before_make_dcp (!ev.IsChecked());
}

void
HintsDialog::pulse ()
{
	_gauge->Pulse ();
}

void
HintsDialog::finished ()
{
	try {
		_hints->rethrow ();
	} catch (std::exception& e) {
		error_dialog (this, wxString::Format(_("A problem occurred when looking for hints (%s)"), std_to_wx(e.what())));
	}

	_finished = true;
	update ();
	_gauge->Hide ();
	_gauge_message->Hide ();
	Layout ();
}

void
HintsDialog::progress (string m)
{
	_gauge_message->SetLabel (std_to_wx(m));
}
