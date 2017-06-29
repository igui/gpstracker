#include "Timer.h"
#include <SoftwareSerial.h> //Include the NewSoftSerial library to send serial commands to the cellular module.
#include <string.h> //Used for string manipulations
#include "GPRS.h"
#include "TinyGPS++.h"

// The serial connection to the GPS device
SoftwareSerial gpsSerial(7,8);
SoftwareSerial cellSerial(2, 3); //Create a 'fake' serial port. Pin 2 is the Rx pin, pin 3 is the Tx pin.

// Triggers passage to DEAD Status
const unsigned char DEADCHAR = 255;

// GRPS Object
GPRS gprs(cellSerial, "antel.lte", "", "", "200.40.220.245");
// The TinyGPS++ object
TinyGPSPlus gps;
// The request path to make requests
String requestPath;
// Read status
enum Status {
	INIT,
	READ_UNREAD_MESSAGES,
	SEND_SMS_DATA_QUERY,
	READ_SMS_DATA_QUERY_RESPONSE,
	SEND_SMS_RESPONSES,
	READ_GPS,
	UPLOAD_GPRS
} state;

const int GPS_SIGNAL_TIMEOUT = 1000;

const char *smsNumber = "+59899389599";

Timer gpsSignalTimeout;

void setup()
{
	//Initialize serial ports for communication.
	Serial.begin(9600);
	cellSerial.begin(9600);
	gpsSerial.begin(9600);
	
	state = INIT;
	cellSerial.listen();

	Serial.println(F("Begin"));
}

void loop() {
	switch (state)
	{
	case(INIT):
		initGPRS();
		break;
	case(READ_UNREAD_MESSAGES):
		readUnreadMessagesLoop();
		break;
	case(READ_GPS):
		readGPSLoop();
		break;
	case(UPLOAD_GPRS):
		uploadGPRSLoop();
		break;
	default:
		break;
	}
	anyStateLoop();
}

void dead()
{
}

void initGPRS()
{
	gprs.loop();

	if (gprs.readyForCommands())
	{
		Serial.println(F("GPRS Module ready"));
		readUnreadMessages();
	}
}

void unreadMessagesCallback(void *data, const String &number, const String &message)
{
	Serial.println(F("<<unreadMessagesCallback>>"));
	Serial.println(number);
	Serial.println(message);
}

void readUnreadMessages()
{
	gprs.receiveUnreadMessages(unreadMessagesCallback, NULL);
	state = READ_UNREAD_MESSAGES;

	if (gprs.readyForCommands())
	{
		Serial.println(F("Finish reading messages"));
		readGPS();
	}

}

void readUnreadMessagesLoop()
{
	gprs.loop();
}

void anyStateLoop()
{
	if (Serial.available())
	{
		auto c = Serial.read();

		if (c == DEADCHAR)
		{
			Serial.println(F("<<Kill GPRS>>"));
			gprs.kill();
		}
		else
		{
			cellSerial.write(c);
			Serial.write(c);
		}
	}
}

inline void readGPS()
{
	gpsSignalTimeout.setTimeout(GPS_SIGNAL_TIMEOUT);
	state = READ_GPS;
	gpsSerial.listen();
}

inline void readGPSLoop()
{
	if (gpsSignalTimeout.wasExpired())
	{
		Serial.println(F("<<GPSSignalTimeout>>"));
		uploadGPRS();
		return;
	}

	if (gpsSerial.available() > 0)
	{
		bool gotSentence = gps.encode(gpsSerial.read());
		if (!gotSentence)
		{
			return;
		}
		bool validGPSSentence = gps.location.isValid() && gps.date.isValid();
		if (!validGPSSentence)
		{
			return;
		}
		displayGPSInfo();
		uploadGPRS();
	}
}

inline void uploadGPRS()
{
	requestPath = "/upload?lat=" +
		String(gps.location.lat(), 6) +
		"&lng=" +
		String(gps.location.lng(), 6) +
		"&time=" +
		String(gps.time.value(), DEC);

	gprs.beginRequest("whereislolo.herokuapp.com", requestPath.c_str());

	cellSerial.listen();
	state = UPLOAD_GPRS;
}

inline void uploadGPRSLoop()
{
	gprs.loop();

	if (gprs.readyForCommands())
	{
		auto error = gprs.getLastError();
		if (error == GPRS::NO_ERROR)
		{
			Serial.println(F("<<<DONE>>>"));
		}
		else
		{
			Serial.print(F("<<ERROR: "));
			Serial.print(error, 10);
			Serial.println(F(">>"));
		}
		
		readGPS();
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
