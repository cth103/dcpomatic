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


#include "config_dialog.h"


namespace dcpomatic {
namespace preferences {


class SoundPage : public Page
{
public:
	SoundPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override;
#endif

private:
	void setup() override;
	void config_changed() override;
        boost::optional<std::string> get_sound_output();
	void sound_changed();
	void sound_output_changed();
	void setup_sensitivity();
	void map_changed(AudioMapping m);
	void reset_to_default();

	CheckBox* _sound;
	wxChoice* _sound_output;
	wxStaticText* _sound_output_details;
	AudioMappingView* _map;
	Button* _reset_to_default;
};


}
}

