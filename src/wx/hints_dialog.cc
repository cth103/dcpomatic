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
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/config.h"
#include <wx/richtext/richtextctrl.h>
#include <boost/foreach.hpp>

using std::max;
using std::vector;
using std::string;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::bind;
using boost::dynamic_pointer_cast;

HintsDialog::HintsDialog (wxWindow* parent, boost::weak_ptr<Film> film, bool ok)
	: wxDialog (parent, wxID_ANY, _("Hints"))
	, _film (film)
	, _hints (new Hints (film))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	_gauge = new wxGauge (this, wxID_ANY, 100);
	sizer->Add (_gauge, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);
	_gauge_message = new wxStaticText (this, wxID_ANY, wxT(""));
	sizer->Add (_gauge_message, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_text = new wxRichTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (400, 300), wxRE_READONLY);
	sizer->Add (_text, 1, wxEXPAND | wxALL, 6);

	if (!ok) {
		wxCheckBox* b = new wxCheckBox (this, wxID_ANY, _("Don't show hints again"));
		sizer->Add (b, 0, wxALL, 6);
		b->Bind (wxEVT_CHECKBOX, bind (&HintsDialog::shut_up, this, _1));
	}

	wxStdDialogButtonSizer* buttons = CreateStdDialogButtonSizer (0);
	sizer->Add (CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	if (ok) {
		buttons->SetAffirmativeButton (new wxButton (this, wxID_OK));
	} else {
		buttons->SetAffirmativeButton (new wxButton (this, wxID_OK, _("Make DCP anyway")));
		buttons->SetNegativeButton (new wxButton (this, wxID_CANCEL, _("Go back")));
	}

	buttons->Realize ();

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_text->GetCaret()->Hide ();

	boost::shared_ptr<Film> locked_film = _film.lock ();
	if (locked_film) {
		_film_changed_connection = locked_film->Changed.connect (boost::bind (&HintsDialog::film_changed, this));
		_film_content_changed_connection = locked_film->ContentChanged.connect (boost::bind (&HintsDialog::film_changed, this));
	}

	_hints->Hint.connect (bind (&HintsDialog::hint, this, _1));
	_hints->Progress.connect (bind (&HintsDialog::progress, this, _1));
	_hints->Pulse.connect (bind (&HintsDialog::pulse, this));
	_hints->Finished.connect (bind (&HintsDialog::finished, this));

	film_changed ();
}

void
HintsDialog::film_changed ()
{
	_text->Clear ();
	_current.clear ();

	boost::shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	_gauge->Show ();
	_gauge_message->Show ();
	Layout ();
	_gauge->SetValue (0);
	update ();
	_hints->start ();
}

void
HintsDialog::update ()
{
	_text->Clear ();
	if (_current.empty ()) {
		_text->WriteText (_("There are no hints: everything looks good!"));
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
	_gauge->Hide ();
	_gauge_message->Hide ();
	Layout ();
}

void
HintsDialog::progress (string m)
{
	_gauge_message->SetLabel (std_to_wx(m));
}
