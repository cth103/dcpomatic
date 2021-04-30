/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/config.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS


class NagDialog : public wxDialog
{
public:
	static bool maybe_nag (wxWindow* parent, Config::Nag nag, wxString message, bool can_cancel = false);

private:
	NagDialog (wxWindow* parent, Config::Nag nag, wxString message, bool can_cancel);
	void shut_up (wxCommandEvent& ev);

	wxStaticText* _text;
	Config::Nag _nag;
};
