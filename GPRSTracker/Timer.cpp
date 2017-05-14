// 
// 
// 

#include "Timer.h"

Timer::Timer(): timeout(0), hasTimeout(false)
{
}

void Timer::removeTimeout()
{
	hasTimeout = false;
}

void Timer::setTimeout(unsigned long milliseconds)
{
	timeout = millis() + milliseconds;
	hasTimeout = true;
}

bool Timer::wasExpired()
{
	//return false;
	return hasTimeout && millis() > timeout;
}
