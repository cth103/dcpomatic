/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
#include "lib/isdcf_metadata.h"
#include <wx/textctrl.h>
#include <boost/shared_ptr.hpp>

class wxSpinCtrl;
class Film;

class ISDCFMetadataDialog : public TableDialog
{
public:
	ISDCFMetadataDialog (wxWindow *, ISDCFMetadata, bool threed);

	ISDCFMetadata isdcf_metadata () const;

private:
	wxSpinCtrl* _content_version;
	wxTextCtrl* _audio_language;
	wxTextCtrl* _subtitle_language;
	wxTextCtrl* _territory;
	wxTextCtrl* _rating;
	wxTextCtrl* _studio;
	wxTextCtrl* _facility;
	wxCheckBox* _temp_version;
	wxCheckBox* _pre_release;
	wxCheckBox* _red_band;
	wxTextCtrl* _chain;
	wxCheckBox* _two_d_version_of_three_d;
	wxTextCtrl* _mastered_luminance;
};
