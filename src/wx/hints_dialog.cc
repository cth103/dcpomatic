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
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

HintsDialog::HintsDialog (wxWindow* parent, boost::weak_ptr<Film> film, bool ok)
	: wxDialog (parent, wxID_ANY, _("Hints"))
	, _film (film)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
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

	film_changed ();
}

void
HintsDialog::film_changed ()
{
	_text->Clear ();

	boost::shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	vector<string> hints = get_hints (film);

	if (hints.empty ()) {
		_text->WriteText (_("There are no hints: everything looks good!"));
	} else {
		_text->BeginStandardBullet (N_("standard/circle"), 1, 50);
		BOOST_FOREACH (string i, hints) {
			_text->WriteText (std_to_wx (i));
			_text->Newline ();
		}
		_text->EndSymbolBullet ();
	}
}

void
HintsDialog::shut_up (wxCommandEvent& ev)
{
	Config::instance()->set_show_hints_before_make_dcp (!ev.IsChecked());
}
