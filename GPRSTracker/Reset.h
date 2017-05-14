// Reset.h

#ifndef _RESET_h
#define _RESET_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class Reset
{
 public:
	static void resetLoop();
};

#endif

