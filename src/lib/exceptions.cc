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

#include "exceptions.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;

/** @param f File that we were trying to open */
OpenFileError::OpenFileError (boost::filesystem::path f)
	: FileError (String::compose (_("could not open file %1"), f.string()), f)
{

}

/** @param f File that we were trying to create */
CreateFileError::CreateFileError (boost::filesystem::path f)
	: FileError (String::compose (_("could not create file %1"), f.string()), f)
{

}

ReadFileError::ReadFileError (boost::filesystem::path f, int e)
	: FileError (String::compose (_("could not read from file %1 (%2)"), f.string(), strerror (e)), f)
{

}

WriteFileError::WriteFileError (boost::filesystem::path f, int e)
	: FileError (String::compose (_("could not write to file %1 (%2)"), f.string(), strerror (e)), f)
{
	
}

MissingSettingError::MissingSettingError (string s)
	: SettingError (s, String::compose (_("missing required setting %1"), s))
{

}

PixelFormatError::PixelFormatError (string o, AVPixelFormat f)
	: StringError (String::compose (_("Cannot handle pixel format %1 during %2"), f, o))
{

}

SubRipError::SubRipError (string saw, string expecting, boost::filesystem::path f)
	: FileError (String::compose (_("Error in SubRip file: saw %1 while expecting %2"), saw.empty() ? "[nothing]" : saw, expecting), f)
{
	
}

InvalidSignerError::InvalidSignerError ()
	: StringError (_("The certificate chain for signing is invalid"))
{

}

ProgrammingError::ProgrammingError (string file, int line)
	: StringError (String::compose (_("Programming error at %1:%2"), file, line))
{

}
