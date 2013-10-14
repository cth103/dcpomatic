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
#include <wx/stdpaths.h>
#include "lib/cinema.h"
#include "lib/config.h"
#include "kdm_dialog.h"
#include "cinema_dialog.h"
#include "screen_dialog.h"
#include "wx_util.h"
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
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
	target_buttons->Add (_add_cinema, 1, wxEXPAND, 6);
	_edit_cinema = new wxButton (this, wxID_ANY, _("Edit Cinema..."));
	target_buttons->Add (_edit_cinema, 1, wxEXPAND, 6);
	_remove_cinema = new wxButton (this, wxID_ANY, _("Remove Cinema"));
	target_buttons->Add (_remove_cinema, 1, wxEXPAND, 6);
	
	_add_screen = new wxButton (this, wxID_ANY, _("Add Screen..."));
	target_buttons->Add (_add_screen, 1, wxEXPAND, 6);
	_edit_screen = new wxButton (this, wxID_ANY, _("Edit Screen..."));
	target_buttons->Add (_edit_screen, 1, wxEXPAND, 6);
	_remove_screen = new wxButton (this, wxID_ANY, _("Remove Screen"));
	target_buttons->Add (_remove_screen, 1, wxEXPAND, 6);

	targets->Add (target_buttons, 0, 0, 6);

	vertical->Add (targets, 1, wxEXPAND | wxALL, 6);

	wxFlexGridSizer* table = new wxFlexGridSizer (3, 2, 6);
	add_label_to_sizer (table, this, _("From"), true);
	_from_date = new wxDatePickerCtrl (this, wxID_ANY);
	table->Add (_from_date, 1, wxEXPAND);
	_from_time = new wxTimePickerCtrl (this, wxID_ANY);
	table->Add (_from_time, 1, wxEXPAND);
	
	add_label_to_sizer (table, this, _("Until"), true);
	_until_date = new wxDatePickerCtrl (this, wxID_ANY);
	table->Add (_until_date, 1, wxEXPAND);
	_until_time = new wxTimePickerCtrl (this, wxID_ANY);
	table->Add (_until_time, 1, wxEXPAND);

	_write_to = new wxRadioButton (this, wxID_ANY, _("Write to"));
	table->Add (_write_to, 1, wxEXPAND);

#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	_folder = new DirPickerCtrl (this); 
#else	
	_folder = new wxDirPickerCtrl (this, wxID_ANY);
#endif

	_folder->SetPath (wxStandardPaths::Get().GetDocumentsDir());
	
	table->Add (_folder, 1, wxEXPAND);
	table->AddSpacer (0);

	_email = new wxRadioButton (this, wxID_ANY, _("Send by email"));
	table->Add (_email, 1, wxEXPAND);
	table->AddSpacer (0);
	
	vertical->Add (table, 0, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		vertical->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	_targets->Bind       (wxEVT_COMMAND_TREE_SEL_CHANGED, boost::bind (&KDMDialog::setup_sensitivity, this));

	_add_cinema->Bind    (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&KDMDialog::add_cinema_clicked, this));
	_edit_cinema->Bind   (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&KDMDialog::edit_cinema_clicked, this));
	_remove_cinema->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&KDMDialog::remove_cinema_clicked, this));

	_add_screen->Bind    (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&KDMDialog::add_screen_clicked, this));
	_edit_screen->Bind   (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&KDMDialog::edit_screen_clicked, this));
	_remove_screen->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&KDMDialog::remove_screen_clicked, this));

	_write_to->Bind      (wxEVT_COMMAND_RADIOBUTTON_SELECTED, boost::bind (&KDMDialog::setup_sensitivity, this));
	_email->Bind         (wxEVT_COMMAND_RADIOBUTTON_SELECTED, boost::bind (&KDMDialog::setup_sensitivity, this));

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
KDMDialog::setup_sensitivity ()
{
	bool const sc = selected_cinemas().size() == 1;
	bool const ss = selected_screens().size() == 1;
	
	_edit_cinema->Enable (sc);
	_remove_cinema->Enable (sc);
	
	_add_screen->Enable (sc);
	_edit_screen->Enable (ss);
	_remove_screen->Enable (ss);

	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK));
	ok->Enable (sc || ss);

	_folder->Enable (_write_to->GetValue ());
}

