#include <gtkmm.h>
#include <string>

class MainWindow : public Gtk::Window
{
public:
	MainWindow ();

private:
	FilmView _film_view;
};
