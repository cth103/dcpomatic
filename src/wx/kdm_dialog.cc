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
#include "lib/config.h"
#include "kdm_dialog.h"
#include "cinema_dialog.h"
#include "screen_dialog.h"
#include "wx_util.h"
#ifdef __WXMSW__
#include "dir_picker_ctrl.h"
#else
#include <wx/filepicker.h>
#endif

using std::string;
using std::map;
using std::list;
using std::pair;
using std::make_pair;
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

	list<shared_ptr<Cinema> > c = Config::instance()->cinemas ();
	for (list<shared_ptr<Cinema> >::iterator i = c.begin(); i != c.end(); ++i) {
		add_cinema (*i);
	}

	_targets->ExpandAll ();

	wxBoxSizer* target_buttons = new wxBoxSizer (wxVERTICAL);

	_add_cinema = new wxButton (this, wxID_ANY, _("Add Cinema..."));
	target_buttons->Add (_add_cinema, 1, 0, 6);
	_edit_cinema = new wxButton (this, wxID_ANY, _("Edit Cinema..."));
	target_buttons->Add (_edit_cinema, 1, 0, 6);
	_remove_cinema = new wxButton (this, wxID_ANY, _("Remove Cinema"));
	target_buttons->Add (_remove_cinema, 1, 0, 6);
	
	_add_screen = new wxButton (this, wxID_ANY, _("Add Screen..."));
	target_buttons->Add (_add_screen, 1, 0, 6);
	_edit_screen = new wxButton (this, wxID_ANY, _("Edit Screen..."));
	target_buttons->Add (_edit_screen, 1, 0, 6);
	_remove_screen = new wxButton (this, wxID_ANY, _("Remove Screen"));
	target_buttons->Add (_remove_screen, 1, 0, 6);

	targets->Add (target_buttons, 0, 0, 6);

	vertical->Add (targets, 1, wxEXPAND | wxALL, 6);

	wxFlexGridSizer* table = new wxFlexGridSizer (3, 2, 6);
	add_label_to_sizer (table, this, "From");
	_from_date = new wxDatePickerCtrl (this, wxID_ANY);
	table->Add (_from_date, 1, wxEXPAND);
	_from_time = new wxTimePickerCtrl (this, wxID_ANY);
	table->Add (_from_time, 1, wxEXPAND);
	
	add_label_to_sizer (table, this, "Until");
	_until_date = new wxDatePickerCtrl (this, wxID_ANY);
	table->Add (_until_date, 1, wxEXPAND);
	_until_time = new wxTimePickerCtrl (this, wxID_ANY);
	table->Add (_until_time, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Write to");

#ifdef __WXMSW__
	_folder = new DirPickerCtrl (this);
#else	
	_folder = new wxDirPickerCtrl (this, wxDD_DIR_MUST_EXIST);
#endif

	table->Add (_folder, 1, wxEXPAND);
	
	vertical->Add (table, 0, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		vertical->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	_targets->Connect (wxID_ANY, wxEVT_COMMAND_TREE_SEL_CHANGED, wxCommandEventHandler (KDMDialog::targets_selection_changed), 0, this);

	_add_cinema->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::add_cinema_clicked), 0, this);
	_edit_cinema->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::edit_cinema_clicked), 0, this);
	_remove_cinema->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::remove_cinema_clicked), 0, this);

	_add_screen->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::add_screen_clicked), 0, this);
	_edit_screen->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::edit_screen_clicked), 0, this);
	_remove_screen->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (KDMDialog::remove_screen_clicked), 0, this);

	setup_sensitivity ();
	
	SetSizer (vertical);
	vertical->Layout ();
	vertical->SetSizeHints (this);
}

list<pair<wxTreeItemId, shared_ptr<Cinema> > >
KDMDialog::selected_cinemas () const
{
	wxArrayTreeItemIds s;
	_targets->GetSelections (s);

	list<pair<wxTreeItemId, shared_ptr<Cinema> > > c;
	for (size_t i = 0; i < s.GetCount(); ++i) {
		map<wxTreeItemId, shared_ptr<Cinema> >::const_iterator j = _cinemas.find (s[i]);
		if (j != _cinemas.end ()) {
			c.push_back (make_pair (j->first, j->second));
		}
	}

	return c;
}

list<pair<wxTreeItemId, shared_ptr<Screen> > >
KDMDialog::selected_screens () const
{
	wxArrayTreeItemIds s;
	_targets->GetSelections (s);

	list<pair<wxTreeItemId, shared_ptr<Screen> > > c;
	for (size_t i = 0; i < s.GetCount(); ++i) {
		map<wxTreeItemId, shared_ptr<Screen> >::const_iterator j = _screens.find (s[i]);
		if (j != _screens.end ()) {
			c.push_back (make_pair (j->first, j->second));
		}
	}

	return c;
}

