/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_CONFIG_DIALOG_H
#define DCPOMATIC_CONFIG_DIALOG_H

#include "wx_util.h"
#include "editable_list.h"
#include "make_chain_dialog.h"
#include "lib/config.h"
#include "lib/ratio.h"
#include "lib/filter.h"
#include "lib/dcp_content_type.h"
#include "lib/log.h"
#include "lib/util.h"
#include "lib/cross.h"
#include "lib/exceptions.h"
#include <dcp/locale_convert.h>
#include <dcp/exceptions.h>
#include <dcp/certificate_chain.h>
#include <wx/stdpaths.h>
#include <wx/preferences.h>
#include <wx/spinctrl.h>
#include <wx/filepicker.h>
#include <RtAudio.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

class Page
{
public:
	Page (wxSize panel_size, int border);
	virtual ~Page () {}

protected:
	wxWindow* create_window (wxWindow* parent);

	int _border;
	wxPanel* _panel;

private:
	virtual void config_changed () = 0;
	virtual void setup () = 0;

	void config_changed_wrapper ();
	void window_destroyed ();

	wxSize _panel_size;
	boost::signals2::scoped_connection _config_connection;
	bool _window_exists;
};

class StockPage : public wxStockPreferencesPage, public Page
{
public:
	StockPage (Kind kind, wxSize panel_size, int border);
	wxWindow* CreateWindow (wxWindow* parent);
};

class StandardPage : public wxPreferencesPage, public Page
{
public:
	StandardPage (wxSize panel_size, int border);
	wxWindow* CreateWindow (wxWindow* parent);
};

class GeneralPage : public StockPage
{
public:
	GeneralPage (wxSize panel_size, int border);

protected:
	void add_language_controls (wxGridBagSizer* table, int& r);
	void add_play_sound_controls (wxGridBagSizer* table, int& r);
	void add_update_controls (wxGridBagSizer* table, int& r);
	virtual void config_changed ();

private:
	void setup_sensitivity ();
        boost::optional<std::string> get_sound_output ();
	void set_language_changed ();
	void language_changed ();
	void check_for_updates_changed ();
	void check_for_test_updates_changed ();
	void sound_changed ();
        void sound_output_changed ();

	wxCheckBox* _set_language;
	wxChoice* _language;
	wxCheckBox* _sound;
	wxChoice* _sound_output;
	wxCheckBox* _check_for_updates;
	wxCheckBox* _check_for_test_updates;
};

class CertificateChainEditor : public wxDialog
{
public:
	CertificateChainEditor (
		wxWindow* parent,
		wxString title,
		int border,
		boost::function<void (boost::shared_ptr<dcp::CertificateChain>)> set,
		boost::function<boost::shared_ptr<const dcp::CertificateChain> (void)> get,
		boost::function<void (void)> nag_remake
		);

	void add_button (wxWindow* button);

private:
	void add_certificate ();
	void remove_certificate ();
	void export_certificate ();
	void update_certificate_list ();
	void remake_certificates ();
	void update_sensitivity ();
	void update_private_key ();
	void import_private_key ();
	void export_private_key ();

	wxListCtrl* _certificates;
	wxButton* _add_certificate;
	wxButton* _export_certificate;
	wxButton* _remove_certificate;
	wxButton* _remake_certificates;
	wxStaticText* _private_key;
	wxButton* _import_private_key;
	wxButton* _export_private_key;
	wxStaticText* _private_key_bad;
	wxSizer* _sizer;
	wxBoxSizer* _button_sizer;
	boost::function<void (boost::shared_ptr<dcp::CertificateChain>)> _set;
	boost::function<boost::shared_ptr<const dcp::CertificateChain> (void)> _get;
	boost::function<void (void)> _nag_remake;
};

class KeysPage : public StandardPage
{
public:
	KeysPage (wxSize panel_size, int border, bool sign)
		: StandardPage (panel_size, border)
		, _sign (sign)
	{}

	wxString GetName () const;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const
	{
		return wxBitmap ("keys", wxBITMAP_TYPE_PNG_RESOURCE);
	}
#endif

private:

	void setup ();

	void export_decryption_certificate ();
	void export_decryption_chain ();
	void config_changed () {}
	void nag_remake_decryption_chain ();
	void decryption_advanced ();
	void signing_advanced ();
	void export_decryption_chain_and_key ();
	void import_decryption_chain_and_key ();

	bool _sign;
};


#endif
