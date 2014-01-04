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


// --------------------------------------------------------------------
//	�	ColorGamma.c
// --------------------------------------------------------------------
//
//	ColorGammaLib
//
//		by Drew Thaler (athaler@umich.edu)
//		with help and technical advice from Matt Slot (fprefect@AmbrosiaSW.com)
//		based in part on Matt Slot's Gamma Fader lib v1.2.
//
//	See the corresponding header file (ColorGamma.h) for details,
//	redistribution information, and miscellaneous documentation.



// --------------------------------------------------
//	�	Preprocessor flags
// --------------------------------------------------

	// extern functions:	true == call externally-linked function
	//						false == define it ourselves (good for standalone libs)
#define EXTERN_MEMSET			false
#define EXTERN_TRAPAVAILABLE	false

	// should internal functions be static (local to this file) ?
	//	if so, symbols for these functions are never exported.
#define INTERNAL_FUNCTIONS_ARE_STATIC		true

	// export symbols for global functions?
#pragma export on



// --------------------------------------------------
//	�	Includes
// --------------------------------------------------

#include <Gestalt.h>
#include <Quickdraw.h>
#include <Traps.h>
#include <Video.h>

#include "Local ColorGamma.h"

#include		"WaterRace.h"

// ----------------------------------------------------------------------------------
//	�	Typedefs, enums, and #defines
// ----------------------------------------------------------------------------------

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif


typedef struct OpaqueGammaInfo GammaInfo, *GammaInfoPtr, **GammaInfoHdl;
struct OpaqueGammaInfo {
	GammaInfoHdl	next;
	
	GDHandle		screenDevice;
	GammaTblHandle	original;
	GammaTblHandle	unmodified;
	GammaTblHandle	hacked;
	UInt16			size;
	Boolean			canDoColor;
};


typedef enum { fadeToColor, fadeToGamma } XFadeCommand;

typedef UInt16 *XRGBChannels;


#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

// ----------------------------------------------------------------------------------
//	�	Prototypes for internal functions
// ----------------------------------------------------------------------------------


#if (INTERNAL_FUNCTIONS_ARE_STATIC)
 #define internal_		static
#else
 #define internal_
#endif


#if (EXTERN_MEMSET)
 #include <string.h>
#else
 #define memset(a,b,c)	ColorGamma_memset(a,b,c)
 internal_ void *ColorGamma_memset( void *, UInt8 val, Size len );
#endif


#if (EXTERN_TRAPAVAILABLE)
 Boolean TrapAvailable( short trap );
#else
 #define TrapAvailable(a)		ColorGamma_TrapAvailable(a)
 internal_ Boolean ColorGamma_TrapAvailable( short trap );
#endif



internal_ OSErr GetDeviceGammaTable( GDHandle theDevice, GammaTblPtr *oldTable );
internal_ OSErr SetDeviceGammaTable( GDHandle theDevice, GammaTblPtr *newTable );

internal_ OSErr XHighLevelFade( GDHandle theDevice, XFadeCommand command, void *to,
				UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume);

internal_ OSErr XCalcFadeToColor( GammaInfoHdl from, const RGBColor *color,
				UInt16 whichStep, UInt16 numSteps );

internal_ OSErr XCalcFadeToGamma( GammaInfoHdl from, GammaInfoHdl to,
				UInt16 whichStep, UInt16 numSteps );


// --------------------------------------------------------------------
//	�	Globals
// --------------------------------------------------------------------

UInt32	colorGammaState = 0;
enum {
	cgNotAvailable = 1,
	cgAvailable = 2
};

OSErr	cgLastErr = noErr;






#if 0
#pragma mark === Availability ===
#endif


// --------------------------------------------------------------------
//	�	IsColorGammaAvailable
// --------------------------------------------------------------------
//	Checks for necessary traps, etc, and returns TRUE if color
//	gamma fading is available.  Note that a true result from
//	here does not necessarily mean that a given monitor will
//	be able to do a color fade, just that the hardware appears
//	to be capable of it.

