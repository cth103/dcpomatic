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


#include <dcp/decrypted_kdm.h>
#include <dcp/encrypted_kdm.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>
#include <memory>


class Button;
class CheckBox;
class VerifyDCPJob;
class VerifyDCPProgressPanel;
class VerifyDCPResultPanel;


class VerifyDCPDialog : public wxDialog
{
public:
	VerifyDCPDialog(
		wxWindow* parent,
		wxString title,
		std::vector<boost::filesystem::path> dcp_directories,
		std::vector<dcp::EncryptedKDM> const& kdms
	);

	VerifyDCPDialog(
		wxWindow* parent,
		wxString title,
		std::vector<boost::filesystem::path> dcp_directories,
		std::vector<boost::filesystem::path> kdms
	);

private:
	void setup();
	void verify_clicked();
	void cancel_clicked();

	CheckBox* _check_picture_details;
	VerifyDCPProgressPanel* _progress_panel;
	VerifyDCPResultPanel* _result_panel;
	Button* _cancel;
	Button* _verify;
	bool _cancel_pending;
	std::vector<boost::filesystem::path> _dcp_directories;
	std::vector<dcp::DecryptedKDM> _kdms;
};


