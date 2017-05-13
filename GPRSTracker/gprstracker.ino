#include <SoftwareSerial.h> //Include the NewSoftSerial library to send serial commands to the cellular module.
#include <string.h> //Used for string manipulations
#include "GPRS.h"
#include "TinyGPS++.h"

// The serial connection to the GPS device
SoftwareSerial gpsSerial(7,8);
SoftwareSerial cellSerial(2, 3); //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.

// GRPS Object
GPRS gprs(cellSerial, "antel.lte", "", "", "200.40.220.245");
// The TinyGPS++ object
TinyGPSPlus gps;

void setup()
{
	//Initialize serial ports for communication.
	Serial.begin(9600);
	cellSerial.begin(9600);
	gpsSerial.begin(9600);

	//Let's get started!
	Serial.println("Begin");
	gprs.beginRequest("httpbin.org", "/get?value=Alaborda");
}


void loop() {
	/*gpsSerial.listen();
	if(gpsSerial.available() > 0)
	{
		if (gps.encode(gpsSerial.read()) 
			&& gps.location.isValid()
			&& gps.date.isValid())
			{
				displayGPSInfo();
			}
	}*/

	//If a character comes in from the cellular module...
	cellSerial.listen();
	if (cellSerial.available() > 0)
	{
		char incomingChar = cellSerial.read(); //Get the character from the cellular serial port.
		gprs.loop(incomingChar);
	}
}

String getGPSInfo()
{
	return String(gps.location.lat(), 6) + ',' + String(gps.location.lng(), 6) + 'D' +
		gps.date.value() + 'T' + gps.time.value();
}

void displayGPSInfo()
{
	Serial.print(F("Location: "));
	Serial.print(gps.location.lat(), 6);
	Serial.print(F(","));
	Serial.print(gps.location.lng(), 6);

	Serial.print(F("  Date/Time: "));
	Serial.print(gps.date.month());
	Serial.print(F("/"));
	Serial.print(gps.date.day());
	Serial.print(F("/"));
	Serial.print(gps.date.year());

	Serial.print(F(" "));
	if (gps.time.hour() < 10) Serial.print(F("0"));
	Serial.print(gps.time.hour());
	Serial.print(F(":"));
	if (gps.time.minute() < 10) Serial.print(F("0"));
	Serial.print(gps.time.minute());
	Serial.print(F(":"));
	if (gps.time.second() < 10) Serial.print(F("0"));
	Serial.print(gps.time.second());
	Serial.print(F("."));
	if (gps.time.centisecond() < 10) Serial.print(F("0"));
	Serial.print(gps.time.centisecond());

	Serial.println();
}
