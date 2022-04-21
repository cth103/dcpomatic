/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_LANGUAGE_TAG_DIALOG_H
#define DCPOMATIC_LANGUAGE_TAG_DIALOG_H


#include "lib/warnings.h"
#include <dcp/language_tag.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS


class wxListCtrl;


class LanguageTagDialog : public wxDialog
{
public:
	LanguageTagDialog (wxWindow* parent, dcp::LanguageTag tag = dcp::LanguageTag("en"));

	dcp::LanguageTag get () const;
	void set (dcp::LanguageTag tag);

private:
	void add_language ();
	void populate_list ();

	std::vector<dcp::LanguageTag> _presets;
	std::vector<dcp::LanguageTag> _custom;
	wxListCtrl* _list;
};

#endif
