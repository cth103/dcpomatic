/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_MAKE_CHAIN_DIALOG_H
#define DCPOMATIC_MAKE_CHAIN_DIALOG_H

#include "table_dialog.h"
#include "wx_util.h"

class MakeChainDialog : public TableDialog
{
public:
	MakeChainDialog (
		wxWindow* parent,
		std::string organisation,
		std::string organisational_unit_name,
		std::string root_common_name,
		std::string intermediate_common_name,
		std::string leaf_common_name
		);

	std::string organisation () const {
		return wx_to_std (_organisation->GetValue ());
	}

	std::string organisational_unit () const {
		return wx_to_std (_organisational_unit->GetValue ());
	}

	std::string root_common_name () const {
		return "." + wx_to_std (_root_common_name->GetValue ());
	}

	std::string intermediate_common_name () const {
		return "." + wx_to_std (_intermediate_common_name->GetValue ());
	}

	std::string leaf_common_name () const {
		return "CS." + wx_to_std (_leaf_common_name->GetValue ());
	}


private:
	wxTextCtrl* _organisation;
	wxTextCtrl* _organisational_unit;
	wxTextCtrl* _root_common_name;
	wxTextCtrl* _intermediate_common_name;
	wxTextCtrl* _leaf_common_name;
};

#endif
