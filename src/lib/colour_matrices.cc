/*
    Taken from OpenDCP: Builds Digital Cinema Packages
    Copyright (c) 2010-2011 Terrence Meiczinger, All Rights Reserved

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "colour_matrices.h"

/* Color Matrices */
float color_matrix[3][3][3] = {
    /* SRGB */
    {{0.4124564, 0.3575761, 0.1804375},
     {0.2126729, 0.7151522, 0.0721750},
     {0.0193339, 0.1191920, 0.9503041}},

    /* REC.709 */
    {{0.4124564, 0.3575761, 0.1804375},
     {0.2126729, 0.7151522, 0.0721750},
     {0.0193339, 0.1191920, 0.9503041}},

    /* DC28.30 (2006-02-24) */
    {{0.4451698156, 0.2771344092, 0.1722826698},
     {0.2094916779, 0.7215952542, 0.0689130679},
     {0.0000000000, 0.0470605601, 0.9073553944}}
};
