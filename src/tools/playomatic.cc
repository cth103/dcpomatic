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
#include "lib/util.h"
#include "gtk/film_player.h"
#include "gtk/film_list.h"

using namespace std;

static FilmPlayer* film_player = 0;

void
film_changed (Film const * f)
{
	film_player->set_film (f);
}

int
main (int argc, char* argv[])
{
	dvdomatic_setup ();

	Gtk::Main kit (argc, argv);

	if (argc != 2) {
		cerr << "Syntax: " << argv[0] << " <directory>\n";
		exit (EXIT_FAILURE);
	}

	Gtk::Window* window = new Gtk::Window ();

	FilmList* film_list = new FilmList (argv[1]);
	film_player = new FilmPlayer ();

	Gtk::HBox hbox;
	hbox.pack_start (film_list->widget(), true, true);
	hbox.pack_start (film_player->widget(), true, true);

	film_list->SelectionChanged.connect (sigc::ptr_fun (&film_changed));
	
	window->set_title ("Play-o-matic");
	window->add (hbox);
	window->show_all ();
	
	window->maximize ();
	Gtk::Main::run (*window);

	return 0;
}

