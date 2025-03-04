/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_INTEROP_METADATA_DIALOG_H
#define DCPOMATIC_INTEROP_METADATA_DIALOG_H


#include "metadata_dialog.h"
#include <dcp/language_tag.h>
#include <dcp/types.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <vector>


class Film;
class LanguageTagWidget;


class InteropMetadataDialog : public MetadataDialog
{
public:
	InteropMetadataDialog (wxWindow* parent, std::weak_ptr<Film> film);

private:
	void setup_standard (wxPanel* panel, wxSizer* sizer) override;

	void content_version_changed ();

	wxTextCtrl* _content_version;
};


#endif

