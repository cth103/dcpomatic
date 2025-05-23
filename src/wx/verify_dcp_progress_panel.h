/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <memory>


class VerifyDCPJob;


class VerifyDCPProgressPanel : public wxPanel
{
public:
	VerifyDCPProgressPanel(wxWindow* parent);

	void update(std::shared_ptr<const VerifyDCPJob> job);
	void clear();

private:
	wxStaticText* _directory_name;
	wxStaticText* _job_name;
	wxStaticText* _file_name;
	wxGauge* _progress;
};

