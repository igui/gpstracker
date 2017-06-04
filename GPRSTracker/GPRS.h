// gprs.h

#ifndef _GPRS_h
#define _GPRS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

#include "Timer.h"
#include <SoftwareSerial.h>

const int MAX_MESSAGE_LENGTH = 32;

struct StringHelper {
	enum {
		CHAR_POINTER,
		FLASH_POINTER,
	} type;

	union {
		const char *memString;
		const __FlashStringHelper *flashString;
	} payload;

	StringHelper(const char *);
	StringHelper(const __FlashStringHelper *);
	StringHelper operator()(const char *);
	StringHelper operator()(const __FlashStringHelper *);
	void printAndSerial(SoftwareSerial &serial) const;
};

class GPRS {
public:
	enum Error {
		NO_ERROR = 0,
		QUERY_CONN_STATUS_ERROR,
		QUERY_CONN_STATUS_INVALID_NUMBER,
		REQUEST_NOT_CONFIGURED,
		APN_NOT_CONFIGURED,
		READ_PAST_RESPONSE_END,
		DNS_NO_ANSWER,
		PDP_NOT_PREPARED,
		TIMEOUT
	};

private:
	enum State {
		// Initialization
		WAIT_FOR_AT_MODULE,
		QUERY_GPRS,
		SETUP_PDP_CONTEXT,
		SET_PDP_CONTEXT_USER_PASS,

		// Begin Request Begin steps: Reactivate PDP
		BEGIN_REQUEST_DEACTIVATE_PDP,
		BEGIN_REQUEST_REACTIVATE_PDP,

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
		READ_ONE_DNS_ANSWER,
		READ_DNS_ANSWER_END,
		SKIP_DNS_ANSWER,
		READ_DNS_ANSWER_END_ERROR,
		CLOSE_DNS_CONNECTION,

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

		// Used when the GPRS Module is idle
		DEAD
	};

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

	// Used if PDP context was set
	bool pdpWasSetUp;

	// Used to calculate timeout
	Timer timer;
public:
	/**
	 * Inits the GPRS with an APN settings, and a DNS as a string like "8.8.8.8"
	 * The invoker MUST call to loop or loopNoInput when a character comes from
	 * the serial port
	 */
	GPRS(SoftwareSerial &cellSerial, const char *apn, const char *apn_user, const char *apn_password, const char *dns);
	
	/**
	 * Returns true if the Module is ready for beginRequest() commands
	 */
	bool readyForCommands();

	/**
	*  Returns any error ocurred during the call to readyForCommands().
	*  Errors are generally non recoverable so the GPRS module has to be
	*  Hard reset after this. :(
	*/
	Error getLastError();

	/**
	 * Initiates a GET Request. Host must be a name "example.com" and path
	 * a valid URL encoded path "/some?a=1&b=2"
	 */
	Error beginRequest(const char *host, const char *path);

	/**
	 * Read from the cell serial port and behaves accordingly 
	 * Should be called in each loop() iteration
	 */
	void loop();

	/**
	 * Cancels any previous command and disables sending 
	 * Further requests to the module using this object.
	 */
	void kill();
private:
	// Implements the state machine state processing using the
	// incoming char as input
	void behaviour(char incomingChar);
	// Updates timers, if no input came from the Serial port
	void behaviourNoInput();

	// Checks if the beginRequest Parameters are correct
	void checkParameters();

	// Process one character and update the incoming char buffer
	void processIncomingASCII(char incomingChar);

	// Process one quad-bit represented as a Hex character and
	// Store in currentPart (only for DNS answer resolution)
	Error processIncomingHex(char incomingChar, bool ignore);

	// Variants of simple step with different number of messages 
	// Sent to the cellSerialPort
	void simpleStep(
		char incomingChar,
		const __FlashStringHelper *expectedMessage,
		State nextState, 
		int nextStateTimeout,
		const StringHelper &nextMessage = (char *)NULL,
		const StringHelper &nextMessage2 = (char *)NULL,
		const StringHelper &nextMessage3 = (char *)NULL,
		const StringHelper &nextMessage4 = (char *)NULL,
		const StringHelper &nextMessage5 = (char *)NULL);
	
	// Short functions that implements the behaviour

	void waitForSetUp(char incomingChar);
	void queryConnStatusWaitForConnection(char incomingChar, State nextState);
	void queryConnStatusWaitForOK(char incomingChar, const char *connectionId, int dataLength, State noConnState, State openConnState);
	void sendPacketDataSendData(char incomingChar);
	void sendDNSRequest(char incomingChar);
	int  getDNSRequestPacketLength();
	int  getRawRequestDataLength();
	void readDNSSDataPrefix(char incomingChar);
	void readDNSHeaderStatus(char incomingChar);
	void setUpSkipDNSAnswer();
	void skipResponseBytes(char incomingChar, State nextState);
	void readDNSFirstAnswer(char incomingChar);
	void readUntilEndLine(char incomingChar, State nextStatus, bool hasError);
	void configureRemoteHost(char incomingChar);

	/** Resets timeouts and set the last error. Almost always that means something bad happened */
	void error(Error lastError);
	/** Advances the state and sets a timeout the timeout parameter is greater than 0 */
	void success(State newState, unsigned long timeout);

	/** Prints a the HEX representation of a Character in the output */
	static void printCharSerial(char c);

	/* Writes a PROGMEM BUFFER in the Cell and the standard Serial Port*/
	void writeProgMemBuffer(const char *progMembuffer, size_t size);
};
#endif