pascal Boolean
IsColorGammaAvailable()
{
		// if we checked already, use that result
	if (colorGammaState)
		return colorGammaState;
	
		// need GetDeviceList
	if ( !TrapAvailable(_GetDeviceList) )
	{
		colorGammaState = cgNotAvailable;
		return false;
	}
	
		// sure, why not?
	colorGammaState = cgAvailable;
	return true;
}





#if 0
#pragma mark -
#pragma mark === Begin/end fading ===
#endif


// --------------------------------------------------------------------
//	�	StartFading
// --------------------------------------------------------------------
//	Obtains the initial gamma state of all monitors, and returns the state
//	as a reference in the returnedState param.  If an error occurs,
//	returnedState is nil and the result is the error code.

pascal OSErr
StartFading( GammaInfoHdl *returnedState )
{
		// implementation is the same as GetMonitorState... just do that.
	*returnedState = GetMonitorState();
	return cgLastErr;
}



// --------------------------------------------------------------------
//	�	Start1Fading
// --------------------------------------------------------------------
//	Single monitor version of StartFading.

pascal OSErr
Start1Fading( GDHandle theDevice, GammaInfoHdl *returnedState )
{
	*returnedState = Get1MonitorState( theDevice );
	return cgLastErr;
}






// --------------------------------------------------------------------
//	�	StopFading
// --------------------------------------------------------------------
//	Called to finish fading -- disposes memory allocated for initialState,
//	and optionally restores the gamma states of the monitors to the
//	specified state.

pascal void
StopFading( GammaInfoHdl initialState, Boolean restore )
{
	if (restore) SetMonitorState( initialState );
	DisposeGammaRef( initialState );
}





#if 0
#pragma mark -
#pragma mark === State functions ===
#endif


// --------------------------------------------------------------------
//	�	GetMonitorState
// --------------------------------------------------------------------
//	Returns a GammaRef representing the current state of all monitors,
//	or nil if no monitors can fade.

pascal GammaInfoHdl
GetMonitorState( void )
{
	GDHandle		theDevice;
	GammaInfoHdl	temp, state = nil;
	
		// check for availability first
	if (colorGammaState == 0)
		IsColorGammaAvailable();
	if (colorGammaState == cgNotAvailable)
	{
		cgLastErr = notEnoughHardwareErr;
		return nil;
	}
	
		// walk device list
	for (theDevice = GetDeviceList(); theDevice; theDevice = GetNextDevice(theDevice) )
	{
		temp = Get1MonitorState( theDevice );
		if (!temp) continue;
		
		(**temp).next = state;
		state = temp;
	}
	
		// check and see if we found anything
	if (state == nil)
		cgLastErr = noMonitorsCanFade;
	
	return state;
}




// --------------------------------------------------------------------
//	�	Get1MonitorState
// --------------------------------------------------------------------
//	Returns a GammaRef representing the current state of
//	the specified monitor, or nil if the monitor isn't capable
//	of gamma fading.

