/*****************************************************************************
 * Copyright (c) 1998-2001, French Touch, SARL
 * http://www.french-touch.net
 * info@french-touch.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/


#ifndef __WATERRACE_CONDITIONALS_DEMO__
#define __WATERRACE_CONDITIONALS_DEMO__

#include	"Conditionals.h"


/********** OPTIONNAL CONDITIONALS **********/

//Play logo animation at game startup
#define	__PLAY_DEMO__	1

//Expo mode
#define	__EXPO_MODE__	1

//Use audio CD tracks
#define	__USE_AUDIO_CD_TRACKS__		1

//Show advertising at game quit
#define	__SHOW_PUB__	1


/********** OPTIONNAL CONSTANTS **********/

#define	kExpo_PilotSet			'BbSk'
#define	kExpo_PilotMode			kPilotType_FreePlay
#define	kExpo_PilotDifficulty	kDifficulty_Medium

#endif
