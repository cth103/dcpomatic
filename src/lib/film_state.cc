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

/** @file src/film_state.cc
 *  @brief The state of a Film.  This is separate from Film so that
 *  state can easily be copied and kept around for reference
 *  by long-running jobs.  This avoids the jobs getting confused
 *  by the user changing Film settings during their run.
 */

#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>
#include "film_state.h"
#include "scaler.h"
#include "filter.h"
#include "format.h"
#include "dcp_content_type.h"
#include "util.h"
#include "exceptions.h"
#include "options.h"
#include "decoder.h"
#include "decoder_factory.h"

using namespace std;
using namespace boost;

/** Write state to our `metadata' file */
void
FilmState::write_metadata () const
{
	filesystem::create_directories (directory());

	string const m = file ("metadata");
	ofstream f (m.c_str ());
	if (!f.good ()) {
		throw CreateFileError (m);
	}

	/* User stuff */
	f << "name " << _name << "\n";
	f << "use_dci_name " << _use_dci_name << "\n";
	f << "content " << _content << "\n";
	if (_dcp_content_type) {
		f << "dcp_content_type " << _dcp_content_type->pretty_name () << "\n";
	}
	if (_format) {
		f << "format " << _format->as_metadata () << "\n";
	}
	f << "left_crop " << _crop.left << "\n";
	f << "right_crop " << _crop.right << "\n";
	f << "top_crop " << _crop.top << "\n";
	f << "bottom_crop " << _crop.bottom << "\n";
	for (vector<Filter const *>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		f << "filter " << (*i)->id () << "\n";
	}
	f << "scaler " << _scaler->id () << "\n";
	f << "dcp_frames " << _dcp_frames << "\n";

	f << "dcp_trim_action ";
	switch (_dcp_trim_action) {
	case CUT:
		f << "cut\n";
		break;
	case BLACK_OUT:
		f << "black_out\n";
		break;
	}
	
	f << "dcp_ab " << (_dcp_ab ? "1" : "0") << "\n";
	f << "selected_audio_stream " << _audio_stream << "\n";
	f << "audio_gain " << _audio_gain << "\n";
	f << "audio_delay " << _audio_delay << "\n";
	f << "still_duration " << _still_duration << "\n";
	f << "selected_subtitle_stream " << _subtitle_stream << "\n";
	f << "with_subtitles " << _with_subtitles << "\n";
	f << "subtitle_offset " << _subtitle_offset << "\n";
	f << "subtitle_scale " << _subtitle_scale << "\n";
	f << "audio_language " << _audio_language << "\n";
	f << "subtitle_language " << _subtitle_language << "\n";
	f << "territory " << _territory << "\n";
	f << "rating " << _rating << "\n";
	f << "studio " << _studio << "\n";
	f << "facility " << _facility << "\n";
	f << "package_type " << _package_type << "\n";

	/* Cached stuff; this is information about our content; we could
	   look it up each time, but that's slow.
	*/
	for (vector<int>::const_iterator i = _thumbs.begin(); i != _thumbs.end(); ++i) {
		f << "thumb " << *i << "\n";
	}
	f << "width " << _size.width << "\n";
	f << "height " << _size.height << "\n";
	f << "length " << _length << "\n";
	f << "audio_sample_rate " << _audio_sample_rate << "\n";
	f << "content_digest " << _content_digest << "\n";
	f << "has_subtitles " << _has_subtitles << "\n";

	for (vector<AudioStream>::const_iterator i = _audio_streams.begin(); i != _audio_streams.end(); ++i) {
		f << "audio_stream " << i->to_string () << "\n";
	}

	for (vector<SubtitleStream>::const_iterator i = _subtitle_streams.begin(); i != _subtitle_streams.end(); ++i) {
		f << "subtitle_stream " << i->to_string () << "\n";
	}

	f << "frames_per_second " << _frames_per_second << "\n";
	
	_dirty = false;
}

