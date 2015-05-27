/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SNDFILE_BASE_H
#define DCPOMATIC_SNDFILE_BASE_H

#include <sndfile.h>
#include <boost/shared_ptr.hpp>

class SndfileContent;

class Sndfile
{
public:
	Sndfile (boost::shared_ptr<const SndfileContent> content);
	virtual ~Sndfile ();

protected:	
	boost::shared_ptr<const SndfileContent> _sndfile_content;
	SNDFILE* _sndfile;
	SF_INFO _info;
};

#endif
