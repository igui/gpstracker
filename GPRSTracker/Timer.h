// Timer.h

#ifndef _TIMER_h
#define _TIMER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

class Timer
{
 private:
	unsigned long timeout;
	bool hasTimeout;
 public:
	Timer();
	void removeTimeout();
	void setTimeout(unsigned long milliseconds);
	bool wasExpired();
};

#endif