/** Read state from our metadata file */
void
FilmState::read_metadata ()
{
	ifstream f (file("metadata").c_str());
	multimap<string, string> kv = read_key_value (f);
	for (multimap<string, string>::const_iterator i = kv.begin(); i != kv.end(); ++i) {
		string const k = i->first;
		string const v = i->second;

		/* User-specified stuff */
		if (k == "name") {
			_name = v;
		} else if (k == "use_dci_name") {
			_use_dci_name = (v == "1");
		} else if (k == "content") {
			_content = v;
		} else if (k == "dcp_content_type") {
			_dcp_content_type = DCPContentType::from_pretty_name (v);
		} else if (k == "format") {
			_format = Format::from_metadata (v);
		} else if (k == "left_crop") {
			_crop.left = atoi (v.c_str ());
		} else if (k == "right_crop") {
			_crop.right = atoi (v.c_str ());
		} else if (k == "top_crop") {
			_crop.top = atoi (v.c_str ());
		} else if (k == "bottom_crop") {
			_crop.bottom = atoi (v.c_str ());
		} else if (k == "filter") {
			_filters.push_back (Filter::from_id (v));
		} else if (k == "scaler") {
			_scaler = Scaler::from_id (v);
		} else if (k == "dcp_frames") {
			_dcp_frames = atoi (v.c_str ());
		} else if (k == "dcp_trim_action") {
			if (v == "cut") {
				_dcp_trim_action = CUT;
			} else if (v == "black_out") {
				_dcp_trim_action = BLACK_OUT;
			}
		} else if (k == "dcp_ab") {
			_dcp_ab = (v == "1");
		} else if (k == "selected_audio_stream") {
			_audio_stream = atoi (v.c_str ());
		} else if (k == "audio_gain") {
			_audio_gain = atof (v.c_str ());
		} else if (k == "audio_delay") {
			_audio_delay = atoi (v.c_str ());
		} else if (k == "still_duration") {
			_still_duration = atoi (v.c_str ());
		} else if (k == "selected_subtitle_stream") {
			_subtitle_stream = atoi (v.c_str ());
		} else if (k == "with_subtitles") {
			_with_subtitles = (v == "1");
		} else if (k == "subtitle_offset") {
			_subtitle_offset = atoi (v.c_str ());
		} else if (k == "subtitle_scale") {
			_subtitle_scale = atof (v.c_str ());
		} else if (k == "audio_language") {
			_audio_language = v;
		} else if (k == "subtitle_language") {
			_subtitle_language = v;
		} else if (k == "territory") {
			_territory = v;
		} else if (k == "rating") {
			_rating = v;
		} else if (k == "studio") {
			_studio = v;
		} else if (k == "facility") {
			_facility = v;
		} else if (k == "package_type") {
			_package_type = v;
		}
		
		/* Cached stuff */
		if (k == "thumb") {
			int const n = atoi (v.c_str ());
			/* Only add it to the list if it still exists */
			if (filesystem::exists (thumb_file_for_frame (n))) {
				_thumbs.push_back (n);
			}
		} else if (k == "width") {
			_size.width = atoi (v.c_str ());
		} else if (k == "height") {
			_size.height = atoi (v.c_str ());
		} else if (k == "length") {
			_length = atof (v.c_str ());
		} else if (k == "audio_sample_rate") {
			_audio_sample_rate = atoi (v.c_str ());
		} else if (k == "content_digest") {
			_content_digest = v;
		} else if (k == "has_subtitles") {
			_has_subtitles = (v == "1");
		} else if (k == "audio_stream") {
			_audio_streams.push_back (AudioStream (v));
		} else if (k == "subtitle_stream") {
			_subtitle_streams.push_back (SubtitleStream (v));
		} else if (k == "frames_per_second") {
			_frames_per_second = atof (v.c_str ());
		}
	}
		
	_dirty = false;
}