pascal GammaInfoHdl
Get1MonitorState( GDHandle theDevice )
{
	GammaInfoHdl	state = nil;
	GammaTblPtr		masterGTable;
	GammaTblHandle	original = nil, hacked = nil, unmodified = nil;
	UInt16			size;
	OSErr			err;
	Boolean			canDoColor = false;
	
		// check for availability first
	if (colorGammaState == 0)
		IsColorGammaAvailable();
	if (colorGammaState == cgNotAvailable)
	{
		cgLastErr = notEnoughHardwareErr;
		return nil;
	}
	
		// device must be a screen, with a driver, and can't be
		//	a fixed-clut device such as a PowerBook.
	if ( ! TestDeviceAttribute(theDevice,screenDevice) ||
		 TestDeviceAttribute(theDevice,noDriver) ||
		 (**theDevice).gdType == fixedType )
	{
		cgLastErr = cDevErr;
		return nil;
	}
	
		// get the device's gamma table
	err = GetDeviceGammaTable( theDevice, &masterGTable );
	if (err) { cgLastErr = err; return nil; }
	
	size = sizeof(GammaTbl) + masterGTable->gFormulaSize +
						(masterGTable->gChanCnt * masterGTable->gDataCnt *
						masterGTable->gDataWidth / 8);
	
		// allocate memory for the state record
	state = (GammaInfoHdl) NewHandle( sizeof(GammaInfo) );
	original = (GammaTblHandle) NewHandle( size );
	hacked = (GammaTblHandle) NewHandle( size );
	if (!state || !original || !hacked)
	{
		cgLastErr = MemError();
		if (cgLastErr == noErr)
			cgLastErr = memFullErr;
		
			// failed? free allocated memory
		if (state) DisposeHandle((Handle)state);
		if (original) DisposeHandle((Handle)original);
		if (hacked) DisposeHandle((Handle)hacked);
		
		return nil;
	}
	
	BlockMoveData( masterGTable, *original, size );
	
		// additional work here... make sure the device is happy
		//	with a three-channel gamma table
	if ((**original).gChanCnt == 1)
	{
		Ptr		data;
		UInt32	dataSize;
		BlockMoveData( *original, *hacked, size );
		
			// copy original -> hacked
		BlockMoveData( *original, *hacked, size );
		
			// modify hacked from 1 channel -> 3 channels
		dataSize = (**original).gDataCnt * (**original).gDataWidth / 8;
		SetHandleSize( (Handle)hacked, sizeof(GammaTbl) + (**original).gFormulaSize
													+ dataSize * 3 );
		
		data = (Ptr) (**hacked).gFormulaData + (**hacked).gFormulaSize;
		BlockMoveData( data, data+dataSize, dataSize );
		BlockMoveData( data, data+dataSize*2, dataSize );
		
			// try installing it
		(**hacked).gChanCnt = 3;
		err = SetDeviceGammaTable( theDevice, hacked );
		if (err == noErr)
		{
				// it worked? then remember the unmodified original
				//	and copy 3-channel version to "original".
			
			unmodified = original;
			original = hacked;
			HandToHand((Handle*)&original);
			size += dataSize * 2;
		}
	}
	
	(**state).next			= nil;
	(**state).screenDevice	= theDevice;
	(**state).original		= original;
	(**state).unmodified	= unmodified;
	(**state).hacked		= hacked;
	(**state).size			= size;
	(**state).canDoColor	= ((**original).gChanCnt == 3) ? true:false;
	
	return state;
}



// --------------------------------------------------------------------
//	�	SetMonitorState
// --------------------------------------------------------------------

pascal OSErr
SetMonitorState( GammaRef state )
{
	for ( ; state; state = (**state).next )
	{
		GammaTblHandle	table = (**state).unmodified;
		if (!table) table = (**state).original;
		
		SetDeviceGammaTable( (**state).screenDevice, table );
	}
	
	return noErr;
}



// --------------------------------------------------------------------
//	�	Set1MonitorState
// --------------------------------------------------------------------

pascal OSErr
Set1MonitorState( GDHandle theDevice, GammaRef state )
{
	GammaTblHandle	table;
	
	for ( ; state && (**state).screenDevice != theDevice; state = (**state).next )
		;
	
	if (state)
	{
		table = (**state).unmodified;
		if (!table) table = (**state).original;
		
		SetDeviceGammaTable( theDevice, table );
	}
	
	return state ? noErr:fnfErr;
}


pascal OSErr Set1MonitorGammaColor(GammaRef state, GDHandle theDevice, const RGBColor* color, long intensity, long max)
{
	for( ; state && (**state).screenDevice != theDevice; state = (**state).next)
	;
	
	if(state) {
		XCalcFadeToColor(state, color, intensity, max);
		SetDeviceGammaTable(theDevice, (**state).hacked);
	}
	
	return state ? noErr:fnfErr;
}



// --------------------------------------------------------------------
//	�	DisposeGammaRef
// --------------------------------------------------------------------
//	Disposes of the memory associated with a GammaRef.

