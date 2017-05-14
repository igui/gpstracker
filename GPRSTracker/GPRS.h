// gprs.h

#ifndef _GPRS_h
#define _GPRS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "Timer.h"
#include <SoftwareSerial.h>

const int MAX_MESSAGE_LENGTH = 32;

class GPRS {
public:
	enum State {
		// Initialization
		WAIT_FOR_AT_MODULE,
		QUERY_GPRS,
		SETUP_PDP_CONTEXT,
		SET_PDP_CONTEXT_USER_PASS,
		ACTIVATE_PDP_CONTEXT,

		// DNS Resolution 
		CONFIGURE_DNS_HOST_CONNECTION,
		START_DNS_CONNECTION,
		QUERY_DNS_CONN_STATUS_START,
		QUERY_DNS_CONN_STATUS_WAIT_FOR_CONN_STATUS,
		QUERY_DNS_CONN_STATUS_WAIT_FOR_OK,
		SEND_DNS_PACKET_DATA_SET_LENGTH,
		SEND_DNS_PACKET_DATA_WRITE,
		READ_DNS_SDATA_PREFIX,
		READ_DNS_HEADER_STATUS,
		SKIP_DNS_SKIP_RESPONSE_QUERY,
		READ_DNS_FIRST_ANSWER,
		READ_DNS_ANSWER_END,

		// Packet transmition
		CONFIGURE_REMOTE_HOST,
		START_TCP_CONNECTION,
		QUERY_CONN_STATUS_START,
		QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS,
		QUERY_CONN_STATUS_WAIT_FOR_OK,
		SEND_PACKET_DATA_SET_LENGTH,
		SEND_PACKET_DATA_WRITE,
		SEND_PACKET_DATA_RECEIVED,
		WAIT_FOR_CONN_CLOSE,

		// Others
		DONE,
	};

	enum Error {
		NO_ERROR = 0,
		QUERY_CONN_STATUS_ERROR,
		QUERY_CONN_STATUS_INVALID_NUMBER,
		REQUEST_NOT_CONFIGURED,
		APN_NOT_CONFIGURED,
		READ_PAST_RESPONSE_END,
		DNS_NO_ANSWER,
		TIMEOUT
	};

private:
	// GPRS Serial to write commands into
	SoftwareSerial &cellSerial;

	// APN info
	const char *apn;
	const char *apn_user;
	const char *apn_password;
	const char *dns;

	// Request
	unsigned char ip[4];
	const char *host;
	const char *path;

	// Operation Status
	State state;
	Error lastError;
	
	// Read message Status
	String currentMessage;
	String lastMessage;

	// Used on quertConnStatus* Methods
	int connectionStatus;

	// Used when parsing binaryResponses
	int responseRemainingBytes;
	
	// Used when parsing DNS responses
	int currentPartRequestedBytes;
	int currentPartReadBytes;
	bool readingHighHexChar;
	char currentHexByte[3];
	char currentPart[MAX_MESSAGE_LENGTH];

	// Used to calculate timeout
	Timer timer;
public:
	GPRS(SoftwareSerial &cellSerial, const char *apn, const char *apn_user, const char *apn_password, const char *dns);
	void beginRequest(const char *host, const char *path);
	State getState();
	Error getLastError();
	void loop(char incomingChar);
	void loopNoInput();
private:
	void checkParameters();
	void processIncomingASCII(char incomingChar);
	Error processIncomingHex(char incomingChar, bool ignore);

	// Variants of simple step with different number of characters
	void simpleStep(
		char incomingChar,
		const char *expectedMessage,
		State nextState, 
		int timeout,
		const char *nextMessage = NULL,
		const char *nextMessage2 = NULL,
		const char *nextMessage3 = NULL,
		const char *nextMessage4 = NULL,
		const char *nextMessage5 = NULL);
	
	void queryConnStatusWaitForConnection(char incomingChar, State nextState);
	void queryConnStatusWaitForOK(char incomingChar, const char *connectionId, int dataLength, State noConnState, State openConnState);
	void sendPacketDataSendData(char incomingChar);
	void sendDNSRequest(char incomingChar);
	int  getDNSRequestPacketLength();
	int  getRawRequestDataLength();
	void readDNSSDataPrefix(char incomingChar);
	void readDNSHeaderStatus(char incomingChar);
	void skipDNSResponseQuery(char incomingChar);
	void readDNSFirstAnswer(char incomingChar);
	void readUntilEndLine(char incomingChar);
	void configureRemoteHost(char incomingChar);
	void error(Error lastError);
	void success(State newState, unsigned long timeout);

	static void printCharSerial(char c);
};
#endif