/** @param n A thumb index.
 *  @return The path to the thumb's image file.
 */
string
FilmState::thumb_file (int n) const
{
	return thumb_file_for_frame (thumb_frame (n));
}

/** @param n A frame index within the Film.
 *  @return The path to the thumb's image file for this frame;
 *  we assume that it exists.
 */
string
FilmState::thumb_file_for_frame (int n) const
{
	return thumb_base_for_frame(n) + ".png";
}

string
FilmState::thumb_base (int n) const
{
	return thumb_base_for_frame (thumb_frame (n));
}

string
FilmState::thumb_base_for_frame (int n) const
{
	stringstream s;
	s.width (8);
	s << setfill('0') << n;
	
	filesystem::path p;
	p /= dir ("thumbs");
	p /= s.str ();
		
	return p.string ();
}


/** @param n A thumb index.
 *  @return The frame within the Film that it is for.
 */
int
FilmState::thumb_frame (int n) const
{
	assert (n < int (_thumbs.size ()));
	return _thumbs[n];
}

Size
FilmState::cropped_size (Size s) const
{
	s.width -= _crop.left + _crop.right;
	s.height -= _crop.top + _crop.bottom;
	return s;
}

/** Given a directory name, return its full path within the Film's directory.
 *  The directory (and its parents) will be created if they do not exist.
 */
string
FilmState::dir (string d) const
{
	filesystem::path p;
	p /= _directory;
	p /= d;
	filesystem::create_directories (p);
	return p.string ();
}

/** Given a file or directory name, return its full path within the Film's directory */
string
FilmState::file (string f) const
{
	filesystem::path p;
	p /= _directory;
	p /= f;
	return p.string ();
}

/** @return full path of the content (actual video) file
 *  of the Film.
 */
string
FilmState::content_path () const
{
	if (filesystem::path(_content).has_root_directory ()) {
		return _content;
	}

	return file (_content);
}

ContentType
FilmState::content_type () const
{
#if BOOST_FILESYSTEM_VERSION == 3
	string ext = filesystem::path(_content).extension().string();
#else
	string ext = filesystem::path(_content).extension();
#endif

	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	
	if (ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
		return STILL;
	}

	return VIDEO;
}

int
FilmState::target_sample_rate () const
{
	/* Resample to a DCI-approved sample rate */
	double t = dcp_audio_sample_rate (_audio_sample_rate);

	/* Compensate for the fact that video will be rounded to the
	   nearest integer number of frames per second.
	*/
	if (rint (_frames_per_second) != _frames_per_second) {
		t *= _frames_per_second / rint (_frames_per_second);
	}

	return rint (t);
}

int
FilmState::dcp_length () const
{
	if (_dcp_frames) {
		return _dcp_frames;
	}

	return _length;
}

/** @return a DCI-compliant name for a DCP of this film */
string
FilmState::dci_name () const
{
	stringstream d;

	string fixed_name = to_upper_copy (_name);
	for (size_t i = 0; i < fixed_name.length(); ++i) {
		if (fixed_name[i] == ' ') {
			fixed_name[i] = '-';
		}
	}

	/* Spec is that the name part should be maximum 14 characters, as I understand it */
	if (fixed_name.length() > 14) {
		fixed_name = fixed_name.substr (0, 14);
	}

	d << fixed_name << "_";

	if (_dcp_content_type) {
		d << _dcp_content_type->dci_name() << "_";
	}

	if (_format) {
		d << _format->dci_name() << "_";
	}

	if (!_audio_language.empty ()) {
		d << _audio_language;
		if (!_subtitle_language.empty() && _with_subtitles) {
			d << "-" << _subtitle_language;
		} else {
			d << "-XX";
		}
			
		d << "_";
	}

	if (!_territory.empty ()) {
		d << _territory;
		if (!_rating.empty ()) {
			d << "-" << _rating;
		}
		d << "_";
	}

	switch (_audio_streams[_audio_stream].channels()) {
	case 1:
		d << "10_";
		break;
	case 2:
		d << "20_";
		break;
	case 6:
		d << "51_";
		break;
	case 8:
		d << "71_";
		break;
	}

	d << "2K_";

	if (!_studio.empty ()) {
		d << _studio << "_";
	}

	gregorian::date today = gregorian::day_clock::local_day ();
	d << gregorian::to_iso_string (today) << "_";

	if (!_facility.empty ()) {
		d << _facility << "_";
	}

	if (!_package_type.empty ()) {
		d << _package_type;
	}

	return d.str ();
}

