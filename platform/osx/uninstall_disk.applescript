--    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>
--
--    This file is part of DCP-o-matic.
--
--    DCP-o-matic is free software; you can redistribute it and/or modify
--    it under the terms of the GNU General Public License as published by
--    the Free Software Foundation; either version 2 of the License, or
--    (at your option) any later version.
--
--    DCP-o-matic is distributed in the hope that it will be useful,
--    but WITHOUT ANY WARRANTY; without even the implied warranty of
--    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--    GNU General Public License for more details.
--
--    You should have received a copy of the GNU General Public License
--    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

set askText to "Do you want to uninstall the .pkg part of the DCP-o-matic Disk Writer?"
set reply to (display dialog askText buttons {"Cancel", "Uninstall"} default button "Uninstall" cancel button "Cancel")
if button returned of reply = "Uninstall" then
	do shell script "rm -rf \"/Library/Application Support/com.dcpomatic\"" with administrator privileges
	set doneText to "Uninstall complete.  Now close and delete the DCP-o-matic Disk Writer app."
	display dialog doneText buttons {"OK"}
end if