void
KDMDialog::targets_selection_changed (wxCommandEvent &)
{
	setup_sensitivity ();
}

void
KDMDialog::setup_sensitivity ()
{
	bool const sc = selected_cinemas().size() == 1;
	bool const ss = selected_screens().size() == 1;
	
	_edit_cinema->Enable (sc);
	_remove_cinema->Enable (sc);
	
	_add_screen->Enable (sc);
	_edit_screen->Enable (ss);
	_remove_screen->Enable (ss);
}

void
KDMDialog::add_cinema (shared_ptr<Cinema> c)
{
	_cinemas[_targets->AppendItem (_root, std_to_wx (c->name))] = c;

	for (list<shared_ptr<Screen> >::iterator i = c->screens.begin(); i != c->screens.end(); ++i) {
		add_screen (c, *i);
	}
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
KDMDialog::add_cinema_clicked (wxCommandEvent &)
{
	CinemaDialog* d = new CinemaDialog (this, "Add Cinema");
	d->ShowModal ();

	shared_ptr<Cinema> c (new Cinema (d->name(), d->email()));
	Config::instance()->add_cinema (c);
	add_cinema (c);

	Config::instance()->write ();
	
	d->Destroy ();
}

void
KDMDialog::edit_cinema_clicked (wxCommandEvent &)
{
	if (selected_cinemas().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Cinema> > c = selected_cinemas().front();
	
	CinemaDialog* d = new CinemaDialog (this, "Edit cinema", c.second->name, c.second->email);
	d->ShowModal ();

	c.second->name = d->name ();
	c.second->email = d->email ();
	_targets->SetItemText (c.first, std_to_wx (d->name()));

	Config::instance()->write ();

	d->Destroy ();	
}

void
KDMDialog::remove_cinema_clicked (wxCommandEvent &)
{
	if (selected_cinemas().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Cinema> > c = selected_cinemas().front();

	Config::instance()->remove_cinema (c.second);
	_targets->Delete (c.first);

	Config::instance()->write ();	
}

void
KDMDialog::add_screen_clicked (wxCommandEvent &)
{
	if (selected_cinemas().size() != 1) {
		return;
	}

	shared_ptr<Cinema> c = selected_cinemas().front().second;
	
	ScreenDialog* d = new ScreenDialog (this, "Add Screen");
	d->ShowModal ();

	shared_ptr<Screen> s (new Screen (d->name()));
	c->screens.push_back (s);
	add_screen (c, s);

	Config::instance()->write ();

	d->Destroy ();
}

void
KDMDialog::edit_screen_clicked (wxCommandEvent &)
{
	if (selected_screens().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Screen> > s = selected_screens().front();
	
	ScreenDialog* d = new ScreenDialog (this, "Edit screen", s.second->name);
	d->ShowModal ();

	s.second->name = d->name ();
	_targets->SetItemText (s.first, std_to_wx (d->name()));

	Config::instance()->write ();

	d->Destroy ();
}

void
KDMDialog::remove_screen_clicked (wxCommandEvent &)
{
	if (selected_screens().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Screen> > s = selected_screens().front();

	map<wxTreeItemId, shared_ptr<Cinema> >::iterator i = _cinemas.begin ();
	while (i != _cinemas.end() && find (i->second->screens.begin(), i->second->screens.end(), s.second) == i->second->screens.end()) {
		++i;
	}

	if (i == _cinemas.end()) {
		return;
	}

	i->second->screens.remove (s.second);
	_targets->Delete (s.first);

	Config::instance()->write ();
}

list<shared_ptr<Screen> >
KDMDialog::screens () const
{
	list<shared_ptr<Screen> > s;

	list<pair<wxTreeItemId, shared_ptr<Cinema> > > cinemas = selected_cinemas ();
	for (list<pair<wxTreeItemId, shared_ptr<Cinema> > >::iterator i = cinemas.begin(); i != cinemas.end(); ++i) {
		for (list<Screen>::iterator j = i->second->screens.begin(); j != i->second->screens.end(); ++j) {
			s.push_back (*j);
		}
	}

	list<pair<wxTreeItemId, shared_ptr<Screen> > > screens = selected_screens ();
	for (list<pair<wxTreeItemId, shared_ptr<Screen> > >::iterator i = screens.begin(); i != screens.end(); ++i) {
		s.push_back (i->second);
	}

	s.sort ();
	s.uniq ();

	return s;
}

boost::locale::date_time
KDMDialog::from () const
{

}

boost::locale::date_time
KDMDialog::until () const
{

}

string
KDMDialog::directory () const
{
	return wx_to_std (_folder->GetPath ());
}
