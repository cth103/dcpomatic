#include "lib/dcpomatic_time.h"
#include "lib/types.h"
#include "lib/film.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class FilmViewer;
class Film;
class ClosedCaptionsDialog;
class Content;
class PlayerVideo;
class wxToggleButton;

class ControlFilmViewer : public wxPanel
{
public:
	ControlFilmViewer (wxWindow* parent, bool outline_content = true, bool jump_to_selected = true);

	void set_film (boost::shared_ptr<Film> film);
	void back_frame ();
	void forward_frame ();

	/* FilmViewer proxies */
	void set_position (DCPTime p);
	void set_position (boost::shared_ptr<Content> content, ContentTime p);
	void set_dcp_decode_reduction (boost::optional<int> reduction);
	void show_closed_captions ();
	void start ();
	bool stop ();
	bool playing () const;
	void slow_refresh ();
	int dropped () const;
	boost::shared_ptr<Film> film () const;
	boost::optional<int> dcp_decode_reduction () const;
	DCPTime position () const;
	void set_coalesce_player_changes (bool c);
	boost::signals2::signal<void (boost::weak_ptr<PlayerVideo>)> ImageChanged;

private:
	void update_position_label ();
	void update_position_slider ();
	void rewind_clicked (wxMouseEvent &);
	void back_clicked (wxKeyboardState& s);
	void forward_clicked (wxKeyboardState &);
	void slider_moved (bool page);
	void slider_released ();
	void play_clicked ();
	void frame_number_clicked ();
	void jump_to_selected_clicked ();
	void setup_sensitivity ();
	void timecode_clicked ();
	void check_play_state ();
	void active_jobs_changed (boost::optional<std::string>);
	DCPTime nudge_amount (wxKeyboardState& ev);
	void image_changed (boost::weak_ptr<PlayerVideo>);
	void film_change (ChangeType type, Film::Property p);
	void outline_content_changed ();
	void eye_changed ();
	void position_changed ();
	void started ();
	void stopped ();

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<FilmViewer> _viewer;

	wxSizer* _v_sizer;
	bool _slider_being_moved;
	bool _was_running_before_slider;

	wxCheckBox* _outline_content;
	wxChoice* _eye;
	wxCheckBox* _jump_to_selected;
	wxSlider* _slider;
	wxButton* _rewind_button;
	wxButton* _back_button;
	wxButton* _forward_button;
	wxStaticText* _frame_number;
	wxStaticText* _timecode;
	wxToggleButton* _play_button;

	ClosedCaptionsDialog* _closed_captions_dialog;
};