pascal OSErr
DisposeGammaRef( GammaInfoHdl state )
{
	GammaInfoHdl	temp;
	
		// reality check
	if (!state) return nilHandleErr;
	
	while (state)
	{
		temp = (**state).next;
		
		if ((**state).unmodified)
			DisposeHandle((Handle)(**state).unmodified);
		
		DisposeHandle((Handle)(**state).original);
		DisposeHandle((Handle)(**state).hacked);
		DisposeHandle((Handle)state);
		state = temp;
	}
	
	return noErr;
}






#if 0
#pragma mark -
#pragma mark === High-level fading functions ===
#endif


// --------------------------------------------------------------------
//	�	FadeToColor, Fade1ToColor, FadeToGamma, Fade1ToGamma
// --------------------------------------------------------------------
//	High-level routines to fade all monitors or a single monitor
//	to a specified color or a specified gamma.  Since the procedures
//	are nearly identical, all of these are mapped to a single internal
//	function, XHighLevelFade.

pascal OSErr
FadeToColor( const RGBColor *color, UInt16 numSteps, FadeType typeOfFade )
{
	return XHighLevelFade( nil, fadeToColor, (void*)color, numSteps, typeOfFade, nil, 0 );
}


pascal OSErr
Fade1ToColor( GDHandle theDevice, const RGBColor *color, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	return XHighLevelFade( theDevice, fadeToColor, (void*)color, numSteps, typeOfFade, chan, volume);
}


pascal OSErr
FadeToGamma( GammaInfoHdl to, UInt16 numSteps, FadeType typeOfFade )
{
	return XHighLevelFade( nil, fadeToGamma, (void*)to, numSteps, typeOfFade, nil, 0);
}


pascal OSErr
Fade1ToGamma( GDHandle theDevice, GammaInfoHdl to, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	return XHighLevelFade( theDevice, fadeToGamma, (void*)to, numSteps, typeOfFade, chan, volume );
}

pascal OSErr
FadeToBlack( UInt16 numSteps, FadeType typeOfFade )
{
	RGBColor	black = {0,0,0};
	return XHighLevelFade( nil, fadeToColor, (void*)&black, numSteps, typeOfFade, nil, 0);
}

pascal OSErr
Fade1ToBlack( GDHandle theDevice, UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	RGBColor	black = {0,0,0};
	return XHighLevelFade( theDevice, fadeToColor, (void*)&black, numSteps, typeOfFade, chan, volume );
}



// --------------------------------------------------------------------
//	�	XHighLevelFade [internal]
// --------------------------------------------------------------------
//	Wrapper function for high-level fades that maps to the appropriate
//	low-level calls.

#define Short2Long(v) ((v << 16) + v)

static void SetVolume(SndChannelPtr chan, short volume)
{
	SndCommand		theCommand;
	
	theCommand.cmd		= volumeCmd;
	theCommand.param1	= 0;
	theCommand.param2	= Short2Long(volume);
	SndDoImmediate(chan, &theCommand);
}

static short GetVolume(SndChannelPtr chan)
{
	SndCommand		theCommand;
	long			volume;
	
	theCommand.cmd		= getVolumeCmd;
	theCommand.param1	= 0;
	theCommand.param2	= (long) &volume;;
	SndDoImmediate(chan, &theCommand);
	
	return (volume & 0x0000FFFF);
}

