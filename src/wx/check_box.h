/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CHECK_BOX_H
#define DCPOMATIC_CHECK_BOX_H


#include "i18n_hook.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


class CheckBox : public wxCheckBox, public I18NHook
{
public:
	CheckBox (wxWindow* parent, wxString label);

	void set_text (wxString text) override;
	wxString get_text () const override;
	bool get() const;
	void set(bool state);

	template <typename... Args>
	void bind(Args... args) {
		Bind(wxEVT_CHECKBOX, boost::bind(std::forward<Args>(args)...));
	}
};


#endif
