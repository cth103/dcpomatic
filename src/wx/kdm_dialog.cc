/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <wx/treectrl.h>
#include <wx/datectrl.h>
#include <wx/timectrl.h>
#include "lib/cinema.h"
#include "kdm_dialog.h"
#include "new_cinema_dialog.h"
#include "wx_util.h"
#ifdef __WXMSW__
#include "dir_picker_ctrl.h"
#else
#include <wx/filepicker.h>
#endif

using namespace std;
using boost::shared_ptr;

KDMDialog::KDMDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Make KDMs"))
{
	wxBoxSizer* vertical = new wxBoxSizer (wxVERTICAL);

	add_label_to_sizer (vertical, this, "Make KDMs for");

	wxBoxSizer* targets = new wxBoxSizer (wxHORIZONTAL);
	
	_targets = new wxTreeCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_MULTIPLE | wxTR_HAS_BUTTONS);
	targets->Add (_targets, 1, wxEXPAND | wxALL, 6);

	_root = _targets->AddRoot ("Foo");
	shared_ptr<Cinema> csy (new Cinema ("City Screen York"));
	add_cinema (csy);
	add_screen (csy, shared_ptr<Screen> (new Screen ("Screen 1")));
	add_screen (csy, shared_ptr<Screen> (new Screen ("Screen 2")));
	add_screen (csy, shared_ptr<Screen> (new Screen ("Screen 3")));
	_targets->ExpandAll ();

	wxBoxSizer* target_buttons = new wxBoxSizer (wxVERTICAL);

	_new_cinema = new wxButton (this, wxID_ANY, _("New Cinema..."));
	target_buttons->Add (_new_cinema, 1, wxEXPAND | wxALL, 6);
	_new_screen = new wxButton (this, wxID_ANY, _("New Screen..."));
	target_buttons->Add (_new_screen, 1, wxEXPAND | wxALL, 6);

	targets->Add (target_buttons, 0, wxEXPAND | wxALL, 6);

	vertical->Add (targets, 0, wxEXPAND | wxALL, 6);

	wxFlexGridSizer* table = new wxFlexGridSizer (3, 2, 6);
	add_label_to_sizer (table, this, "From");
	_from_date = new wxDatePickerCtrl (this, wxID_ANY);
	table->Add (_from_date, 1, wxEXPAND);
	_from_time = new wxTimePickerCtrl (this, wxID_ANY);
	table->Add (_from_time, 1, wxEXPAND);
	
	add_label_to_sizer (table, this, "To");
	_to_date = new wxDatePickerCtrl (this, wxID_ANY);
	table->Add (_to_date, 1, wxEXPAND);
	_to_time = new wxTimePickerCtrl (this, wxID_ANY);
	table->Add (_to_time, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Write to");

#ifdef __WXMSW__
	_folder = new DirPickerCtrl (this);
#else	
	_folder = new wxDirPickerCtrl (this, wxDD_DIR_MUST_EXIST);
#endif

	table->Add (_folder, 1, wxEXPAND);
	
	vertical->Add (table, 1, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		vertical->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	_targets->Connect (wxID_ANY, wxEVT_COMMAND_TREE_SEL_CHANGED, wxCommandEventHandler (KDMDialog::targets_selection_changed), 0, this);
	_new_cinema->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::new_cinema_clicked), 0, this);
	_new_screen->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::new_screen_clicked), 0, this);

	_new_screen->Enable (false);
	
	SetSizer (vertical);
	vertical->Layout ();
	vertical->SetSizeHints (this);
}

void
KDMDialog::targets_selection_changed (wxCommandEvent &)
{
	wxArrayTreeItemIds s;
	_targets->GetSelections (s);

	bool selected_cinema = false;
	for (size_t i = 0; i < s.GetCount(); ++i) {
		if (_cinemas.find (s[i]) != _cinemas.end()) {
			selected_cinema = true;
		}
	}
	
	_new_screen->Enable (selected_cinema);	
}

void
KDMDialog::add_cinema (shared_ptr<Cinema> c)
{
	_cinemas[_targets->AppendItem (_root, std_to_wx (c->name))] = c;
}

void
KDMDialog::add_screen (shared_ptr<Cinema> c, shared_ptr<Screen> s)
{
	map<wxTreeItemId, shared_ptr<Cinema> >::const_iterator i = _cinemas.begin();
	while (i != _cinemas.end() && i->second != c) {
		++i;
	}

	if (i == _cinemas.end()) {
		return;
	}

	_screens[_targets->AppendItem (i->first, std_to_wx (s->name))] = s;
}

void
KDMDialog::new_cinema_clicked (wxCommandEvent &)
{
	NewCinemaDialog* d = new NewCinemaDialog (this);
	d->ShowModal ();
	d->Destroy ();
}

void
KDMDialog::new_screen_clicked (wxCommandEvent &)
{

}
