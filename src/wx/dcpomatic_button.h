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


#ifndef DCPOMATIC_BUTTON_H
#define DCPOMATIC_BUTTON_H


#include "i18n_hook.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/button.h>
DCPOMATIC_ENABLE_WARNINGS


class Button : public wxButton, public I18NHook
{
public:
	Button (wxWindow* parent, wxString label, wxPoint pos = wxDefaultPosition, wxSize = wxDefaultSize, long style = 0);

	void set_text (wxString text) override;
	wxString get_text () const override;
};


#endif