/** @return name to give the DCP */
string
FilmState::dcp_name () const
{
	if (_use_dci_name) {
		return dci_name ();
	}

	return _name;
}


void
FilmState::set_directory (string d)
{
	_directory = d;
	_dirty = true;
}

void
FilmState::set_name (string n)
{
	_name = n;
	signal_changed (NAME);
}

void
FilmState::set_use_dci_name (bool u)
{
	_use_dci_name = u;
	signal_changed (USE_DCI_NAME);
}

void
FilmState::set_content (string c)
{
	string check = _directory;

#if BOOST_FILESYSTEM_VERSION == 3
	filesystem::path slash ("/");
	string platform_slash = slash.make_preferred().string ();
#else
#ifdef DVDOMATIC_WINDOWS
	string platform_slash = "\\";
#else
	string platform_slash = "/";
#endif
#endif	

	if (!ends_with (check, platform_slash)) {
		check += platform_slash;
	}
	
	if (filesystem::path(c).has_root_directory () && starts_with (c, check)) {
		c = c.substr (_directory.length() + 1);
	}

	if (c == _content) {
		return;
	}

	/* Create a temporary decoder so that we can get information
	   about the content.
	*/

	shared_ptr<FilmState> s = state_copy ();
	s->_content = c;
	shared_ptr<Options> o (new Options ("", "", ""));
	o->out_size = Size (1024, 1024);
	
	shared_ptr<Decoder> d = decoder_factory (s, o, 0, 0);
	
	set_size (d->native_size ());
	set_frames_per_second (d->frames_per_second ());
	set_audio_sample_rate (d->audio_sample_rate ());
	set_has_subtitles (d->has_subtitles ());
	set_audio_streams (d->audio_streams ());
	set_subtitle_streams (d->subtitle_streams ());
	set_audio_stream (audio_streams().empty() ? -1 : 0);
	set_subtitle_stream (subtitle_streams().empty() ? -1 : 0);
	set_content_digest (md5_digest (c));
	
	_content = c;
	signal_changed (CONTENT);
}
	       
void
FilmState::set_dcp_content_type (DCPContentType const * t)
{
	_dcp_content_type = t;
	signal_changed (DCP_CONTENT_TYPE);
}

void
FilmState::set_format (Format const * f)
{
	_format = f;
	signal_changed (FORMAT);
}

void
FilmState::set_crop (Crop c)
{
	_crop = c;
	signal_changed (CROP);
}

void
FilmState::set_left_crop (int c)
{
	if (_crop.left == c) {
		return;
	}

	_crop.left = c;
	signal_changed (CROP);
}

void
FilmState::set_right_crop (int c)
{
	if (_crop.right == c) {
		return;
	}

	_crop.right = c;
	signal_changed (CROP);
}

void
FilmState::set_top_crop (int c)
{
	if (_crop.top == c) {
		return;
	}

	_crop.top = c;
	signal_changed (CROP);
}

void
FilmState::set_bottom_crop (int c)
{
	if (_crop.bottom == c) {
		return;
	}

	_crop.bottom = c;
	signal_changed (CROP);
}

void
FilmState::set_filters (vector<Filter const *> f)
{
	_filters = f;
	signal_changed (FILTERS);
}

