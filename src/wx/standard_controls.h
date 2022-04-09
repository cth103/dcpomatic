/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "controls.h"


class StandardControls : public Controls
{
public:
	StandardControls (wxWindow* parent, std::shared_ptr<FilmViewer> viewer, bool editor_controls);

	void play () override;
	void stop () override;

private:
	void check_play_state ();
	void play_clicked ();
	void started () override;
	void stopped () override;
	void setup_sensitivity () override;

	wxToggleButton* _play_button;
};
