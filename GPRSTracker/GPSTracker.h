// GPSTracker.h

#ifndef _GPSTRACKER_h
#define _GPSTRACKER_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <SoftwareSerial.h>

class GPSTrackerClass
{
 private:
 public:
	void init(SoftwareSerial &cellSerial, SoftwareSerial& gpsSerial);
	void loop();
};

extern GPSTrackerClass GPSTracker;

#endif

