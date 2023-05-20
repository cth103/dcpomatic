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
#include "lib/dkdm_recipient_list.h"
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
RecipientsPanel::add_recipient(DKDMRecipientID id, DKDMRecipient const& recipient)
{
	string const search = wx_to_std(_search->GetValue());

	if (!search.empty() && !_collator.find(search, recipient.name)) {
		return;
	}

	_recipients.emplace(make_pair(_targets->AppendItem(_root, std_to_wx(recipient.name)), id));

	_targets->SortChildren (_root);
}


void
RecipientsPanel::add_recipient_clicked ()
{
	RecipientDialog dialog(GetParent(), _("Add recipient"));
	if (dialog.ShowModal() == wxID_OK) {
		auto recipient = DKDMRecipient(dialog.name(), dialog.notes(), dialog.recipient(), dialog.emails());
		DKDMRecipientList recipient_list;
		auto const id = recipient_list.add_dkdm_recipient(recipient);
		add_recipient(id, recipient);
	}
}


void
RecipientsPanel::edit_recipient_clicked ()
{
	if (_selected.size() != 1) {
		return;
	}

	DKDMRecipientList recipients;
	auto selection = *_selected.begin();
	auto const recipient_id = selection.second;
	auto recipient = recipients.dkdm_recipient(recipient_id);
	DCPOMATIC_ASSERT(recipient);

	RecipientDialog dialog(
		GetParent(),
		_("Edit recipient"),
		recipient->name,
		recipient->notes,
		recipient->emails,
		recipient->recipient
		);

	if (dialog.ShowModal() == wxID_OK) {
		recipient->name = dialog.name();
		recipient->emails = dialog.emails();
		recipient->notes = dialog.notes();
		recipient->recipient = dialog.recipient();
		recipients.update_dkdm_recipient(recipient_id, *recipient);
		_targets->SetItemText(selection.first, std_to_wx(dialog.name()));
	}
}


void
RecipientsPanel::remove_recipient_clicked ()
{
	for (auto const& i: _selected) {
		DKDMRecipientList recipient_list;
		recipient_list.remove_dkdm_recipient(i.second);
		_targets->Delete (i.first);
	}

	selection_changed ();
}


list<DKDMRecipient>
RecipientsPanel::recipients () const
{
	list<DKDMRecipient> all;
	DKDMRecipientList recipients;
	for (auto const& recipient: recipients.dkdm_recipients()) {
		all.push_back(recipient.second);
	}
	return all;
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
			_selected.emplace(*j);
		}
	}

	setup_sensitivity ();
	RecipientsChanged ();
}


void
RecipientsPanel::add_recipients ()
{
	_root = _targets->AddRoot ("Foo");

	DKDMRecipientList recipients;
	for (auto const& recipient: recipients.dkdm_recipients()) {
		add_recipient(recipient.first, recipient.second);
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
