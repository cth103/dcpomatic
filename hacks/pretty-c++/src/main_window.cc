#include "main_window.h"
#include <iostream>

using namespace std;

MainWindow::MainWindow ()
{
	set_title ("DVD-o-matic");
	maximize ();

	Gtk::VBox* main_vbox = manage (new Gtk::VBox);
	main_vbox->set_border_width (12);
	add (*main_vbox);
	
	main_vbox->add (*_film_view.widget ());
}