void
FilmState::set_scaler (Scaler const * s)
{
	_scaler = s;
	signal_changed (SCALER);
}

void
FilmState::set_dcp_frames (int f)
{
	_dcp_frames = f;
	signal_changed (DCP_FRAMES);
}

void
FilmState::set_dcp_trim_action (TrimAction a)
{
	_dcp_trim_action = a;
	signal_changed (DCP_TRIM_ACTION);
}

void
FilmState::set_dcp_ab (bool a)
{
	_dcp_ab = a;
	signal_changed (DCP_AB);
}

void
FilmState::set_audio_stream (int s)
{
	_audio_stream = s;
	signal_changed (AUDIO_STREAM);
}

void
FilmState::set_audio_gain (float g)
{
	_audio_gain = g;
	signal_changed (AUDIO_GAIN);
}

void
FilmState::set_audio_delay (int d)
{
	_audio_delay = d;
	signal_changed (AUDIO_DELAY);
}

void
FilmState::set_still_duration (int d)
{
	_still_duration = d;
	signal_changed (STILL_DURATION);
}

void
FilmState::set_subtitle_stream (int s)
{
	_subtitle_stream = s;
	signal_changed (SUBTITLE_STREAM);
}

void
FilmState::set_with_subtitles (bool w)
{
	_with_subtitles = w;
	signal_changed (WITH_SUBTITLES);
}

void
FilmState::set_subtitle_offset (int o)
{
	_subtitle_offset = o;
	signal_changed (SUBTITLE_OFFSET);
}

void
FilmState::set_subtitle_scale (float s)
{
	_subtitle_scale = s;
	signal_changed (SUBTITLE_SCALE);
}

void
FilmState::set_audio_language (string l)
{
	_audio_language = l;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_subtitle_language (string l)
{
	_subtitle_language = l;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_territory (string t)
{
	_territory = t;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_rating (string r)
{
	_rating = r;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_studio (string s)
{
	_studio = s;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_facility (string f)
{
	_facility = f;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_package_type (string p)
{
	_package_type = p;
	signal_changed (DCI_METADATA);
}

void
FilmState::set_thumbs (vector<int> t)
{
	_thumbs = t;
	signal_changed (THUMBS);
}

void
FilmState::set_size (Size s)
{
	_size = s;
	signal_changed (SIZE);
}

void
FilmState::set_length (int l)
{
	_length = l;
	signal_changed (LENGTH);
}

void
FilmState::set_audio_sample_rate (int r)
{
	_audio_sample_rate = r;
	signal_changed (AUDIO_SAMPLE_RATE);
}

void
FilmState::set_content_digest (string d)
{
	_content_digest = d;
	_dirty = true;
}

void
FilmState::set_has_subtitles (bool s)
{
	_has_subtitles = s;
	signal_changed (HAS_SUBTITLES);
}

void
FilmState::set_audio_streams (vector<AudioStream> s)
{
	_audio_streams = s;
	_dirty = true;
}

void
FilmState::set_subtitle_streams (vector<SubtitleStream> s)
{
	_subtitle_streams = s;
	_dirty = true;
}

void
FilmState::set_frames_per_second (float f)
{
	_frames_per_second = f;
	signal_changed (FRAMES_PER_SECOND);
}

void
FilmState::set_audio_to_discard (int a)
{
	_audio_to_discard = a;
	signal_changed (AUDIO_TO_DISCARD);
}
	
void
FilmState::signal_changed (Property p)
{
	_dirty = true;
	Changed (p);
}

shared_ptr<FilmState>
FilmState::state_copy () const
{
	return shared_ptr<FilmState> (new FilmState (*this));
}

int
FilmState::audio_channels () const
{
	if (_audio_stream == -1) {
		return 0;
	}

	return _audio_streams[_audio_stream].channels ();
}

int
FilmState::total_audio_delay () const
{
	return _audio_delay - _audio_to_discard;
}
