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
class wxTreeCtrl;


class VerifyDCPResultPanel : public wxPanel
{
public:
	VerifyDCPResultPanel(wxWindow* parent);

	void add(std::vector<std::shared_ptr<const VerifyDCPJob>> job);

private:
	std::map<dcp::VerificationNote::Type, int> add(std::shared_ptr<const VerifyDCPJob> job, bool many);
	void save_text_report();
	void save_html_report();
	void save_pdf_report();

	wxStaticText* _summary;
	std::map<dcp::VerificationNote::Type, wxTreeCtrl*> _pages;
	Button* _save_text_report;
	Button* _save_html_report;
	Button* _save_pdf_report;

	std::vector<std::shared_ptr<const VerifyDCPJob>> _jobs;

	std::vector<dcp::VerificationNote::Type> _types;
};
