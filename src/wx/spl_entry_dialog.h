/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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
#include "lib/spl.h"


class RatioPicker;


class SPLEntryDialog : public TableDialog
{
public:
	SPLEntryDialog(wxWindow* parent, SPLEntry entry);

	SPLEntry get() const;

private:
	void crop_changed(boost::optional<float> ratio);

	SPLEntry _entry;

	RatioPicker* _crop;
};
