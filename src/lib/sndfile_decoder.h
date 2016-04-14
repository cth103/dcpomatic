/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "audio_decoder.h"
#include "sndfile_base.h"

class SndfileContent;

class SndfileDecoder : public Sndfile, public AudioDecoder
{
public:
	SndfileDecoder (boost::shared_ptr<const SndfileContent> c, bool fast, boost::shared_ptr<Log> log);
	~SndfileDecoder ();

private:
	bool pass (PassReason, bool);
	void seek (ContentTime, bool);

	int64_t _done;
	int64_t _remaining;
	float* _deinterleave_buffer;
};
