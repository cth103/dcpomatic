/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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


#include "wx_util.h"
#include <wx/wx.h>


class SendI18NDialog : public wxDialog
{
public:
	SendI18NDialog (wxWindow* parent);

	std::string name () {
		return wx_to_std (_name->GetValue());
	}

	std::string email () {
		return wx_to_std (_email->GetValue());
	}

	std::string language () {
		return wx_to_std (_language->GetValue());
	}

private:
	wxTextCtrl* _name;
	wxTextCtrl* _email;
	wxTextCtrl* _language;
};
