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


#include "lib/collator.h"
#include "lib/dkdm_recipient.h"
#include "lib/dkdm_recipient_list.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/srchctrl.h>
#include <wx/treectrl.h>
#include <wx/wx.h>
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS
#include <list>
#include <map>


class DKDMRecipient;


class RecipientsPanel : public wxPanel
{
public:
	explicit RecipientsPanel (wxWindow* parent);
	~RecipientsPanel ();

	void setup_sensitivity ();

	/** @return List of selected recipients */
	std::list<DKDMRecipient> recipients() const;

	boost::signals2::signal<void ()> RecipientsChanged;

private:
	void add_recipients ();
	void add_recipient(DKDMRecipientID id, DKDMRecipient const& recipient);
	void add_recipient_clicked ();
	void edit_recipient_clicked ();
	void remove_recipient_clicked ();
	void selection_changed_shim (wxTreeEvent &);
	void selection_changed ();
	void search_changed ();

	wxSearchCtrl* _search;
	wxTreeCtrl* _targets;
	wxButton* _add_recipient;
	wxButton* _edit_recipient;
	wxButton* _remove_recipient;
	wxTreeItemId _root;

	typedef std::map<wxTreeItemId, DKDMRecipientID> RecipientMap;
	RecipientMap _recipients;
	RecipientMap _selected;

	bool _ignore_selection_change;

	Collator _collator;
};
