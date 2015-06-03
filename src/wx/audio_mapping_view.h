/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/wx/audio_mapping_view.h
 *  @brief AudioMappingView class.
 *
 */

#include <boost/signals2.hpp>
#include <wx/wx.h>
#include <wx/grid.h>
#include "lib/audio_mapping.h"

/** @class AudioMappingView
 *  @brief This class displays the mapping of one set of audio channels to another,
 *  with gain values on each node of the map.
 *
 *  The AudioMapping passed to set() describes the actual channel mapping,
 *  and the names passed to set_input_channels() and set_output_channels() are
 *  used to label the rows and columns.
 *
 *  The display's row count is equal to the AudioMapping's input channel count,
 *  and the column count is equal to the number of name passed to
 *  set_output_channels().  Any other output channels in the AudioMapping are
 *  hidden from view.  Thus input channels are never hidden but output channels
 *  might be.
 */

class AudioMappingView : public wxPanel
{
public:
	AudioMappingView (wxWindow *);

	void set (AudioMapping);
	void set_input_channels (std::vector<std::string> const & names);
	void set_output_channels (std::vector<std::string> const & names);

	boost::signals2::signal<void (AudioMapping)> Changed;

private:
	void left_click (wxGridEvent &);
	void right_click (wxGridEvent &);
	void mouse_moved (wxMouseEvent &);
	void update_cells ();
	void map_values_changed ();

	void off ();
	void full ();
	void minus6dB ();
	void edit ();

	wxGrid* _grid;
	wxSizer* _sizer;
	AudioMapping _map;

	wxMenu* _menu;
	int _menu_row;
	int _menu_column;

	int _last_tooltip_row;
	int _last_tooltip_column;
};
