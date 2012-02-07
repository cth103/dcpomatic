#include "main_window.h"

int main (int argc, char* argv[])
{
	Gtk::Main kit (argc, argv);
	
	MainWindow window;
	Gtk::Main::run (window);

	return 0;
}
