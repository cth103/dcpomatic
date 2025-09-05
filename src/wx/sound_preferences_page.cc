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


#include "audio_backend.h"
#include "audio_mapping_view.h"
#include "check_box.h"
#include "dcpomatic_button.h"
#include "dcpomatic_spin_ctrl.h"
#include "sound_preferences_page.h"
#include "wx_util.h"
#include "lib/constants.h"
#include "lib/util.h"
#include <fmt/format.h>


using std::map;
using std::string;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::preferences;


SoundPage::SoundPage(wxSize panel_size, int border, Purpose purpose)
	: Page(panel_size, border)
	, _purpose(purpose)
{

}


#ifdef DCPOMATIC_OSX
wxBitmap
SoundPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("sound"), wxBITMAP_TYPE_PNG);
}
#endif


wxString
SoundPage::GetName() const
{
	return _("Sound");
}

void
SoundPage::setup()
{
	auto table = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

	int r = 0;

	_sound = new CheckBox(_panel, _("Play sound via"));
	table->Add(_sound, wxGBPosition(r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	wxBoxSizer* s = new wxBoxSizer(wxHORIZONTAL);
	_sound_output = new wxChoice(_panel, wxID_ANY);
	s->Add(_sound_output, 0);
	_sound_output_details = new wxStaticText(_panel, wxID_ANY, {});
	s->Add(_sound_output_details, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, DCPOMATIC_SIZER_X_GAP);
	table->Add(s, wxGBPosition(r, 1));
	++r;

	if (_purpose == Purpose::PLAYER) {
		add_label_to_sizer(table, _panel, _("Delay audio by"), true, wxGBPosition(r, 0));
		auto s = new wxBoxSizer(wxHORIZONTAL);
		_delay = new SpinCtrl(_panel, -1000, 1000);
		s->Add(_delay, 0);
		s->Add(new wxStaticText(_panel, wxID_ANY, _("ms")), 1, wxALIGN_CENTER_VERTICAL | wxLEFT, DCPOMATIC_SIZER_X_GAP);
		table->Add(s, wxGBPosition(r, 1));
		++r;
	}

	add_label_to_sizer(table, _panel, _("Mapping"), true, wxGBPosition(r, 0));
	_map = new AudioMappingView(_panel, _("DCP"), _("DCP"), _("Output"), _("output"));
	table->Add(_map, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	_reset_to_default = new Button(_panel, _("Reset to default"));
	table->Add(_reset_to_default, wxGBPosition(r, 1));
	++r;

	wxFont font = _sound_output_details->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_sound_output_details->SetFont(font);

	for (auto name: AudioBackend::instance()->output_device_names()) {
		_sound_output->Append(std_to_wx(name));
	}

	_sound->bind(&SoundPage::sound_changed, this);
	_sound_output->Bind(wxEVT_CHOICE, bind(&SoundPage::sound_output_changed, this));
	if (_delay) {
		_delay->bind(&SoundPage::delay_changed, this);
	}
	_map->Changed.connect(bind(&SoundPage::map_changed, this, _1));
	_reset_to_default->Bind(wxEVT_BUTTON, bind(&SoundPage::reset_to_default, this));
}

void
SoundPage::reset_to_default()
{
	Config::instance()->set_audio_mapping_to_default();
}

void
SoundPage::map_changed(AudioMapping m)
{
	Config::instance()->set_audio_mapping(m);
}

void
SoundPage::sound_changed()
{
	Config::instance()->set_sound(_sound->GetValue());
}

void
SoundPage::sound_output_changed()
{
	auto const so = get_sound_output();
	auto default_device = AudioBackend::instance()->default_device_name();

	if (!so || so == default_device) {
		Config::instance()->unset_sound_output();
	} else {
		Config::instance()->set_sound_output(*so);
	}
}


void
SoundPage::delay_changed()
{
	Config::instance()->set_player_audio_delay(_delay->get());
}


void
SoundPage::config_changed()
{
	auto config = Config::instance();

	checked_set(_sound, config->sound());

	auto const current_so = get_sound_output();
	optional<string> configured_so;

	auto& audio = AudioBackend::instance()->rtaudio();

	if (config->sound_output()) {
		configured_so = config->sound_output().get();
	} else {
		/* No configured output means we should use the default */
		configured_so = AudioBackend::instance()->default_device_name();
	}

	if (configured_so && current_so != configured_so) {
		/* Update _sound_output with the configured value */
		unsigned int i = 0;
		while (i < _sound_output->GetCount()) {
			if (_sound_output->GetString(i) == std_to_wx(*configured_so)) {
				_sound_output->SetSelection(i);
				break;
			}
			++i;
		}
	}

	map<int, wxString> apis;
	apis[RtAudio::MACOSX_CORE]    = _("CoreAudio");
	apis[RtAudio::WINDOWS_ASIO]   = _("ASIO");
	apis[RtAudio::WINDOWS_DS]     = _("Direct Sound");
	apis[RtAudio::WINDOWS_WASAPI] = _("WASAPI");
	apis[RtAudio::UNIX_JACK]      = _("JACK");
	apis[RtAudio::LINUX_ALSA]     = _("ALSA");
	apis[RtAudio::LINUX_PULSE]    = _("PulseAudio");
	apis[RtAudio::LINUX_OSS]      = _("OSS");
	apis[RtAudio::RTAUDIO_DUMMY]  = _("Dummy");

	int const channels = configured_so ? AudioBackend::instance()->device_output_channels(*configured_so).get_value_or(0) : 0;

	_sound_output_details->SetLabel(
		wxString::Format(_("%d channels on %s"), channels, apis[audio.getCurrentApi()])
		);

	_map->set(Config::instance()->audio_mapping(channels));

	vector<NamedChannel> input;
	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		input.push_back(NamedChannel(short_audio_channel_name(i), i));
	}
	_map->set_input_channels(input);

	vector<NamedChannel> output;
	for (int i = 0; i < channels; ++i) {
		output.push_back(NamedChannel(fmt::to_string(i + 1), i));
	}
	_map->set_output_channels(output);

	setup_sensitivity();
}

void
SoundPage::setup_sensitivity()
{
	_sound_output->Enable(_sound->GetValue());
}

/** @return Currently-selected preview sound output in the dialogue */
optional<string>
SoundPage::get_sound_output()
{
	int const sel = _sound_output->GetSelection();
	if (sel == wxNOT_FOUND) {
		return {};
	}

	return wx_to_std(_sound_output->GetString(sel));
}