internal_ OSErr
XHighLevelFade( GDHandle theDevice, XFadeCommand command, void *to,
				UInt16 numSteps, FadeType typeOfFade, SndChannelPtr chan, short volume)
{
	register GammaInfoHdl	currentState;
	register UInt32	i, which, total;
	register OSErr	err = invalidFadeType;
	register float	soundDiff;
	UnsignedWide			startTime,
								time;
							
		// pin numSteps to appropriate ranges for nonlinear fades
	if (typeOfFade & quadraticFade)
		while (numSteps > 256)
			numSteps >>= 1;
	
		// set up total number of steps
	switch (typeOfFade)
	{
		case linearFade:
			total = numSteps;
			break;
		
		case quadraticFade:
		case inverseQuadraticFade:
			total = numSteps * numSteps;
			break;
		
		default:
			return invalidFadeType;
	}
	
		// retrieve current state
	//HideCursor();
	if (theDevice)
		currentState = Get1MonitorState(theDevice);
	else
		currentState = GetMonitorState();
	if (!currentState) return cgLastErr;
	
	//Calculate sound fade
	if(chan != nil)
	soundDiff = (float) (GetVolume(chan) - volume) / (float) (numSteps + 1);
		
	// loop and do the fade
	for(i=0;i<=numSteps;++i)
	{
		Microseconds(&startTime);
		
		switch (typeOfFade)
		{
			case linearFade:			which = i;				break;
			case quadraticFade:			which = i*i;			break;
			
			case inverseQuadraticFade:
				which = total - (numSteps-i) * (numSteps-i);
				break;
		}
		
		// call appropriate low-level command
		if (command == fadeToColor)
			err = FadeFromGammaToColor( currentState, (RGBColor*)to, which, total );
		else
			err = FadeFromGammaToGamma( currentState, (GammaInfoHdl)to, which, total );
		
		if (err) break;

		//Set sound volume
		if((chan != nil) && (soundDiff != 0.0))
		SetVolume(chan, volume + soundDiff * (numSteps - i));

		do {
			Microseconds(&time);
		} while((time.lo - startTime.lo) < 10000);
	}
	
	DisposeGammaRef( currentState );
	//ShowCursor();
	cgLastErr = err;
	return err;
}

#if 0
#pragma mark -
#pragma mark === Low-level fading functions ===
#endif



// --------------------------------------------------------------------
//	�	FadeFromGammaToColor
// --------------------------------------------------------------------
//	Low-level routine to fade from the given gamma to a specified color.

pascal OSErr
FadeFromGammaToColor( GammaInfoHdl from, const RGBColor *color,
						UInt16 whichStep, UInt16 numSteps )
{
	cgLastErr = noErr;
	
		// walk the list of screens in the GammaRef
	for ( ; from; from = (**from).next )
	{
		OSErr	err;
		
			// calculate the fade
		err = XCalcFadeToColor( from, color, whichStep, numSteps );
		if (err)
		{
			cgLastErr = err;
			continue;
		}
		
			// and install the hacked table
		err = SetDeviceGammaTable( (**from).screenDevice, (**from).hacked );
		if (err) cgLastErr = err;
	}
	
	return cgLastErr;
}




// --------------------------------------------------------------------
//	�	FadeFromGammaToGamma
// --------------------------------------------------------------------
//	Low-level routine to fade from the given gamma to a specified gamma.

pascal OSErr
FadeFromGammaToGamma( GammaInfoHdl from, GammaInfoHdl to,
						UInt16 whichStep, UInt16 numSteps )
{
	cgLastErr = noErr;
	
		// walk the list of screens in the GammaRef
	for ( ; from; from = (**from).next )
	{
		OSErr	err;
		
			// calculate the fade
		err = XCalcFadeToGamma( from, to, whichStep, numSteps );
		if (err)
		{
			cgLastErr = err;
			continue;
		}
		
			// and install the hacked table
		err = SetDeviceGammaTable( (**from).screenDevice, (**from).hacked );
		if (err) cgLastErr = err;
	}
	
	return cgLastErr;
}







// --------------------------------------------------------------------
//	�	CalcFadeToColor
// --------------------------------------------------------------------
//	Calculates an intermediate gamma value between a specified gamma
//	and a color, without actually doing the fade.  Allocates and returns
//	a new GammaRef, which caller is responsible for disposing of.

