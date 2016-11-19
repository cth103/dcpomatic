/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "templates_dialog.h"
#include "wx_util.h"
#include "rename_template_dialog.h"
#include "lib/config.h"
#include <wx/wx.h>
#include <boost/foreach.hpp>

using std::string;
using boost::bind;

TemplatesDialog::TemplatesDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Templates"))
{
	_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_sizer);

	wxSizer* hs = new wxBoxSizer (wxHORIZONTAL);
	_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (200, 100), wxLC_REPORT | wxLC_SINGLE_SEL);

	wxListItem ip;
	ip.SetId (0);
	ip.SetText (_("Template"));
	ip.SetWidth (200);
	_list->InsertColumn (0, ip);

	hs->Add (_list, 1, wxEXPAND, DCPOMATIC_SIZER_GAP);

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		_rename = new wxButton (this, wxID_ANY, _("Rename..."));
		s->Add (_rename, 0, wxTOP | wxBOTTOM, 2);
		_remove = new wxButton (this, wxID_ANY, _("Remove"));
		s->Add (_remove, 0, wxTOP | wxBOTTOM, 2);
		hs->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
	}

	_sizer->Add (hs, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	_rename->Bind (wxEVT_BUTTON, bind (&TemplatesDialog::rename_clicked, this));
	_remove->Bind (wxEVT_BUTTON, bind (&TemplatesDialog::remove_clicked, this));

	_list->Bind (wxEVT_LIST_ITEM_SELECTED, bind (&TemplatesDialog::selection_changed, this));
	_list->Bind (wxEVT_LIST_ITEM_DESELECTED, bind (&TemplatesDialog::selection_changed, this));
	_list->Bind (wxEVT_SIZE, bind (&TemplatesDialog::resized, this, _1));
	_config_connection = Config::instance()->Changed.connect (bind (&TemplatesDialog::refresh, this));

	refresh ();
	selection_changed ();
}

void
TemplatesDialog::refresh ()
{
	_list->DeleteAllItems ();

	BOOST_FOREACH (string i, Config::instance()->templates()) {
		wxListItem list_item;
		int const n = _list->GetItemCount ();
		list_item.SetId (n);
		_list->InsertItem (list_item);
		_list->SetItem (n, 0, std_to_wx (i));
	}
}

void
TemplatesDialog::layout ()
{
	_sizer->Layout ();
}

void
TemplatesDialog::selection_changed ()
{
	int const i = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	_rename->Enable (i >= 0);
	_remove->Enable (i >= 0);
}

void
TemplatesDialog::rename_clicked ()
{
	int item = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (item == -1) {
		return;
	}

	wxListItem li;
	li.m_itemId = item;
	li.m_col = 0;
	li.m_mask = wxLIST_MASK_TEXT;
	_list->GetItem (li);

	RenameTemplateDialog* d = new RenameTemplateDialog (this);
	d->set (li.m_text);
	if (d->ShowModal() == wxID_OK) {
		if (!d->get().IsEmpty()) {
			Config::instance()->rename_template (wx_to_std (li.m_text), wx_to_std (d->get ()));
			_list->SetItem (item, 0, d->get());
		} else {
			error_dialog (this, _("Template names must not be empty."));
		}
	}
	d->Destroy ();
}

void
TemplatesDialog::remove_clicked ()
{
	int i = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (i == -1) {
		return;
	}

	wxListItem li;
	li.m_itemId = i;
	li.m_col = 0;
	li.m_mask = wxLIST_MASK_TEXT;
	_list->GetItem (li);

	Config::instance()->delete_template (wx_to_std (li.m_text));
	_list->DeleteItem (i);

	selection_changed ();
}

void
TemplatesDialog::resized (wxSizeEvent& ev)
{
	_list->SetColumnWidth (0, GetSize().GetWidth());
	ev.Skip ();
}
