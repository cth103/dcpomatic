/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "instant_i18n_dialog.h"
#include "wx_util.h"
#include <boost/bind/bind.hpp>

using boost::bind;

InstantI18NDialog::InstantI18NDialog (wxWindow* parent, wxString text)
	: wxDialog (parent, wxID_ANY, _("Translate"))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);

	_text = new wxTextCtrl (this, wxID_ANY, text, wxDefaultPosition, wxSize(200, -1), wxTE_PROCESS_ENTER);

	_text->Bind (wxEVT_TEXT_ENTER, bind(&InstantI18NDialog::close, this));

	overall_sizer->Add (_text, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	SetSizerAndFit (overall_sizer);
}

wxString
InstantI18NDialog::get () const
{
	return _text->GetValue ();
}

void
InstantI18NDialog::close ()
{
	Close ();
}
