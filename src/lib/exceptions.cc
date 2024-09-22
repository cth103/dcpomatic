/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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
using boost::optional;


/** @param f File that we were trying to open */
OpenFileError::OpenFileError (boost::filesystem::path f, int error, Mode mode)
	: FileError (
		String::compose (
			mode == READ_WRITE ? _("could not open file %1 for read/write (%2)") :
			(mode == READ ? _("could not open file %1 for read (%2)") : _("could not open file %1 for write (%2)")),
			f.string(),
			error),
		f
		)
{

}


FileNotFoundError::FileNotFoundError (boost::filesystem::path f)
	: runtime_error(String::compose("File %1 not found", f.string()))
	, _file (f)
{

}


ReadFileError::ReadFileError (boost::filesystem::path f, int e)
	: FileError (String::compose(_("could not read from file %1 (%2)"), f.string(), strerror(e)), f)
{

}


WriteFileError::WriteFileError (boost::filesystem::path f, int e)
	: FileError (String::compose(_("could not write to file %1 (%2)"), f.string(), strerror(e)), f)
{

}


MissingSettingError::MissingSettingError (string s)
	: SettingError (s, String::compose(_("Missing required setting %1"), s))
{

}


PixelFormatError::PixelFormatError (string o, AVPixelFormat f)
	: runtime_error (String::compose(_("Cannot handle pixel format %1 during %2"), (int) f, o))
{

}


TextSubtitleError::TextSubtitleError (string saw, string expecting, boost::filesystem::path f)
	: FileError (String::compose(_("Error in subtitle file: saw %1 while expecting %2"), saw.empty() ? "[nothing]" : saw, expecting), f)
{

}


InvalidSignerError::InvalidSignerError ()
	: runtime_error (_("The certificate chain for signing is invalid"))
{

}


InvalidSignerError::InvalidSignerError (string reason)
	: runtime_error (String::compose(_("The certificate chain for signing is invalid (%1)"), reason))
{

}


ProgrammingError::ProgrammingError (string file, int line, string message)
	: runtime_error (String::compose(_("Programming error at %1:%2 %3"), file, line, message))
{

}


KDMAsContentError::KDMAsContentError ()
	: runtime_error (_("This file is a KDM.  KDMs should be added to DCP content by right-clicking the content and choosing \"Add KDM\"."))
{

}


NetworkError::NetworkError (string s, optional<string> d)
	: runtime_error (String::compose("%1%2", s, d ? String::compose(" (%1)", *d) : ""))
	, _summary (s)
	, _detail (d)
{

}


KDMError::KDMError (string s, string d)
	: runtime_error (String::compose("%1 (%2)", s, d))
	, _summary (s)
	, _detail (d)
{

}


GLError::GLError (char const* last, int e)
	: runtime_error (String::compose("%1 failed %2", last, e))
{

}


GLError::GLError (char const* message)
	: runtime_error (message)
{

}


CopyError::CopyError(string m, optional<int> ext4, optional<int> platform)
	: runtime_error(String::compose("%1%2%3", m, ext4 ? String::compose(" (%1)", *ext4) : "", platform ? String::compose(" (%1)", *platform) : ""))
	, _message (m)
	, _ext4_number(ext4)
	, _platform_number(platform)
{

}


CommunicationFailedError::CommunicationFailedError ()
	: CopyError (_("Lost communication between main and writer processes"))
{

}


VerifyError::VerifyError (string m, int n)
	: runtime_error (String::compose("%1 (%2)", m, n))
	, _message (m)
	, _number (n)
{

}


DiskFullError::DiskFullError(boost::filesystem::path writing)
	: std::runtime_error(String::compose(_("Disk full when writing %1"), writing.string()))
{

}