pascal GammaInfoHdl
CalcFadeToColor( GammaInfoHdl from, const RGBColor *color,
							UInt16 whichStep, UInt16 numSteps )
{
	GammaInfoHdl	result = nil, temp = nil;

	cgLastErr = noErr;
	
		// walk list of devices
	for ( ; from; from = (**from).next )
	{
		GammaInfoHdl	temp;
		GammaInfoPtr	fromPtr, tempPtr;
		GammaTblHandle	original, hacked;
		UInt16			size = (**from).size;
		OSErr	err;
		
			// calculate fade
		err = XCalcFadeToColor( from, color, whichStep, numSteps );
		
			// and copy the hacked table to the new 
		temp = (GammaInfoHdl) NewHandle( sizeof(GammaInfo) );
		original = (GammaTblHandle) NewHandle( size );
		hacked = (GammaTblHandle) NewHandle( size );
		if (!temp || !original || !hacked)
		{
			cgLastErr = MemError();
			if (cgLastErr == noErr)
				cgLastErr = memFullErr;
			
				// failed? free allocated memory
			if (temp) DisposeHandle((Handle)temp);
			if (original) DisposeHandle((Handle)original);
			if (hacked) DisposeHandle((Handle)hacked);
			
			return result;
		}
		
			// copy the hacked table
		fromPtr = *from;
		tempPtr = *temp;
		BlockMoveData( *(fromPtr->hacked), *original, size );
		
		tempPtr->next			= result;
		tempPtr->screenDevice	= fromPtr->screenDevice;
		tempPtr->original		= original;
		tempPtr->hacked			= hacked;
		tempPtr->unmodified		= nil;
		tempPtr->size			= size;
		tempPtr->canDoColor		= fromPtr->canDoColor;
		result = temp;
	}
	
	return result;
}









// --------------------------------------------------------------------
//	�	CalcFadeToGamma
// --------------------------------------------------------------------
//	Calculates an intermediate gamma value between two specified gammas,
//	without actually doing the fade.  Allocates and returns a new GammaRef,
//	which caller is responsible for disposing of.

pascal GammaInfoHdl
CalcFadeToGamma( GammaInfoHdl from, GammaInfoHdl to,
							UInt16 whichStep, UInt16 numSteps )
{
	GammaInfoHdl	result = nil, temp = nil;

	cgLastErr = noErr;
	
		// walk list of devices
	for ( ; from; from = (**from).next )
	{
		GammaInfoHdl	temp;
		GammaInfoPtr	fromPtr, tempPtr;
		GammaTblHandle	original, hacked;
		UInt16			size = (**from).size;
		OSErr	err;
		
			// calculate fade
		err = XCalcFadeToGamma( from, to, whichStep, numSteps );
		
			// and copy the hacked table to the new 
		temp = (GammaInfoHdl) NewHandle( sizeof(GammaInfo) );
		original = (GammaTblHandle) NewHandle( size );
		hacked = (GammaTblHandle) NewHandle( size );
		if (!temp || !original || !hacked)
		{
			cgLastErr = MemError();
			if (cgLastErr == noErr)
				cgLastErr = memFullErr;
			
				// failed? free allocated memory
			if (temp) DisposeHandle((Handle)temp);
			if (original) DisposeHandle((Handle)original);
			if (hacked) DisposeHandle((Handle)hacked);
			
			return result;
		}
		
			// copy the hacked table
		fromPtr = *from;
		tempPtr = *temp;
		BlockMoveData( *(fromPtr->hacked), *original, size );
		
		tempPtr->next			= result;
		tempPtr->screenDevice	= fromPtr->screenDevice;
		tempPtr->original		= original;
		tempPtr->hacked			= hacked;
		tempPtr->unmodified		= nil;
		tempPtr->size			= size;
		tempPtr->canDoColor		= fromPtr->canDoColor;
		result = temp;
	}
	
	return result;
}



// -----------------------------------------------------------------------------------
//	�	XCalcFadeToColor [internal]
// -----------------------------------------------------------------------------------
//	Calculates a color fade between (**from).original and (*color), and places the
//	result in (**from).hacked.

