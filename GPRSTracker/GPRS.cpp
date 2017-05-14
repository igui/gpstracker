// 
// 
// 

#include "GPRS.h"
#include <avr/pgmspace.h>

// GPRS constants
const char *HTTP_USER_AGENT = "Igui's GPRS_CLIENT 0.0.1";
const char *DNS_PORT = "53";
const int DNS_ANSWER_LENGTH = 16;

const char DNS_REQUEST_HEADER_BYTES[] = {
	0x00, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

const char DNS_REQUEST_SUFFIX_BYTES[] = { 0x00, 0x00,
	0x01, 0x00, 0x01
};

const unsigned long LONG_TIMEOUT = 20 * 1000;
const unsigned long SHORT_TIMEOUT = 10 * 1000;

GPRS::GPRS(SoftwareSerial &cellSerial, const char *apn, const char *apn_user, const char *apn_password, const char *dns) :
	cellSerial(cellSerial),
	apn(apn),
	apn_user(apn_user),
	apn_password(apn_password),
	dns(dns),
	state(DONE),
	lastError(NO_ERROR),
	connectionStatus(0),
	responseRemainingBytes(0),
	currentPartRequestedBytes(0),
	currentPartReadBytes(0),
	readingHighHexChar(true)
{
	currentHexByte[0] = '\0';
}

void GPRS::beginRequest(const char *host, const char *path)
{
	lastError = NO_ERROR;
	ip[0] = ip[1] = ip[2] = ip[3] = 0;
	this->host = host;
	this->path = path;
	connectionStatus = 0;
	responseRemainingBytes = 0;
	currentPartRequestedBytes = 0;
	currentPartReadBytes = 0;
	readingHighHexChar = true;
	currentHexByte[0] = '\0';
	timer.removeTimeout();
	checkParameters();
}

GPRS::State GPRS::getState()
{
	return state;
}

GPRS::Error GPRS::getLastError()
{
	return lastError;
}

void GPRS::processIncomingASCII(char incoming_char)
{
	currentMessage += incoming_char;
	int current_message_length = currentMessage.length();

	if (incoming_char == '\n' &&
		current_message_length > 1 &&
		currentMessage[current_message_length - 2] == '\r')
	{
		String result = currentMessage.substring(0, current_message_length - 2);
		currentMessage = "";
		lastMessage = result;
	}
	else
	{
		lastMessage = "";
	}
}

GPRS::Error GPRS::processIncomingHex(char incomingChar, bool ignore)
{
	if (responseRemainingBytes <= 0)
	{
		error(READ_PAST_RESPONSE_END);
		return lastError;
	}

	lastMessage = "";
	if (readingHighHexChar)
	{
		if (!ignore)
		{
			currentHexByte[0] = incomingChar;
			currentHexByte[1] = '\0';
			currentHexByte[2] = '\0';
		}
		readingHighHexChar = false;
	}
	else
	{
		unsigned char currentByte;
		if (!ignore)
		{
			currentHexByte[1] = incomingChar;
			currentHexByte[2] = '\0';
			currentByte = strtol(currentHexByte, NULL, HEX);
		
			if (currentPartReadBytes <= MAX_MESSAGE_LENGTH)
			{
				currentPart[currentPartReadBytes] = currentByte;
			}
		}

		readingHighHexChar = true;
		responseRemainingBytes--;
		currentPartReadBytes++;
	}

	return NO_ERROR;
}


void GPRS::checkParameters()
{
	if (!apn || !apn_user || !apn_password || !dns)
	{
		error(APN_NOT_CONFIGURED);
	}
	else if (!host || !path)
	{
		error(REQUEST_NOT_CONFIGURED);
	}
	else
	{
		success(WAIT_FOR_AT_MODULE, LONG_TIMEOUT);
	}
}


void GPRS::simpleStep(
	char incomingChar,
	const char *expectedCellMessage,
	State nextState,
	int timeout,
	const char *nextCommand1,
	const char *nextCommand2,
	const char *nextCommand3,
	const char *nextCommand4,
	const char *nextCommand5)
{
	processIncomingASCII(incomingChar);

	if (lastMessage == expectedCellMessage)
	{
		success(nextState, timeout);
		if (nextCommand1)
		{
			cellSerial.print(nextCommand1);
			Serial.print(nextCommand1);
		}
		if (nextCommand2)
		{
			cellSerial.print(nextCommand2);
			Serial.print(nextCommand2);
		}
		if (nextCommand3)
		{
			cellSerial.print(nextCommand3);
			Serial.print(nextCommand3);
		}
		if (nextCommand4)
		{
			cellSerial.print(nextCommand4);
			Serial.print(nextCommand4);
		}
		if (nextCommand5)
		{
			cellSerial.print(nextCommand5);
			Serial.print(nextCommand5);
		}
	}
}

void GPRS::queryConnStatusWaitForConnection(char incomingChar, GPRS::State nextState)
{
	processIncomingASCII(incomingChar);

	if (!lastMessage.startsWith("+SOCKSTATUS:"))
	{
		return;
	}

	
	int first_comma = lastMessage.indexOf(',');
	if (first_comma < 0)
	{
		error(QUERY_CONN_STATUS_ERROR);
		return;
	}

	connectionStatus = lastMessage.substring(first_comma + 1).toInt();
	success(nextState, SHORT_TIMEOUT);
}

int GPRS::getRawRequestDataLength()
{
	return 4 + strlen(path) + 11 +
		6 + strlen(host) + 2 +
		12 + strlen(HTTP_USER_AGENT) + 4;
}

void GPRS::readDNSSDataPrefix(char incomingChar)
{
	processIncomingASCII(incomingChar);
	if (currentMessage.startsWith("+SDATA:2,") &&
		currentMessage.length() > 10 &&
		currentMessage.endsWith(",")
		)
	{
		responseRemainingBytes = currentMessage.substring(9).toInt();
		success(READ_DNS_HEADER_STATUS, SHORT_TIMEOUT);
		currentMessage = "";
		lastMessage = "";
		currentPartRequestedBytes = sizeof(DNS_REQUEST_HEADER_BYTES);
	}
}

void GPRS::readDNSHeaderStatus(char incomingChar)
{
	if(processIncomingHex(incomingChar, false) != NO_ERROR)
	{
		return;
	}

	if (currentPartReadBytes == currentPartRequestedBytes)
	{
		Serial.println();
		
		Serial.println(F("Read DNS Header"));
		
		if (currentPart[0] != 0 || currentPart[1] != 2)
		{
			Serial.print(F("Invalid DNS identifier: "));
			Serial.print(currentPart[0], HEX);
			Serial.println(currentPart[1], HEX);
			error(DNS_NO_ANSWER);
			return;
		}
		if (currentPart[6] == 0 && currentPart[7] == 0)
		{
			Serial.println(F("Got no DNS answers"));
			error(DNS_NO_ANSWER);
			return;
		}

		currentPartRequestedBytes = 1 + strlen(host) + sizeof(DNS_REQUEST_SUFFIX_BYTES);
		currentPartReadBytes = 0;
		success(SKIP_DNS_SKIP_RESPONSE_QUERY, SHORT_TIMEOUT);
	}
}

void GPRS::skipDNSResponseQuery(char incomingChar)
{
	if (processIncomingHex(incomingChar, true) != NO_ERROR)
	{
		return;
	}

	if (currentPartReadBytes == currentPartRequestedBytes)
	{
		Serial.println();
		Serial.println(F("Skipped Response Query"));
		currentPartRequestedBytes = DNS_ANSWER_LENGTH;
		currentPartReadBytes = 0;
		success(READ_DNS_FIRST_ANSWER, SHORT_TIMEOUT);
	}
}

void GPRS::readDNSFirstAnswer(char incomingChar)
{
	if (processIncomingHex(incomingChar, false) != NO_ERROR)
	{
		return;
	}

	if (currentPartReadBytes == currentPartRequestedBytes)
	{
		Serial.println();
		Serial.println(F("Read answer section"));

		if (currentPart[2] != 0 || currentPart[3] != 1)
		{
			Serial.print(F("Expected A response type: "));
			Serial.print(currentPart[2], HEX);
			Serial.println(currentPart[3], HEX);
			error(DNS_NO_ANSWER);
			return;
		}

		if (currentPart[4] != 0 || currentPart[5] != 1)
		{
			Serial.print(F("Expected IN class: "));
			Serial.print(currentPart[4], HEX);
			Serial.println(currentPart[5], HEX);
			error(DNS_NO_ANSWER);
			return;
		}

		if (currentPart[10] != 0 || currentPart[11] != 4)
		{
			Serial.print(F("Expected 4 bytes of response: "));
			Serial.print(currentPart[10], HEX);
			Serial.println(currentPart[11], HEX);
			error(DNS_NO_ANSWER);
			return;
		}

		ip[0] = currentPart[12];
		ip[1] = currentPart[13];
		ip[2] = currentPart[14];
		ip[3] = currentPart[15];

		Serial.print(F("Got IP: "));
		Serial.print(ip[0], 10);
		Serial.print(".");
		Serial.print(ip[1], 10);
		Serial.print(".");
		Serial.print(ip[2], 10);
		Serial.print(".");
		Serial.println(ip[3], 10);

		currentPartRequestedBytes = 0;
		currentPartReadBytes = 0;
		success(READ_DNS_ANSWER_END, SHORT_TIMEOUT);
	}
}

void GPRS::readUntilEndLine(char incomingChar)
{
	if (incomingChar == '\r' || incomingChar == '\n')
	{
		Serial.println();
		Serial.println(F("Read whole DNS packet"));
		cellSerial.print(F("AT+SDATASTART=2,0\r"));
		Serial.print(F("AT+SDATASTART=2,0\r"));
		success(CONFIGURE_REMOTE_HOST, SHORT_TIMEOUT);
	}
}

void GPRS::configureRemoteHost(char incomingChar)
{
	processIncomingASCII(incomingChar);

	if (lastMessage == "OK")
	{
		success(START_TCP_CONNECTION, SHORT_TIMEOUT);

		cellSerial.print(F("AT+SDATACONF=1,\"TCP\",\""));
		Serial.print(F("AT+SDATACONF=1,\"TCP\",\""));

		for (int i = 0; i < 4; ++i)
		{
			cellSerial.print(ip[i], DEC);
			Serial.print(ip[i], DEC);
			if (i < 3)
			{
				cellSerial.print(".");
				Serial.print(".");
			}
		}
		cellSerial.print(F("\",80\r"));
		Serial.print(F("\",80\r"));
	}
}

void GPRS::error(Error lastError)
{
	state = DONE;
	this->lastError = lastError;
	timer.removeTimeout();
}

void GPRS::success(State newState, unsigned long timeout)
{
	state = newState;
	lastError = NO_ERROR;
	if(timeout > 0)
	{
		timer.setTimeout(timeout);
	}
	else
	{
		timer.removeTimeout();
	}
}

void GPRS::queryConnStatusWaitForOK(char incomingChar, const char *connectionId, int dataLength, State onNoConn, State onYesConn)
{
	processIncomingASCII(incomingChar);

	const int CONNECTION_STATUS_LOOP_INTERVAL = 1000;

	if (lastMessage != F("OK"))
	{
		return;
	}

	switch (connectionStatus)
	{
	case(0):
		delay(CONNECTION_STATUS_LOOP_INTERVAL);
		cellSerial.print(F("AT+SDATASTATUS="));
		Serial.print(F("AT+SDATASTATUS="));
		cellSerial.print(connectionId);
		Serial.print(connectionId);
		cellSerial.print("\r");
		success(onNoConn, SHORT_TIMEOUT);
		break;
	case(1):
		cellSerial.print(F("AT+SDATATSEND="));
		Serial.print(F("AT+SDATATSEND="));
		cellSerial.print(connectionId);
		Serial.print(connectionId);
		cellSerial.print(F(","));
		Serial.print(F(","));
		cellSerial.print(String(dataLength));
		Serial.print(String(dataLength));
		cellSerial.print(F("\r"));
		success(onYesConn, SHORT_TIMEOUT);
		break;
	default:
		error(QUERY_CONN_STATUS_INVALID_NUMBER);
		break;
	}
}

void GPRS::sendPacketDataSendData(char incomingChar)
{
	processIncomingASCII(incomingChar);

	if (currentMessage != F(">"))
	{
		return;
	}

	Serial.print(F("<<Sending data>>\n"));

	cellSerial.print(F("GET "));
	cellSerial.print(path);
	cellSerial.print(F(" HTTP/1.1\r\n"));
	cellSerial.print(F("Host: "));
	cellSerial.print(host);
	cellSerial.print(F("\r\nUser-Agent: "));
	cellSerial.print(HTTP_USER_AGENT);
	cellSerial.print(F("\r\n\r\n"));
	cellSerial.write(26); // Control+Z

	success(SEND_PACKET_DATA_WRITE, SHORT_TIMEOUT);
}

void GPRS::printCharSerial(const char c)
{
	Serial.print(String(int(c >> 4), HEX));
	Serial.print(String(int(c & 0xF), HEX));
}

void GPRS::sendDNSRequest(char incomingChar)
{
	processIncomingASCII(incomingChar);

	if (currentMessage != F(">"))
	{
		return;
	}

	cellSerial.write(DNS_REQUEST_HEADER_BYTES, sizeof(DNS_REQUEST_HEADER_BYTES));
	for (unsigned int i = 0; i < sizeof(DNS_REQUEST_HEADER_BYTES); ++i)
	{
		printCharSerial(DNS_REQUEST_HEADER_BYTES[i]);
	}

	auto labelStart = host;
	while (true)
	{
		auto labelEnd = labelStart;
		while(*labelEnd != '.' && *labelEnd != '\0')
		{
			++labelEnd;
		}

		auto labelLength = labelEnd - labelStart;
		cellSerial.write(labelLength);
		printCharSerial(labelLength);

		cellSerial.write(labelStart, labelLength);
		for (auto c = labelStart; c != labelEnd; ++ c)
		{
			printCharSerial(*c);
		}
		
		if (*labelEnd == '\0')
		{
			break;
		}
		else
		{
			labelStart = labelEnd + 1;
		}
	}

	
	cellSerial.write(DNS_REQUEST_SUFFIX_BYTES, sizeof(DNS_REQUEST_SUFFIX_BYTES));
	for (unsigned int i = 0; i < sizeof(DNS_REQUEST_SUFFIX_BYTES); ++i)
	{
		printCharSerial(DNS_REQUEST_SUFFIX_BYTES[i]);
	}
	

	cellSerial.write(26); // Control+Z

	Serial.print(F(" ESC\n"));

	success(SEND_DNS_PACKET_DATA_WRITE, SHORT_TIMEOUT);
}

int GPRS::getDNSRequestPacketLength()
{
	// 1 is the octet for the first label
	return sizeof(DNS_REQUEST_HEADER_BYTES) + 1 + strlen(host) + sizeof(DNS_REQUEST_SUFFIX_BYTES);
}

void GPRS::loop(char incomingChar) {
	Serial.print(incomingChar); 
	if (timer.wasExpired())
	{
		error(TIMEOUT);
		return;
	}

	switch (state)
	{
	case(WAIT_FOR_AT_MODULE):
		simpleStep(
			incomingChar,
			"+SIND: 4",
			QUERY_GPRS,
			LONG_TIMEOUT,
			"AT+CGATT?\r"
		);
		break;
	case(QUERY_GPRS):
		simpleStep(
			incomingChar,
			"OK",
			SETUP_PDP_CONTEXT,
			SHORT_TIMEOUT,
			"AT+CGDCONT=1,\"IP\",\"", apn, "\"\r");
		break;
	case(SETUP_PDP_CONTEXT):
		simpleStep(
			incomingChar,
			"OK",
			SET_PDP_CONTEXT_USER_PASS,
			SHORT_TIMEOUT,
			"AT+CGPCO=0,\"", apn_user, "\",\"", apn_password, "\", 1\r"
		);
		break;
	case(SET_PDP_CONTEXT_USER_PASS):
		simpleStep(
			incomingChar,
			"OK",
			CONFIGURE_DNS_HOST_CONNECTION,
			LONG_TIMEOUT,
			"AT+CGACT=1,1\r");
		break;

	case(CONFIGURE_DNS_HOST_CONNECTION):
		simpleStep(
			incomingChar,
			"OK",
			START_DNS_CONNECTION,
			LONG_TIMEOUT,
			"AT+SDATACONF=2,\"UDP\",\"",  dns, "\",", DNS_PORT, "\r"
		);
		break;
	case(START_DNS_CONNECTION):
		simpleStep(
			incomingChar,
			"OK",
			QUERY_DNS_CONN_STATUS_START,
			SHORT_TIMEOUT,
			"AT+SDATASTART=2,1\r"
		);
		break;
	case(QUERY_DNS_CONN_STATUS_START):
		simpleStep(
			incomingChar,
			"OK",
			QUERY_DNS_CONN_STATUS_WAIT_FOR_CONN_STATUS,
			SHORT_TIMEOUT,
			"AT+SDATASTATUS=2\r"
		);
		break;
	case(QUERY_DNS_CONN_STATUS_WAIT_FOR_CONN_STATUS):
		queryConnStatusWaitForConnection(incomingChar, QUERY_DNS_CONN_STATUS_WAIT_FOR_OK);
		break;
	case(QUERY_DNS_CONN_STATUS_WAIT_FOR_OK):
		queryConnStatusWaitForOK(
			incomingChar,
			"2",
			getDNSRequestPacketLength(),
			QUERY_DNS_CONN_STATUS_WAIT_FOR_CONN_STATUS,
			SEND_DNS_PACKET_DATA_SET_LENGTH);
		break;
	case(SEND_DNS_PACKET_DATA_SET_LENGTH):
		sendDNSRequest(incomingChar);
		break;

	case(SEND_DNS_PACKET_DATA_WRITE):
		simpleStep(
			incomingChar,
			"OK",
			READ_DNS_SDATA_PREFIX,
			SHORT_TIMEOUT
		);
		break;
	case(READ_DNS_SDATA_PREFIX):
		readDNSSDataPrefix(incomingChar);
		break;
	case(READ_DNS_HEADER_STATUS):
		readDNSHeaderStatus(incomingChar);
		break;
	case(SKIP_DNS_SKIP_RESPONSE_QUERY):
		skipDNSResponseQuery(incomingChar);
		break;
	case(READ_DNS_FIRST_ANSWER):
		readDNSFirstAnswer(incomingChar);
		break;
	case(READ_DNS_ANSWER_END):
		readUntilEndLine(incomingChar);
		break;

	case(CONFIGURE_REMOTE_HOST):
		configureRemoteHost(incomingChar);
		break;
	case(START_TCP_CONNECTION):
		simpleStep(
			incomingChar,
			"OK",
			QUERY_CONN_STATUS_START,
			SHORT_TIMEOUT,
			"AT+SDATASTART=1,1\r"
		);
		break;
	case(QUERY_CONN_STATUS_START):
		simpleStep(
			incomingChar,
			"OK",
			QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS,
			SHORT_TIMEOUT,
			"AT+SDATASTATUS=1\r"
		);
		break;
	case(QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS):
		queryConnStatusWaitForConnection(
			incomingChar,
			QUERY_CONN_STATUS_WAIT_FOR_OK
		);
		break;
	case(QUERY_CONN_STATUS_WAIT_FOR_OK):
		queryConnStatusWaitForOK(
			incomingChar,
			"1",
			getRawRequestDataLength(),
			QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS,
			SEND_PACKET_DATA_SET_LENGTH);
		break;
	case(SEND_PACKET_DATA_SET_LENGTH):
		sendPacketDataSendData(incomingChar);
		break;
	case(SEND_PACKET_DATA_WRITE):
		simpleStep(
			incomingChar,
			"OK",
			SEND_PACKET_DATA_RECEIVED,
			SHORT_TIMEOUT
		);
		break;
	case(SEND_PACKET_DATA_RECEIVED):
		simpleStep(
			incomingChar,
			"+STCPD:1",
			DONE,
			SHORT_TIMEOUT,
			"AT+SDATATREAD=1\r"
		);
		break;
	default:
		break;
	}
}

void GPRS::loopNoInput()
{
	if (timer.wasExpired())
	{
		error(TIMEOUT);
	}
}


