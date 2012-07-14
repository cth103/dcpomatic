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

/** A class which can be fed a stream of bytes and which can
 *  delay them by a positive or negative amount.
 */
class DelayLine
{
public:
	DelayLine (int);
	~DelayLine ();
	
	int feed (uint8_t *, int);
	void get_remaining (uint8_t *);

private:
	int _delay; ///< delay in bytes, +ve to move data later
	uint8_t* _buffer; ///< buffer for +ve delays, or 0
	int _negative_delay_remaining; ///< number of bytes of negative delay that remain to emit
};
