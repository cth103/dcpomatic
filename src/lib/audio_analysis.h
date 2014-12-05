/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/audio_analysis.h
 *  @brief AudioAnalysis and AudioPoint classes.
 */

#ifndef DCPOMATIC_AUDIO_ANALYSIS_H
#define DCPOMATIC_AUDIO_ANALYSIS_H

#include <boost/filesystem.hpp>
#include <vector>

/** @class AudioPoint
 *  @brief A single point of an audio analysis for one portion of one channel.
 */
class AudioPoint
{
public:
	enum Type {
		PEAK,
		RMS,
		COUNT
	};

	AudioPoint ();
	AudioPoint (FILE *);
	AudioPoint (AudioPoint const &);
	AudioPoint& operator= (AudioPoint const &);

	void write (FILE *) const;
	
	float& operator[] (int t) {
		return _data[t];
	}

private:
	float _data[COUNT];
};

/** @class AudioAnalysis
 *  @brief An analysis of the audio data in a piece of AudioContent.
 *
 *  This is a set of AudioPoints for each channel.  The AudioPoints
 *  each represent some measurement of the audio over a portion of the
 *  content.  For example each AudioPoint may give the RMS level of
 *  a 1-minute portion of the audio.
 */
class AudioAnalysis : public boost::noncopyable
{
public:
	AudioAnalysis (int c);
	AudioAnalysis (boost::filesystem::path);

	void add_point (int c, AudioPoint const & p);
	
	AudioPoint get_point (int c, int p) const;
	int points (int c) const;
	int channels () const;

	void write (boost::filesystem::path);

private:
	std::vector<std::vector<AudioPoint> > _data;
};

#endif
