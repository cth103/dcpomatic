FilmView::FilmView ()
	: _film (0)
	, _content_file_radio (_content_group)
	, _content_file_chooser ("Content", Gtk::FILE_CHOOSER_ACTION_OPEN)
	, _content_file_button (_content_file_chooser)
	, _content_folder_radio (_content_group)
	, _content_folder_chooser ("Content", Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER)
	, _content_folder_button (_content_folder_chooser)
	, _dvd ("DVD")
	, _deinterlace ("Deinterlace")
{
	_table = manage (new Gtk::Table);
	table->set_row_spacings (4);
	table->set_col_spacings (12);
	main_vbox->pack_start (*table, false, false);

	int n = 0;

	table->attach (*left_aligned_label ("Content"), 0, 1, n, n + 1);
	_content_file_chooser.add_button ("Select", Gtk::RESPONSE_OK);
	_content_folder_chooser.add_button ("Select", Gtk::RESPONSE_OK);
	Gtk::HBox* b = Gtk::manage (new Gtk::HBox);
	b->set_spacing (12);
	_content_file_radio.set_label ("File");
	b->pack_start (_content_file_radio, false, false);
	b->pack_start (_content_file_button, true, true);
	_content_folder_radio.set_label ("Folder");
	b->pack_start (_content_folder_radio, false, false);
	b->pack_start (_content_folder_button, true, true);
	table->attach (*b, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Name"), 0, 1, n, n + 1);
	table->attach (_name, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Ratio"), 0, 1, n, n + 1);
	_ratio.append_text ("1.33:1 (4:3)");
	_ratio.append_text ("1.78:1 (16:9)");
	_ratio.append_text ("1.85:1 (Flat)");
	_ratio.append_text ("2.39:1 (Scope)");
	table->attach (_ratio, 1, 2, n, n + 1);
	_ratio.set_active_text ("1.85:1 (Flat)");
	++n;

	table->attach (_dvd, 0, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("DVD title"), 0, 1, n, n + 1);
	_dvd_title.set_range (1, 64);
	_dvd_title.set_increments (1, 4);
	_dvd_title.set_value (1);
	table->attach (_dvd_title, 1, 2, n, n + 1);
	++n;

	table->attach (_deinterlace, 0, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Left Crop"), 0, 1, n, n + 1);
	_left_crop.set_range (0, 1024);
	_left_crop.set_increments (1, 64);
	_left_crop.set_value (0);
	table->attach (_left_crop, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Right Crop"), 0, 1, n, n + 1);
	_right_crop.set_range (0, 1024);
	_right_crop.set_increments (1, 64);
	_right_crop.set_value (0);
	table->attach (_right_crop, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Top Crop"), 0, 1, n, n + 1);
	_top_crop.set_range (0, 1024);
	_top_crop.set_increments (1, 64);
	_top_crop.set_value (0);
	table->attach (_top_crop, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Bottom Crop"), 0, 1, n, n + 1);
	_bottom_crop.set_range (0, 1024);
	_bottom_crop.set_increments (1, 64);
	_bottom_crop.set_value (0);
	table->attach (_bottom_crop, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Size"), 0, 1, n, n + 1);
	table->attach (_size, 1, 2, n, n + 1);
	++n;

	table->attach (*left_aligned_label ("Length"), 0, 1, n, n + 1);
	table->attach (_length, 1, 2, n, n + 1);
	++n;
	
	/* We only need to connect to one of the radios in the group */
	_content_file_radio.signal_toggled().connect (sigc::mem_fun (*this, &FilmView::content_radio_toggled));
	_content_file_button.signal_file_set().connect (sigc::mem_fun (*this, &FilmView::content_changed));
	_content_folder_button.signal_file_set().connect (sigc::mem_fun (*this, &FilmView::content_changed));
	_dvd.signal_toggled().connect (sigc::mem_fun (*this, &FilmView::update_dvd_title_sensitivity));

	update_content_radio_sensitivity ();
	update_dvd_title_sensitivity ();
	show_all ();
}

Gtk::Label *
FilmView::left_aligned_label (string const & t) const
{
	Gtk::Label* l = Gtk::manage (new Gtk::Label (t));
	l->set_alignment (0, 0.5);
	return l;
}

void
FilmView::content_radio_toggled ()
{
	update_content_radio_sensitivity ();
	content_changed ();
}

void
FilmView::update_content_radio_sensitivity ()
{
	_content_file_button.set_sensitive (_content_file_radio.get_active ());
	_content_folder_button.set_sensitive (_content_folder_radio.get_active ());
}

void
FilmView::update_dvd_title_sensitivity ()
{
	_dvd_title.set_sensitive (_dvd.get_active ());
}

void
FilmView::content_changed ()
{
	cout << "Content changed.\n";
}

void
FilmView::set_film (Film* f)
{
	_film = f;
	update ();
}
