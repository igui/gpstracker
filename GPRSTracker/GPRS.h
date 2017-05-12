// gprs.h

#ifndef _GPRS_h
#define _GPRS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <SoftwareSerial.h>

class GPRS {
private:
	enum State {
		WAIT_FOR_AT_MODULE,
		QUERY_GPRS,
		SETUP_PDP_CONTEXT,
		SET_PDP_CONTEXT_USER_PASS,
		ACTIVATE_PDP_CONTEXT,
		START_DNS_CONNECTION,
		QUERY_DNS_CONN_STATUS_START,
		QUERY_DNS_CONN_STATUS_WAIT_FOR_CONN_STATUS,
		QUERY_DNS_CONN_STATUS_WAIT_FOR_OK,
		SEND_DNS_PACKET_DATA_SET_LENGTH,
		SEND_DNS_PACKET_DATA_WRITE,
		SEND_DNS_PACKET_DATA_RECEIVED,
		WAIT_DNS_FOR_CONN_CLOSE,
		CONFIGURE_REMOTE_HOST,
		START_TCP_CONNECTION,
		QUERY_CONN_STATUS_START,
		QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS,
		QUERY_CONN_STATUS_WAIT_FOR_OK,
		SEND_PACKET_DATA_SET_LENGTH,
		SEND_PACKET_DATA_WRITE,
		SEND_PACKET_DATA_RECEIVED,
		WAIT_FOR_CONN_CLOSE,
		DONE,
		
		MAKING_REQUEST,
	};

	SoftwareSerial &cellSerial;

	// APN info
	const char *apn;
	const char *apn_user;
	const char *apn_password;
	const char *dns;

	// Request
	const char *ip;
	const char *host;
	String path;

	// GPRS Status
	State state;
	String currentMessage;
	String lastMessage;
	int connectionStatus;

public:
	GPRS(SoftwareSerial &cellSerial, const char *apn, const char *apn_user, const char *apn_password, const char *dns);
	void beginRequest(const char *ip, const char *host, const String &path);
	void makeRequest(const char *ip, const char *host, const String &path);
	void loop(char incomingChar);
private:
	bool checkParameters();
	void processIncomingChar(char incomingChar);
	void simpleStep(const char *expectedMessage, const char *sucessUserMessage, const char *nextCommand, State nextState);
	void simpleStep(const char *expectedMessage, const char *sucessUserMessage, const String &nextCommand, State nextState);
	void queryConnStatusWaitForConnection();
	void queryConnStatusWaitForOK();
	void sendPacketDataSendData();
	int  getRawRequestDataLength();
};
#endif

