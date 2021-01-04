/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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

#include "save_template_dialog.h"
#include "wx_util.h"
#include "lib/config.h"

using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

SaveTemplateDialog::SaveTemplateDialog (wxWindow* parent)
	: TableDialog (parent, _("Save template"), 2, 1, true)
{
	add (_("Template name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (300, -1)));
	_name->SetFocus ();
	layout ();

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Bind (wxEVT_BUTTON, boost::bind(&SaveTemplateDialog::check, this, _1));

	_name->Bind (wxEVT_TEXT, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));

	setup_sensitivity ();
}


void
SaveTemplateDialog::setup_sensitivity ()
{
	wxButton* ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	if (ok) {
		ok->Enable (!_name->GetValue().IsEmpty());
	}
}


string
SaveTemplateDialog::name () const
{
	return wx_to_std (_name->GetValue ());
}

void
SaveTemplateDialog::check (wxCommandEvent& ev)
{
	bool ok = true;

	if (Config::instance()->existing_template (wx_to_std (_name->GetValue ()))) {
		ok = confirm_dialog (this, _("There is already a template with this name.  Do you want to overwrite it?"));
	}

	if (ok) {
		ev.Skip ();
	}
}