void
KDMDialog::add_cinema (shared_ptr<Cinema> c)
{
	_cinemas[_targets->AppendItem (_root, std_to_wx (c->name))] = c;

	list<shared_ptr<Screen> > sc = c->screens ();
	for (list<shared_ptr<Screen> >::iterator i = sc.begin(); i != sc.end(); ++i) {
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
KDMDialog::add_cinema_clicked ()
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
KDMDialog::edit_cinema_clicked ()
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
KDMDialog::remove_cinema_clicked ()
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
KDMDialog::add_screen_clicked ()
{
	if (selected_cinemas().size() != 1) {
		return;
	}

	shared_ptr<Cinema> c = selected_cinemas().front().second;
	
	ScreenDialog* d = new ScreenDialog (this, "Add Screen");
	if (d->ShowModal () != wxID_OK) {
		return;
	}

	shared_ptr<Screen> s (new Screen (d->name(), d->certificate()));
	c->add_screen (s);
	add_screen (c, s);

	Config::instance()->write ();

	d->Destroy ();
}

void
KDMDialog::edit_screen_clicked ()
{
	if (selected_screens().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Screen> > s = selected_screens().front();
	
	ScreenDialog* d = new ScreenDialog (this, "Edit screen", s.second->name, s.second->certificate);
	d->ShowModal ();

	s.second->name = d->name ();
	s.second->certificate = d->certificate ();
	_targets->SetItemText (s.first, std_to_wx (d->name()));

	Config::instance()->write ();

	d->Destroy ();
}

void
KDMDialog::remove_screen_clicked ()
{
	if (selected_screens().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Screen> > s = selected_screens().front();

	map<wxTreeItemId, shared_ptr<Cinema> >::iterator i = _cinemas.begin ();
	list<shared_ptr<Screen> > sc = i->second->screens ();
	while (i != _cinemas.end() && find (sc.begin(), sc.end(), s.second) == sc.end()) {
		++i;
	}

	if (i == _cinemas.end()) {
		return;
	}

	i->second->remove_screen (s.second);
	_targets->Delete (s.first);

	Config::instance()->write ();
}

list<shared_ptr<Screen> >
KDMDialog::screens () const
{
	list<shared_ptr<Screen> > s;

	list<pair<wxTreeItemId, shared_ptr<Cinema> > > cinemas = selected_cinemas ();
	for (list<pair<wxTreeItemId, shared_ptr<Cinema> > >::iterator i = cinemas.begin(); i != cinemas.end(); ++i) {
		list<shared_ptr<Screen> > sc = i->second->screens ();
		for (list<shared_ptr<Screen> >::const_iterator j = sc.begin(); j != sc.end(); ++j) {
			s.push_back (*j);
		}
	}

	list<pair<wxTreeItemId, shared_ptr<Screen> > > screens = selected_screens ();
	for (list<pair<wxTreeItemId, shared_ptr<Screen> > >::iterator i = screens.begin(); i != screens.end(); ++i) {
		s.push_back (i->second);
	}

	s.sort ();
	s.unique ();

	return s;
}

boost::posix_time::ptime
KDMDialog::from () const
{
	return posix_time (_from_date, _from_time);
}

boost::posix_time::ptime
KDMDialog::posix_time (wxDatePickerCtrl* date_picker, wxTimePickerCtrl* time_picker)
{
	wxDateTime const date = date_picker->GetValue ();
	wxDateTime const time = time_picker->GetValue ();
	return boost::posix_time::ptime (
		boost::gregorian::date (date.GetYear(), date.GetMonth() + 1, date.GetDay()),
		boost::posix_time::time_duration (time.GetHour(), time.GetMinute(), time.GetSecond())
		);
}

boost::posix_time::ptime
KDMDialog::until () const
{
	return posix_time (_until_date, _until_time);
}

string
KDMDialog::directory () const
{
	return wx_to_std (_folder->GetPath ());
}

bool
KDMDialog::write_to () const
{
	return _write_to->GetValue ();
}
