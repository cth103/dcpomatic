/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "util.h"

class AudioBuffers;

class AudioMerger
{
public:
	AudioMerger (int channels, int frame_rate);

	/** Pull audio up to a given time; after this call, no more data can be pushed
	 *  before the specified time.
	 */
	TimedAudioBuffers<DCPTime> pull (DCPTime time);
	void push (boost::shared_ptr<const AudioBuffers> audio, DCPTime time);
	TimedAudioBuffers<DCPTime> flush ();
	void clear (DCPTime t);
	
private:
	boost::shared_ptr<AudioBuffers> _buffers;
	int _frame_rate;
	DCPTime _last_pull;
};
