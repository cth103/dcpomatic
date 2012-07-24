/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <iostream>
#include <gtkmm.h>
#include <cairomm/cairomm.h>
#include "alignment.h"

using namespace std;

class AlignmentWidget : public Gtk::DrawingArea
{
public:
	void set_text_line (int n, string t)
	{
		if (int(_text.size()) < (n + 1)) {
			_text.resize (n + 1);
		}
		
		_text[n] = t;
		queue_draw ();
	}
	
private:	
	bool on_expose_event (GdkEventExpose* ev)
	{
		if (!get_window ()) {
			return false;
		}
		
		Cairo::RefPtr<Cairo::Context> c = get_window()->create_cairo_context ();
		
		Gtk::Allocation a = get_allocation ();
		int const w = a.get_width ();
		int const h = a.get_height ();
		
		c->rectangle (0, 0, w, h);
		c->set_source_rgb (0, 0, 0);
		c->fill ();
		
		c->set_source_rgb (1, 1, 1);
		c->set_line_width (1);
		
		int const arrow_size = h / 8;
		int const head_size = h / 32;
		
		/* arrow to left edge */
		c->move_to (arrow_size, h / 2);
		c->line_to (0, h / 2);
		c->rel_line_to (head_size, head_size);
		c->move_to (0, h / 2);
		c->rel_line_to (head_size, -head_size);
		c->stroke ();
		
		/* arrow to right edge */
		c->move_to (w - arrow_size, h / 2);
		c->line_to (w, h / 2);
		c->rel_line_to (-head_size, head_size);
		c->move_to (w, h / 2);
		c->rel_line_to (-head_size, -head_size);
		c->stroke ();
		
		/* arrow to top edge */
		c->move_to (w / 2, arrow_size);
		c->line_to (w / 2, 0);
		c->rel_line_to (head_size, head_size);
		c->move_to (w / 2, 0);
		c->rel_line_to (-head_size, head_size);
		c->stroke ();
		
		/* arrow to bottom edge */
		c->move_to (w / 2, h - h / 8);
		c->line_to (w / 2, h);
		c->rel_line_to (head_size, -head_size);
		c->move_to (w / 2, h);
		c->rel_line_to (-head_size, -head_size);
		c->stroke ();
		
		/* arrow to top-left corner */
		c->move_to (arrow_size, arrow_size);
		c->line_to (0, 0);
		c->rel_line_to (head_size, 0);
		c->move_to (0, 0);
		c->rel_line_to (0, head_size);
		c->stroke ();
		
		/* arrow to top-right corner */
		c->move_to (w - arrow_size, arrow_size);
		c->line_to (w, 0);
		c->rel_line_to (0, head_size);
		c->move_to (w, 0);
		c->rel_line_to (-head_size, 0);
		c->stroke ();
		
		/* arrow to bottom-left corner */
		c->move_to (arrow_size, h - arrow_size);
		c->line_to (0, h);
		c->rel_line_to (head_size, 0);
		c->move_to (0, h);
		c->rel_line_to (0, -head_size);
		c->stroke ();
		
		/* arrow to bottom-right corner */
		c->move_to (w - arrow_size, h - arrow_size);
		c->line_to (w, h);
		c->rel_line_to (-head_size, 0);
		c->line_to (w, h);
		c->rel_line_to (0, -head_size);
		c->stroke ();
		
		/* text */
		int max_height = 0;
		for (vector<string>::iterator i = _text.begin(); i != _text.end(); ++i) {
			Cairo::TextExtents e;
			c->get_text_extents (*i, e);
			max_height = max (max_height, int (e.height));
		}
		
		int total_height = max_height * _text.size() * 2;
		
		for (vector<string>::size_type i = 0; i < _text.size(); ++i) {
			Cairo::TextExtents e;
			c->get_text_extents (_text[i], e);
			c->move_to ((w - e.width) / 2, ((h - total_height) / 2) + ((i * 2) + 1) *  max_height);
			c->text_path (_text[i]);
			c->stroke ();
		}
		
		return true;
	}

	std::vector<std::string> _text;
};

Alignment::Alignment (Position p, Size s)
{
	_widget = Gtk::manage (new AlignmentWidget);
	add (*_widget);
	show_all ();
	
	set_decorated (false);
	set_resizable (false);
	set_size_request (s.width, s.height);
	move (p.x, p.y);
}

void
Alignment::set_text_line (int n, string t)
{
	_widget->set_text_line (n, t);
}
