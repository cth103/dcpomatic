/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "exceptions.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;
using std::runtime_error;

/** @param f File that we were trying to open */
OpenFileError::OpenFileError (boost::filesystem::path f, int error, bool reading)
	: FileError (
		String::compose (
			reading ? _("could not open file %1 for reading (%2)") : _("could not open file %1 for writing (%2)"),
			f.string(),
			error),
		f
		)
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
	: SettingError (s, String::compose (_("Missing required setting %1"), s))
{

}

PixelFormatError::PixelFormatError (string o, AVPixelFormat f)
	: runtime_error (String::compose (_("Cannot handle pixel format %1 during %2"), (int) f, o))
{

}

TextSubtitleError::TextSubtitleError (string saw, string expecting, boost::filesystem::path f)
	: FileError (String::compose (_("Error in subtitle file: saw %1 while expecting %2"), saw.empty() ? "[nothing]" : saw, expecting), f)
{

}

InvalidSignerError::InvalidSignerError ()
	: runtime_error (_("The certificate chain for signing is invalid"))
{

}

InvalidSignerError::InvalidSignerError (string reason)
	: runtime_error (String::compose (_("The certificate chain for signing is invalid (%1)"), reason))
{

}

ProgrammingError::ProgrammingError (string file, int line, string message)
	: runtime_error (String::compose (_("Programming error at %1:%2 %3"), file, line, message))
{

}

KDMAsContentError::KDMAsContentError ()
	: runtime_error (_("This file is a KDM.  KDMs should be added to DCP content by right-clicking the content and choosing \"Add KDM\"."))
{

}

KDMError::KDMError (string s, string d)
	: runtime_error (String::compose ("%1 (%2)", s, d))
	, _summary (s)
	, _detail (d)
{

}
