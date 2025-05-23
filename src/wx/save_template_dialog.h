/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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


#include "table_dialog.h"

class Choice;


class SaveTemplateDialog : public TableDialog
{
public:
	explicit SaveTemplateDialog (wxWindow* parent);

	boost::optional<std::string> name() const;

private:
	void setup_sensitivity ();
	void check (wxCommandEvent& ev);

	wxRadioButton* _default;
	wxRadioButton* _existing;
	Choice* _existing_name;
	wxRadioButton* _new;
	wxTextCtrl* _new_name;
};
