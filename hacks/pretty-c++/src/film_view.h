class FilmView
{
public:
	FilmView ();

	void set_film (Film* f);

	Gtk::Widget* widget ();
	
private:
	Gtk::Label* left_aligned_label (std::string const &) const;
	void content_changed ();
	void content_radio_toggled ();
	void update_content_radio_sensitivity ();
	void update_dvd_title_sensitivity ();

	Film* _film;

	Gtk::RadioButtonGroup _content_group;
	Gtk::RadioButton _content_file_radio;
	Gtk::FileChooserDialog _content_file_chooser;
	Gtk::FileChooserButton _content_file_button;
	Gtk::RadioButton _content_folder_radio;
	Gtk::FileChooserDialog _content_folder_chooser;
	Gtk::FileChooserButton _content_folder_button;
	Gtk::Entry _name;
	Gtk::ComboBoxText _ratio;
	Gtk::CheckButton _dvd;
	Gtk::CheckButton _deinterlace;
	Gtk::SpinButton _dvd_title;
	Gtk::SpinButton _left_crop;
	Gtk::SpinButton _right_crop;
	Gtk::SpinButton _top_crop;
	Gtk::SpinButton _bottom_crop;
	Gtk::Label _size;
	Gtk::Label _length;
};
