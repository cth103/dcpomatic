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

/** @file src/lut.h
 *  @brief Look-up tables for colour conversions (from OpenDCP)
 */

#define BIT_DEPTH      12 
#define BIT_PRECISION  16 
#define COLOR_DEPTH    (4095)
#define DCI_LUT_SIZE   ((COLOR_DEPTH + 1) * BIT_PRECISION)
#define DCI_GAMMA      (2.6)
#define DCI_DEGAMMA    (1/DCI_GAMMA)
#define DCI_COEFFICENT (48.0/52.37)

enum COLOR_PROFILE_ENUM {
    CP_SRGB = 0,
    CP_REC709,
    CP_DC28,
    CP_MAX
};

enum LUT_IN_ENUM {
    LI_SRGB    = 0,
    LI_REC709,
    LI_MAX
};

enum LUT_OUT_ENUM {
    LO_DCI    = 0,
    LO_MAX
};

extern float color_matrix[3][3][3];
extern float lut_in[LI_MAX][4095+1];
extern int lut_out[1][DCI_LUT_SIZE];
