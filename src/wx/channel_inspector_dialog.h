/*
    Copyright (C) 2026

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


#ifndef DCPOMATIC_CHANNEL_INSPECTOR_DIALOG_H
#define DCPOMATIC_CHANNEL_INSPECTOR_DIALOG_H


#include "film_viewer.h"
#include "wx_util.h"
#include "lib/film.h"
#include "lib/util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <algorithm>
#include <list>
#include <vector>


class ChannelInspectorDialog : public wxFrame
{
public:
	ChannelInspectorDialog(wxWindow* parent, FilmViewer& viewer)
		: wxFrame(parent, wxID_ANY, _("Channel inspector"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT)
		, _viewer(viewer)
		, _timer(this)
	{
		_sizer = new wxBoxSizer(wxVERTICAL);
		_grid = new wxFlexGridSizer(4, 4, 12);
		_sizer->Add(_grid, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		auto clear = new wxButton(this, wxID_ANY, _("Clear solo/mute"));
		clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { clear_clicked(); });
		_sizer->Add(clear, 0, wxLEFT | wxRIGHT | wxBOTTOM, DCPOMATIC_DIALOG_BORDER);

		SetSizerAndFit(_sizer);

		Bind(wxEVT_TIMER, [this](wxTimerEvent&) { on_timer(); });
		Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& ev) {
			_timer.Stop();
			_viewer.inspector_set_active(false);
			Hide();
			ev.Veto();
		});
	}

	void open()
	{
		_viewer.inspector_set_active(true);
		rebuild_rows();
		_timer.Start(100);
		wxFrame::Show(true);
		Raise();
	}

private:
	void rebuild_rows()
	{
		_grid->Clear(true);
		_solo.clear();
		_mute.clear();
		_peak.clear();

		add_header(_("Channel"));
		add_header(_("Solo"));
		add_header(_("Mute"));
		add_header(_("Peak"));

		std::list<int> mapped;
		if (auto film = _viewer.film()) {
			mapped = film->mapped_audio_channels();
		}

		_last_dcp_channels = _viewer.inspector_dcp_channels();
		for (int c = 0; c < _last_dcp_channels; ++c) {
			auto const is_mapped = std::find(mapped.begin(), mapped.end(), c) != mapped.end();
			if (!is_mapped) {
				if (_viewer.inspector_solo(c)) {
					_viewer.inspector_set_solo(c, false);
				}
				if (_viewer.inspector_mute(c)) {
					_viewer.inspector_set_mute(c, false);
				}
			}

			auto label_text = wxString::Format(wxT("%d  "), c) + std_to_wx(short_audio_channel_name(c));
			if (!is_mapped) {
				label_text += wxT("  (unmapped)");
			}

			auto label = new wxStaticText(this, wxID_ANY, label_text);
			if (!is_mapped) {
				label->Enable(false);
			}

			auto solo = new wxCheckBox(this, wxID_ANY, wxT(""));
			auto mute = new wxCheckBox(this, wxID_ANY, wxT(""));
			auto peak = new wxStaticText(this, wxID_ANY, wxT("-inf dB"));

			solo->SetValue(is_mapped && _viewer.inspector_solo(c));
			mute->SetValue(is_mapped && _viewer.inspector_mute(c));
			solo->Enable(is_mapped);
			mute->Enable(is_mapped);
			peak->Enable(is_mapped);

			solo->Bind(wxEVT_CHECKBOX, [this, c](wxCommandEvent& ev) { _viewer.inspector_set_solo(c, ev.IsChecked()); });
			mute->Bind(wxEVT_CHECKBOX, [this, c](wxCommandEvent& ev) { _viewer.inspector_set_mute(c, ev.IsChecked()); });

			_grid->Add(label, 0, wxALIGN_CENTER_VERTICAL);
			_grid->Add(solo, 0, wxALIGN_CENTER);
			_grid->Add(mute, 0, wxALIGN_CENTER);
			_grid->Add(peak, 0, wxALIGN_CENTER_VERTICAL);

			_solo.push_back(solo);
			_mute.push_back(mute);
			_peak.push_back(peak);
		}

		_sizer->Layout();
		Fit();
	}

	void add_header(wxString const& text)
	{
		auto header = new wxStaticText(this, wxID_ANY, text);
		auto font = header->GetFont();
		font.SetWeight(wxFONTWEIGHT_BOLD);
		header->SetFont(font);
		_grid->Add(header, 0, wxALIGN_CENTER_VERTICAL);
	}

	void clear_clicked()
	{
		_viewer.inspector_clear();
		for (auto button: _solo) {
			button->SetValue(false);
		}
		for (auto button: _mute) {
			button->SetValue(false);
		}
	}

	void on_timer()
	{
		if (_viewer.inspector_dcp_channels() != _last_dcp_channels) {
			rebuild_rows();
		}

		for (size_t c = 0; c < _peak.size(); ++c) {
			auto const db = _viewer.inspector_peak_dbfs(static_cast<int>(c));
			_peak[c]->SetLabel(db <= -119.0f ? wxString(wxT("-inf dB")) : wxString::Format(wxT("%.1f dB"), db));
		}
	}

	FilmViewer& _viewer;
	wxTimer _timer;
	wxBoxSizer* _sizer = nullptr;
	wxFlexGridSizer* _grid = nullptr;
	std::vector<wxCheckBox*> _solo;
	std::vector<wxCheckBox*> _mute;
	std::vector<wxStaticText*> _peak;
	int _last_dcp_channels = -1;
};


#endif
