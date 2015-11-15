/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "download_certificate_panel.h"
#include <curl/curl.h>
#include <list>

class DolbyCertificatePanel : public DownloadCertificatePanel
{
public:
	DolbyCertificatePanel (wxWindow* parent, DownloadCertificateDialog* dialog);

	void setup ();
	bool ready_to_download () const;
	void download (wxStaticText* message);

private:
	void finish_download (wxStaticText* message);
	void setup_countries ();
	void finish_setup_countries ();
	void country_selected ();
	void finish_country_selected ();
	void cinema_selected ();
	void finish_cinema_selected ();
	std::list<std::string> get_dir (std::string) const;

	wxChoice* _country;
	wxChoice* _cinema;
	wxChoice* _serial;
};