internal_ OSErr
XCalcFadeToColor( GammaInfoHdl from, const RGBColor *color, UInt16 whichStep, UInt16 numSteps )
{
	GDHandle	theDevice = (**from).screenDevice;
	GammaTblHandle	original = (**from).original;
	GammaTblHandle	hacked = (**from).hacked;
	
	register UInt32	maxval, i, size, chan;
	register unsigned char	*srcptr, *dstptr;
	
		// reality check, monitor must be able to do 3-channel gamma
	if ( (**from).canDoColor == false )
		return monitorCantDoColorGamma;
	
		// copy start of original -> hacked
	BlockMoveData( *original, *hacked, sizeof(GammaTbl) + (**original).gFormulaSize );
	
		// some preliminary setup before the fade
	maxval = (1 << (**hacked).gDataWidth) - 1;
	
	if (numSteps > 256)
	{
		whichStep = ((long)whichStep) * 256 / numSteps;
		numSteps = 256;
	}
	
		// hack the table -- this is a bit more compute-intensive than
		//	a simple percentage fade, since the values aren't all being faded
		//	at the same rate...
	
	srcptr = (unsigned char *) (**original).gFormulaData + (**original).gFormulaSize;
	dstptr = (unsigned char *) (**hacked).gFormulaData + (**hacked).gFormulaSize;
	
	size = (**hacked).gDataCnt;
	for (chan = 0; chan < (**hacked).gChanCnt; ++chan)
	{
			// for safety, don't index outside the color array's range [0-2]
		UInt32 chanindex = (chan<3) ? chan : 2;

			// calculate values we're fading to
		register SInt32 channelDestination =
							((XRGBChannels) color) [chanindex] * maxval / 0xFFFF;
		
			// modify towards the given values
		for(i=0; i < size; i++)
		{
			dstptr[i] = srcptr[i] - ((long)srcptr[i] - (long)channelDestination)
										* whichStep / numSteps;
		}
		srcptr += size;
		dstptr += size;
	}
	
	return noErr;
}



// -----------------------------------------------------------------------------------
//	�	XCalcFadeToGamma [internal]
// -----------------------------------------------------------------------------------
//	Calculates a gamma fade between (**from).original and (**to).original, and places
//	the result in (**from).hacked. 

internal_ OSErr
XCalcFadeToGamma( GammaInfoHdl from, GammaInfoHdl to, UInt16 whichStep, UInt16 numSteps )
{
	GDHandle	theDevice = (**from).screenDevice;
	GammaTblHandle	original = (**from).original;
	GammaTblHandle	hacked = (**from).hacked;
	GammaTblHandle	src2;
	GammaInfoHdl	src2Info;
	
	register UInt32	i, size;
	register unsigned char	*src1ptr, *src2ptr, *dstptr;
	
		// find the right gamma entry in the "to" table for this device
	for (src2Info = to; src2Info; src2Info = (**src2Info).next)
		if ( (**src2Info).screenDevice == theDevice )
			break;
	if (!src2Info) return fnfErr;
	
		// tables must have same # of entries
	src2 = (**src2Info).original;
	if ( ((**src2).gChanCnt != (**original).gChanCnt) ||
		 ((**src2).gDataCnt != (**original).gDataCnt) )
		 return cDevErr;
	
		// copy start of original -> hacked
	BlockMoveData( *original, *hacked, sizeof(GammaTbl) + (**original).gFormulaSize );
	
		// hack the table
	
	if (numSteps > 256)
	{
		whichStep = ((long)whichStep) * 256 / numSteps;
		numSteps = 256;
	}

	src1ptr = (unsigned char *) (**original).gFormulaData + (**original).gFormulaSize;
	src2ptr = (unsigned char *) (**src2).gFormulaData + (**src2).gFormulaSize;
	dstptr = (unsigned char *) (**hacked).gFormulaData + (**hacked).gFormulaSize;
	
	size = (**hacked).gDataCnt * (**hacked).gChanCnt;
	for(i=0; i < size; i++)
	{
		dstptr[i] = src1ptr[i] - (long)(((long)src1ptr[i] -
					                (long)src2ptr[i]) * whichStep / numSteps);
	}
	
	return noErr;
}







