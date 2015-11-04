/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/config.h"
#include "lib/cinema.h"
#include "lib/screen.h"
#include "screens_panel.h"
#include "wx_util.h"
#include "cinema_dialog.h"
#include "screen_dialog.h"

using std::list;
using std::pair;
using std::cout;
using std::map;
using std::make_pair;
using boost::shared_ptr;

ScreensPanel::ScreensPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	wxBoxSizer* targets = new wxBoxSizer (wxHORIZONTAL);
	_targets = new wxTreeCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_MULTIPLE | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT);
	targets->Add (_targets, 1, wxEXPAND | wxTOP | wxRIGHT, DCPOMATIC_SIZER_GAP);

	_root = _targets->AddRoot ("Foo");

	list<shared_ptr<Cinema> > c = Config::instance()->cinemas ();
	for (list<shared_ptr<Cinema> >::iterator i = c.begin(); i != c.end(); ++i) {
		add_cinema (*i);
	}

	_targets->ExpandAll ();

	wxBoxSizer* target_buttons = new wxBoxSizer (wxVERTICAL);

	_add_cinema = new wxButton (this, wxID_ANY, _("Add Cinema..."));
	target_buttons->Add (_add_cinema, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_cinema = new wxButton (this, wxID_ANY, _("Edit Cinema..."));
	target_buttons->Add (_edit_cinema, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_cinema = new wxButton (this, wxID_ANY, _("Remove Cinema"));
	target_buttons->Add (_remove_cinema, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

	_add_screen = new wxButton (this, wxID_ANY, _("Add Screen..."));
	target_buttons->Add (_add_screen, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_screen = new wxButton (this, wxID_ANY, _("Edit Screen..."));
	target_buttons->Add (_edit_screen, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_screen = new wxButton (this, wxID_ANY, _("Remove Screen"));
	target_buttons->Add (_remove_screen, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

	targets->Add (target_buttons, 0, 0);

	sizer->Add (targets, 1, wxEXPAND);

	_targets->Bind       (wxEVT_COMMAND_TREE_SEL_CHANGED, &ScreensPanel::selection_changed, this);

	_add_cinema->Bind    (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreensPanel::add_cinema_clicked, this));
	_edit_cinema->Bind   (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreensPanel::edit_cinema_clicked, this));
	_remove_cinema->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreensPanel::remove_cinema_clicked, this));

	_add_screen->Bind    (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreensPanel::add_screen_clicked, this));
	_edit_screen->Bind   (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreensPanel::edit_screen_clicked, this));
	_remove_screen->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ScreensPanel::remove_screen_clicked, this));

	SetSizer (sizer);
}

ScreensPanel::~ScreensPanel ()
{
	_targets->Unbind (wxEVT_COMMAND_TREE_SEL_CHANGED, &ScreensPanel::selection_changed, this);
}

list<pair<wxTreeItemId, shared_ptr<Cinema> > >
ScreensPanel::selected_cinemas () const
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
ScreensPanel::selected_screens () const
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
ScreensPanel::setup_sensitivity ()
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
ScreensPanel::add_cinema (shared_ptr<Cinema> c)
{
	_cinemas[_targets->AppendItem (_root, std_to_wx (c->name))] = c;

	list<shared_ptr<Screen> > sc = c->screens ();
	for (list<shared_ptr<Screen> >::iterator i = sc.begin(); i != sc.end(); ++i) {
		add_screen (c, *i);
	}
}

void
ScreensPanel::add_screen (shared_ptr<Cinema> c, shared_ptr<Screen> s)
{
	map<wxTreeItemId, shared_ptr<Cinema> >::const_iterator i = _cinemas.begin();
	while (i != _cinemas.end() && i->second != c) {
		++i;
	}

	if (i == _cinemas.end()) {
		return;
	}

	_screens[_targets->AppendItem (i->first, std_to_wx (s->name))] = s;
	_targets->Expand (i->first);
}

void
ScreensPanel::add_cinema_clicked ()
{
	CinemaDialog* d = new CinemaDialog (this, "Add Cinema");
	if (d->ShowModal () == wxID_OK) {
		shared_ptr<Cinema> c (new Cinema (d->name(), d->email()));
		Config::instance()->add_cinema (c);
		add_cinema (c);
	}

	d->Destroy ();
}

void
ScreensPanel::edit_cinema_clicked ()
{
	if (selected_cinemas().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Cinema> > c = selected_cinemas().front();

	CinemaDialog* d = new CinemaDialog (this, "Edit cinema", c.second->name, c.second->email);
	if (d->ShowModal () == wxID_OK) {
		c.second->name = d->name ();
		c.second->email = d->email ();
		_targets->SetItemText (c.first, std_to_wx (d->name()));
		Config::instance()->changed ();
	}

	d->Destroy ();
}

void
ScreensPanel::remove_cinema_clicked ()
{
	if (selected_cinemas().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Cinema> > c = selected_cinemas().front();

	Config::instance()->remove_cinema (c.second);
	_targets->Delete (c.first);
}

void
ScreensPanel::add_screen_clicked ()
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

	Config::instance()->changed ();

	d->Destroy ();
}

void
ScreensPanel::edit_screen_clicked ()
{
	if (selected_screens().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Screen> > s = selected_screens().front();

	ScreenDialog* d = new ScreenDialog (this, "Edit screen", s.second->name, s.second->certificate);
	if (d->ShowModal () == wxID_OK) {
		s.second->name = d->name ();
		s.second->certificate = d->certificate ();
		_targets->SetItemText (s.first, std_to_wx (d->name()));
		Config::instance()->changed ();
	}

	d->Destroy ();
}

void
ScreensPanel::remove_screen_clicked ()
{
	if (selected_screens().size() != 1) {
		return;
	}

	pair<wxTreeItemId, shared_ptr<Screen> > s = selected_screens().front();

	map<wxTreeItemId, shared_ptr<Cinema> >::iterator i = _cinemas.begin ();
	while (i != _cinemas.end ()) {
		list<shared_ptr<Screen> > sc = i->second->screens ();
		if (find (sc.begin(), sc.end(), s.second) != sc.end ()) {
			break;
		}
	}

	if (i == _cinemas.end()) {
		return;
	}

	i->second->remove_screen (s.second);
	_targets->Delete (s.first);

	Config::instance()->changed ();
}

list<shared_ptr<Screen> >
ScreensPanel::screens () const
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

void
ScreensPanel::selection_changed (wxTreeEvent &)
{
	setup_sensitivity ();
	ScreensChanged ();
}
