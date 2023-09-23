/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_J2K_ENCODER_THREAD
#define DCPOMATIC_J2K_ENCODER_THREAD


#include <boost/thread.hpp>


class J2KEncoder;


class J2KEncoderThread
{
public:
	J2KEncoderThread(J2KEncoder& encoder);

	J2KEncoderThread(J2KEncoderThread const&) = delete;
	J2KEncoderThread& operator=(J2KEncoderThread const&) = delete;

	void start();
	void stop();

	virtual void run() = 0;

protected:
	J2KEncoder& _encoder;

private:
	boost::thread _thread;
};


#endif
