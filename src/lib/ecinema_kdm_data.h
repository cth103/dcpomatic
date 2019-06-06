/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

/* ECinema KDM data block contains:
   - key (16 bytes)
   - (optional) not-valid-before time (25 bytes)
   - (optional) not-valid-after time  (25 bytes)
*/

#define ECINEMA_KDM_KEY 0
#define ECINEMA_KDM_KEY_LENGTH 16
#define ECINEMA_KDM_NOT_VALID_BEFORE (ECINEMA_KDM_KEY_LENGTH)
#define ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH 25
#define ECINEMA_KDM_NOT_VALID_AFTER (ECINEMA_KDM_NOT_VALID_BEFORE + ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH)
#define ECINEMA_KDM_NOT_VALID_AFTER_LENGTH 25
