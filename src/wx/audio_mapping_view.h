/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

/** @file  src/wx/audio_mapping_view.h
 *  @brief AudioMappingView class.
 *
 */

#include <boost/signals2.hpp>
#include <wx/wx.h>
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
	explicit AudioMappingView (wxWindow *, wxString left_label, wxString from, wxString top_label, wxString to);

	void set (AudioMapping);
	void set_input_channels (std::vector<std::string> const & names);
	void set_output_channels (std::vector<std::string> const & names);

	struct Group
	{
		Group (int f, int t, std::string n)
			: from (f)
			, to (t)
			, name (n)
		{}

		/** First channel index (from 0) */
		int from;
		/** Last channel index (from 0) */
		int to;
		/** Name of this group */
		std::string name;
	};

	void set_input_groups (std::vector<Group> const & groups);

	boost::signals2::signal<void (AudioMapping)> Changed;

private:
	void map_values_changed ();
	void setup_sizes ();
	void paint ();
	void paint_static (wxDC& dc, wxGraphicsContext* gc);
	void paint_column_labels (wxDC& dc, wxGraphicsContext* gc);
	void paint_column_lines (wxGraphicsContext* gc);
	void paint_row_labels (wxDC& dc, wxGraphicsContext* gc);
	void paint_row_lines (wxGraphicsContext* gc);
	void paint_indicators (wxDC& dc);
	void size (wxSizeEvent &);
	void scroll ();
	void left_down (wxMouseEvent &);
	void right_down (wxMouseEvent &);
	void motion (wxMouseEvent &);
	void mouse_wheel (wxMouseEvent &);
	boost::optional<std::pair<int, int> > mouse_event_to_channels (wxMouseEvent& ev) const;
	boost::optional<std::string> mouse_event_to_input_group_name (wxMouseEvent& ev) const;
	void setup ();
	wxString safe_input_channel_name (int n) const;
	wxString safe_output_channel_name (int n) const;

	void set_gain_from_menu (double linear);
	void edit ();

	AudioMapping _map;

	wxMenu* _menu;
	wxPanel* _body;
	wxScrollBar* _vertical_scroll;
	wxScrollBar* _horizontal_scroll;
	int _menu_input;
	int _menu_output;

	wxString _left_label;
	wxString _from;
	wxString _top_label;
	wxString _to;

	std::vector<std::string> _input_channels;
	std::vector<std::string> _output_channels;
	std::vector<Group> _input_groups;

	boost::optional<std::pair<int, int> > _last_tooltip_channels;
};