#if 0
#pragma mark -
#pragma mark === Utility functions ===
#endif





// ----------------------------------------------------------------------------------
//	�	GetDeviceGammaTable [internal]
// ----------------------------------------------------------------------------------
//	Returns a pointer to the specified device's gamma table.

internal_ OSErr
GetDeviceGammaTable(GDHandle theGDevice, GammaTblPtr *theTable)
{
	OSErr		err = noErr;
	CntrlParam  cpb;
	
	*theTable = nil;
	
	memset( &cpb, 0, sizeof(cpb) );
	cpb.csCode = cscGetGamma;
	cpb.ioCRefNum = (**theGDevice).gdRefNum;
	
	*((GammaTblPtr**)cpb.csParam) = theTable;
	
	err = PBStatusSync( (ParmBlkPtr) &cpb );
	return err;
}





// ----------------------------------------------------------------------------------
//	�	SetDeviceGammaTable [internal]
// ----------------------------------------------------------------------------------
//	Sets the device's gamma table to the specified table.

internal_ OSErr
SetDeviceGammaTable(GDHandle theGDevice, GammaTblPtr *theTable)
{
	register OSErr		err = noErr;
	CntrlParam	cpb; 	
	
	memset( &cpb, 0, sizeof(cpb) );
	cpb.csCode = cscSetGamma;
	cpb.ioCRefNum = (**theGDevice).gdRefNum;
	((GammaTblPtr **) cpb.csParam)[0] = theTable;
	
	err = PBControlSync( (ParmBlkPtr) &cpb );
 	
	if (err == noErr)
	{
		register CTabPtr	cTab;
		register GDHandle	saveGDevice;
		
		saveGDevice = GetGDevice();
		SetGDevice(theGDevice);
 		cTab = *((**(**theGDevice).gdPMap).pmTable);
		SetEntries (0, cTab->ctSize, cTab->ctTable);
		SetGDevice(saveGDevice);
	}
	
	return err;
}





// -----------------------------------------------------------------------
//	�	TrapAvailable [internal]
// -----------------------------------------------------------------------
//	Returns true if the given trap is available on the machine.

#if ! EXTERN_TRAPAVAILABLE

internal_ Boolean
TrapAvailable( short trap )
{
	return NGetTrapAddress(trap, (trap & 0x0800) ? ToolTrap:OSTrap) !=
			NGetTrapAddress(_Unimplemented, ToolTrap);
}


#endif





// ---------------------------------------------------------------------------------
//	�	memset [internal]
// ---------------------------------------------------------------------------------
//	If you're not linking with the ANSI C libs, modify the preprocessor flags
//	at the beginning of this file and use this routine.  This is a moderately
//	optimized implementation of memset that will be fast on PPC and not
//	too bad on 68k...

#if ! EXTERN_MEMSET

internal_ void *
memset( void *ptr, UInt8 val, Size len )
{
	register UInt8	*charp = (UInt8*) ptr;
	
		// round off to 4-byte boundary
	if ( len & 3 )
	{
		register Size	len3 = len & 3;
		len -= len3;
		
		charp--;
		for ( ; len3 > 0; --len3 )
			(*++charp) = val;
	}
	
		// now do four bytes at a time
	if ( len > 4 )
	{
		register UInt32	*longp = (UInt32*) charp;
		register UInt32	longval = 0;
		register Size	len4;
		
		if (val) 
		{
			longval = val | (val<<8);
			longval |= longval << 16;
		}
		
		longp--;
		for ( len4 = len>>2; len4 > 0; --len4 )
		{
			(*++longp) = longval;
		}
		
		charp = (UInt8*) longp;
		len = len & 3;
	}
		
		// clean up remainder, one byte at a time
	charp--;
	for ( ; len > 0; --len )
	{
		(*++charp) = val;
	}
	
	return ptr;
}


#endif


