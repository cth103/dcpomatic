/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_EDITABLE_LIST_H
#define DCPOMATIC_EDITABLE_LIST_H

#include "wx_util.h"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <boost/function.hpp>
#include <vector>

/** @param T type of things being edited.
 *  @param S dialog to edit a thing.
 */
template<class T, class S>
class EditableList : public wxPanel
{
public:
	EditableList (
		wxWindow* parent,
		std::vector<std::string> columns,
		boost::function<std::vector<T> ()> get,
		boost::function<void (std::vector<T>)> set,
		boost::function<std::string (T, int)> column,
		bool can_edit = true,
		bool title = true
		)
		: wxPanel (parent)
		, _get (get)
		, _set (set)
		, _columns (columns.size ())
		, _column (column)
		, _edit (0)
	{
		_sizer = new wxBoxSizer (wxHORIZONTAL);
		SetSizer (_sizer);

		long style = wxLC_REPORT | wxLC_SINGLE_SEL;
		if (title) {
			style |= wxLC_NO_HEADER;
		}
		_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (columns.size() * 200, 100), style);

		for (size_t i = 0; i < columns.size(); ++i) {
			wxListItem ip;
			ip.SetId (i);
			ip.SetText (std_to_wx (columns[i]));
			ip.SetWidth (200);
			_list->InsertColumn (i, ip);
		}

		_sizer->Add (_list, 1, wxEXPAND);

		{
			wxSizer* s = new wxBoxSizer (wxVERTICAL);
			_add = new wxButton (this, wxID_ANY, _("Add..."));
			s->Add (_add, 0, wxTOP | wxBOTTOM, 2);
			if (can_edit) {
				_edit = new wxButton (this, wxID_ANY, _("Edit..."));
				s->Add (_edit, 0, wxTOP | wxBOTTOM, 2);
			}
			_remove = new wxButton (this, wxID_ANY, _("Remove"));
			s->Add (_remove, 0, wxTOP | wxBOTTOM, 2);
			_sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		}

		_add->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&EditableList::add_clicked, this));
		if (_edit) {
			_edit->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&EditableList::edit_clicked, this));
		}
		_remove->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&EditableList::remove_clicked, this));

		_list->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&EditableList::selection_changed, this));
		_list->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&EditableList::selection_changed, this));
		_list->Bind (wxEVT_SIZE, boost::bind (&EditableList::resized, this, _1));

		refresh ();
		selection_changed ();
	}

	void refresh ()
	{
		_list->DeleteAllItems ();

		std::vector<T> current = _get ();
		for (typename std::vector<T>::iterator i = current.begin (); i != current.end(); ++i) {
			add_to_control (*i);
		}
	}

	boost::optional<T> selection () const
	{
		int item = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) {
			return boost::optional<T> ();
		}

		std::vector<T> all = _get ();
		DCPOMATIC_ASSERT (item >= 0 && item < int (all.size ()));
		return all[item];
	}

	void layout ()
	{
		_sizer->Layout ();
	}

	boost::signals2::signal<void ()> SelectionChanged;

private:

	void add_to_control (T item)
	{
		wxListItem list_item;
		int const n = _list->GetItemCount ();
		list_item.SetId (n);
		_list->InsertItem (list_item);

		for (int i = 0; i < _columns; ++i) {
			_list->SetItem (n, i, std_to_wx (_column (item, i)));
		}
	}

	void selection_changed ()
	{
		int const i = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (_edit) {
			_edit->Enable (i >= 0);
		}
		_remove->Enable (i >= 0);

		SelectionChanged ();
	}

	void add_clicked ()
	{
		S* dialog = new S (this);

		if (dialog->ShowModal() == wxID_OK) {
			boost::optional<T> const v = dialog->get ();
			if (v) {
				add_to_control (v.get ());
				std::vector<T> all = _get ();
				all.push_back (v.get ());
				_set (all);
			}
		}

		dialog->Destroy ();
	}

	void edit_clicked ()
	{
		int item = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) {
			return;
		}

		std::vector<T> all = _get ();
		DCPOMATIC_ASSERT (item >= 0 && item < int (all.size ()));

		S* dialog = new S (this);
		dialog->set (all[item]);
		if (dialog->ShowModal() == wxID_OK) {
			boost::optional<T> const v = dialog->get ();
			if (!v) {
				return;
			}

			all[item] = v.get ();
		}
		dialog->Destroy ();

		for (int i = 0; i < _columns; ++i) {
			_list->SetItem (item, i, std_to_wx (_column (all[item], i)));
		}

		_set (all);
	}

	void remove_clicked ()
	{
		int i = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (i == -1) {
			return;
		}

		_list->DeleteItem (i);
		std::vector<T> all = _get ();
		all.erase (all.begin() + i);
		_set (all);

		selection_changed ();
	}

	void resized (wxSizeEvent& ev)
	{
		int const w = GetSize().GetWidth() / _columns;
		for (int i = 0; i < _columns; ++i) {
			_list->SetColumnWidth (i, w);
		}
		ev.Skip ();
	}

	boost::function <std::vector<T> ()> _get;
	boost::function <void (std::vector<T>)> _set;
	int _columns;
	boost::function<std::string (T, int)> _column;

	wxButton* _add;
	wxButton* _edit;
	wxButton* _remove;
	wxListCtrl* _list;
	wxBoxSizer* _sizer;
};

#endif
