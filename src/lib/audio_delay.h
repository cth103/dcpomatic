/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include <memory>


class AudioBuffers;


/** @class AudioDelay
 *  @brief An audio delay line
 */
class AudioDelay
{
public:
	explicit AudioDelay (int samples);
	std::shared_ptr<AudioBuffers> run (std::shared_ptr<const AudioBuffers> in);
	void flush ();

private:
	std::shared_ptr<AudioBuffers> _tail;
	int _samples;
};
