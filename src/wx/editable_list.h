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


#include "dcpomatic_button.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/listctrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <vector>


class EditableListColumn
{
public:
	EditableListColumn (wxString name_)
		: name (name_)
		, growable (false)
	{}

	EditableListColumn (wxString name_, boost::optional<int> width_, bool growable_)
		: name (name_)
		, width (width_)
		, growable (growable_)
	{}

	wxString name;
	boost::optional<int> width;
	bool growable;
};

/** @param T type of things being edited.
 *  @param S dialog to edit a thing.
 *  @param get Function to get a std::vector of the things being edited.
 *  @param set Function set the things from a a std::vector.
 *  @param column Function to get the display string for a given column in a given item.
 */
template<class T, class S>
class EditableList : public wxPanel
{
public:
	EditableList (
		wxWindow* parent,
		std::vector<EditableListColumn> columns,
		std::function<std::vector<T> ()> get,
		std::function<void (std::vector<T>)> set,
		std::function<std::string (T, int)> column,
		bool can_edit = true,
		bool title = true
		)
		: wxPanel (parent)
		, _get (get)
		, _set (set)
		, _columns (columns)
		, _column (column)
		, _edit (0)
		, _default_width (200)
	{
		_sizer = new wxBoxSizer (wxHORIZONTAL);
		SetSizer (_sizer);

		long style = wxLC_REPORT | wxLC_SINGLE_SEL;
		if (!title) {
			style |= wxLC_NO_HEADER;
		}

		int total_width = 0;
		for (auto i: _columns) {
			total_width += i.width.get_value_or (_default_width);
		}

#ifdef __WXGTK3__
		/* With the GTK3 backend wxListCtrls are hard to pick out from the background of the
		 * window, so put a border in to help.
		 */
		auto border = new wxPanel (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_THEME);
		_list = new wxListCtrl (border, wxID_ANY, wxDefaultPosition, wxSize(total_width, 100), style);
		auto border_sizer = new wxBoxSizer (wxHORIZONTAL);
		border_sizer->Add (_list, 1, wxALL | wxEXPAND, 2);
		border->SetSizer (border_sizer);
#else
		_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize(total_width, 100), style);
#endif

		int j = 0;
		for (auto i: _columns) {
			wxListItem ip;
			ip.SetId (j);
			ip.SetText (i.name);
			_list->InsertColumn (j, ip);
			++j;
		}

#ifdef __WXGTK3__
		_sizer->Add (border, 1, wxEXPAND);
#else
		_sizer->Add (_list, 1, wxEXPAND);
#endif

		{
			wxSizer* s = new wxBoxSizer (wxVERTICAL);
			_add = new Button (this, _("Add..."));
			s->Add (_add, 1, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
			if (can_edit) {
				_edit = new Button (this, _("Edit..."));
				s->Add (_edit, 1, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
			}
			_remove = new Button (this, _("Remove"));
			s->Add (_remove, 1, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
			_sizer->Add (s, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		}

		_add->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&EditableList::add_clicked, this));
		if (_edit) {
			_edit->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&EditableList::edit_clicked, this));
		}
		_remove->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&EditableList::remove_clicked, this));

		_list->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&EditableList::selection_changed, this));
		_list->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&EditableList::selection_changed, this));
#if BOOST_VERSION >= 106100
		_list->Bind (wxEVT_SIZE, boost::bind (&EditableList::resized, this, boost::placeholders::_1));
#else
		_list->Bind (wxEVT_SIZE, boost::bind (&EditableList::resized, this, _1));
#endif

		refresh ();
		selection_changed ();
	}

	void refresh ()
	{
		_list->DeleteAllItems ();

		auto current = _get ();
		for (auto const& i: current) {
			add_to_control (i);
		}
	}

	boost::optional<T> selection () const
	{
		int item = _list->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (item == -1) {
			return {};
		}

		auto all = _get ();
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

		for (size_t i = 0; i < _columns.size(); ++i) {
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
			auto const v = dialog->get ();
			static_assert(std::is_same<typename std::remove_const<decltype(v)>::type, boost::optional<T>>::value, "get() must return boost::optional<T>");
			if (v) {
				add_to_control (v.get ());
				auto all = _get ();
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
			auto const v = dialog->get ();
			static_assert(std::is_same<typename std::remove_const<decltype(v)>::type, boost::optional<T>>::value, "get() must return boost::optional<T>");
			if (!v) {
				return;
			}

			all[item] = v.get ();
		}
		dialog->Destroy ();

		for (size_t i = 0; i < _columns.size(); ++i) {
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
		auto all = _get ();
		all.erase (all.begin() + i);
		_set (all);

		selection_changed ();
	}

	void resized (wxSizeEvent& ev)
	{
		int const w = _list->GetSize().GetWidth() - 2;

		int fixed_width = 0;
		int growable = 0;
		int j = 0;
		for (auto i: _columns) {
			fixed_width += i.width.get_value_or (_default_width);
			if (!i.growable) {
				_list->SetColumnWidth (j, i.width.get_value_or(_default_width));
			} else {
				++growable;
			}
			++j;
		}

		j = 0;
		for (auto i: _columns) {
			if (i.growable) {
				_list->SetColumnWidth (j, i.width.get_value_or(_default_width) + (w - fixed_width) / growable);
			}
			++j;
		}

		ev.Skip ();
	}

	std::function <std::vector<T> ()> _get;
	std::function <void (std::vector<T>)> _set;
	std::vector<EditableListColumn> _columns;
	std::function<std::string (T, int)> _column;

	wxButton* _add;
	wxButton* _edit;
	wxButton* _remove;
	wxListCtrl* _list;
	wxBoxSizer* _sizer;
	int _default_width;
};

#endif
