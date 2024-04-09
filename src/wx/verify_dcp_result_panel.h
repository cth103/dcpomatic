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


#include <dcp/verify.h>
#include <wx/wx.h>
#include <map>
#include <memory>


class Button;
class VerifyDCPJob;
class wxRichTextCtrl;


class VerifyDCPResultPanel : public wxPanel
{
public:
	VerifyDCPResultPanel(wxWindow* parent);

	void fill(std::shared_ptr<VerifyDCPJob> job);

private:
	void save_text_report();
	void save_html_report();

	wxStaticText* _summary;
	std::map<dcp::VerificationNote::Type, wxRichTextCtrl*> _pages;
	Button* _save_text_report;
	Button* _save_html_report;

	std::shared_ptr<VerifyDCPJob> _job;
};
