/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


#include "recipients_panel.h"
#include "wx_util.h"
#include "recipient_dialog.h"
#include "dcpomatic_button.h"
#include "lib/config.h"
#include <list>
#include <iostream>


using std::cout;
using std::list;
using std::make_pair;
using std::map;
using std::pair;
using std::shared_ptr;
using std::string;
using boost::optional;
using namespace dcpomatic;


RecipientsPanel::RecipientsPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
	, _ignore_selection_change (false)
{
	auto sizer = new wxBoxSizer (wxVERTICAL);

#ifdef __WXGTK3__
	int const height = 30;
#else
	int const height = -1;
#endif

	_search = new wxSearchCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (200, height));
#ifndef __WXGTK3__
	/* The cancel button seems to be strangely broken in GTK3; clicking on it twice sometimes works */
	_search->ShowCancelButton (true);
#endif
	sizer->Add (_search, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);

	auto targets = new wxBoxSizer (wxHORIZONTAL);
	_targets = new wxTreeCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HIDE_ROOT | wxTR_MULTIPLE | wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT);
	targets->Add (_targets, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);

	add_recipients ();

	auto target_buttons = new wxBoxSizer (wxVERTICAL);

	_add_recipient = new Button (this, _("Add..."));
	target_buttons->Add (_add_recipient, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_edit_recipient = new Button (this, _("Edit..."));
	target_buttons->Add (_edit_recipient, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_remove_recipient = new Button (this, _("Remove"));
	target_buttons->Add (_remove_recipient, 1, wxEXPAND | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	targets->Add (target_buttons, 0, 0);

	sizer->Add (targets, 1, wxEXPAND);

	_search->Bind  (wxEVT_TEXT, boost::bind (&RecipientsPanel::search_changed, this));
	_targets->Bind (wxEVT_TREE_SEL_CHANGED, &RecipientsPanel::selection_changed_shim, this);

	_add_recipient->Bind    (wxEVT_BUTTON, boost::bind (&RecipientsPanel::add_recipient_clicked, this));
	_edit_recipient->Bind   (wxEVT_BUTTON, boost::bind (&RecipientsPanel::edit_recipient_clicked, this));
	_remove_recipient->Bind (wxEVT_BUTTON, boost::bind (&RecipientsPanel::remove_recipient_clicked, this));

	SetSizer (sizer);
}


RecipientsPanel::~RecipientsPanel ()
{
	_targets->Unbind (wxEVT_TREE_SEL_CHANGED, &RecipientsPanel::selection_changed_shim, this);
}


void
RecipientsPanel::setup_sensitivity ()
{
	_edit_recipient->Enable (_selected.size() == 1);
	_remove_recipient->Enable (_selected.size() >= 1);
}


void
RecipientsPanel::add_recipient (shared_ptr<DKDMRecipient> r)
{
	string search = wx_to_std (_search->GetValue());
	transform (search.begin(), search.end(), search.begin(), ::tolower);

	if (!search.empty()) {
		string name = r->name;
		transform (name.begin(), name.end(), name.begin(), ::tolower);
		if (name.find(search) == string::npos) {
			return;
		}
	}

	_recipients[_targets->AppendItem(_root, std_to_wx(r->name))] = r;

	_targets->SortChildren (_root);
}


void
RecipientsPanel::add_recipient_clicked ()
{
	RecipientDialog dialog(GetParent(), _("Add recipient"));
	if (dialog.ShowModal() == wxID_OK) {
		auto r = std::make_shared<DKDMRecipient>(dialog.name(), dialog.notes(), dialog.recipient(), dialog.emails(), dialog.utc_offset_hour(), dialog.utc_offset_minute());
		Config::instance()->add_dkdm_recipient (r);
		add_recipient (r);
	}
}


void
RecipientsPanel::edit_recipient_clicked ()
{
	if (_selected.size() != 1) {
		return;
	}

	auto c = *_selected.begin();

	RecipientDialog dialog(
		GetParent(), _("Edit recipient"), c.second->name, c.second->notes, c.second->emails, c.second->utc_offset_hour, c.second->utc_offset_minute, c.second->recipient
		);

	if (dialog.ShowModal() == wxID_OK) {
		c.second->name = dialog.name();
		c.second->emails = dialog.emails();
		c.second->notes = dialog.notes();
		c.second->utc_offset_hour = dialog.utc_offset_hour();
		c.second->utc_offset_minute = dialog.utc_offset_minute();
		_targets->SetItemText(c.first, std_to_wx(dialog.name()));
		Config::instance()->changed (Config::DKDM_RECIPIENTS);
	}
}


void
RecipientsPanel::remove_recipient_clicked ()
{
	for (auto const& i: _selected) {
		Config::instance()->remove_dkdm_recipient (i.second);
		_targets->Delete (i.first);
	}

	selection_changed ();
}


list<shared_ptr<DKDMRecipient>>
RecipientsPanel::recipients () const
{
	list<shared_ptr<DKDMRecipient>> r;

	for (auto const& i: _selected) {
		r.push_back (i.second);
	}

	r.sort ();
	r.unique ();

	return r;
}


void
RecipientsPanel::selection_changed_shim (wxTreeEvent &)
{
	selection_changed ();
}


void
RecipientsPanel::selection_changed ()
{
	if (_ignore_selection_change) {
		return;
	}

	wxArrayTreeItemIds s;
	_targets->GetSelections (s);

	_selected.clear ();

	for (size_t i = 0; i < s.GetCount(); ++i) {
		RecipientMap::const_iterator j = _recipients.find (s[i]);
		if (j != _recipients.end ()) {
			_selected[j->first] = j->second;
		}
	}

	setup_sensitivity ();
	RecipientsChanged ();
}


void
RecipientsPanel::add_recipients ()
{
	_root = _targets->AddRoot ("Foo");

	for (auto i: Config::instance()->dkdm_recipients()) {
		add_recipient (i);
	}
}


void
RecipientsPanel::search_changed ()
{
	_targets->DeleteAllItems ();
	_recipients.clear ();

	add_recipients ();

	_ignore_selection_change = true;

	for (auto const& i: _selected) {
		/* The wxTreeItemIds will now be different, so we must search by recipient */
		auto j = _recipients.begin ();
		while (j != _recipients.end() && j->second != i.second) {
			++j;
		}

		if (j != _recipients.end ()) {
			_targets->SelectItem (j->first);
		}
	}

	_ignore_selection_change = false;
}
