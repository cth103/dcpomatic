/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "table_dialog.h"
#include "wx_util.h"

class MakeSignerChainDialog : public TableDialog
{
public:
	MakeSignerChainDialog (wxWindow* parent);

	std::string organisation () const {
		return wx_to_std (_organisation->GetValue ());
	}

	std::string organisational_unit () const {
		return wx_to_std (_organisational_unit->GetValue ());
	}

	std::string root_common_name () const {
		return wx_to_std (_root_common_name->GetValue ());
	}

	std::string intermediate_common_name () const {
		return wx_to_std (_intermediate_common_name->GetValue ());
	}

	std::string leaf_common_name () const {
		return wx_to_std (_leaf_common_name->GetValue ());
	}
	

private:
	wxTextCtrl* _organisation;
	wxTextCtrl* _organisational_unit;
	wxTextCtrl* _root_common_name;
	wxTextCtrl* _intermediate_common_name;
	wxTextCtrl* _leaf_common_name;
};
	
