// 
// 
// 

#include "GPRS.h"

// GPRS constants
const int CONNECTION_STATUS_LOOP_INTERVAL = 1000;
const char *HTTP_USER_AGENT = "Igui's GPRS_CLIENT 0.0.1";

#define T_A 1 //Ipv4 address

GPRS::GPRS(SoftwareSerial &cellSerial, const char *apn, const char *apn_user, const char *apn_password, const char *dns):
	cellSerial(cellSerial),
	apn(apn),
	apn_user(apn_user),
	apn_password(apn_password),
	dns(dns),
	state(DONE),
	connectionStatus(0)
{

}

void GPRS::beginRequest(const char *ip, const char *host, const String &path)
{
	this->ip = ip;
	this->host = host;
	this->path = path;
	if (checkParameters())
	{
		state = WAIT_FOR_AT_MODULE;
	}
	else
	{
		state = DONE;
	}
}



void GPRS::makeRequest(const char * ip, const char * host, const String & path)
{
	this->ip = ip;
	this->host = host;
	this->path = path;
	if (checkParameters())
	{
		state = MAKING_REQUEST;
	}
	else
	{
		state = DONE;
		return;
	}

	while (true)
	{
		while (cellSerial.available() > 0)
		{
			char incomingChar = cellSerial.read();
			Serial.print(incomingChar); //Print the incoming character to the terminal.
		}
	}
}

void GPRS::processIncomingChar(char incoming_char)
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


bool GPRS::checkParameters()
{
	if (!apn || !apn_user || !apn_password || !dns)
	{
		Serial.print("<<checkParameters: ERROR: GPS request was not set >>\n");
		return false;
	}
	else if (!ip || !host || !path)
	{
		Serial.print("<<checkParameters: ERROR: Request IP NOT SET >>\n");
		return false;
	}
	return true;
}

void GPRS::simpleStep(const char *expectedCellMessage, const char *sucessUserMessage, const char *nextCommand, State nextState)
{
	if (lastMessage == expectedCellMessage)
	{
		Serial.print("<<");
		Serial.print(sucessUserMessage);
		Serial.print(">>");
		if (nextCommand)
		{
			cellSerial.print(nextCommand);
			Serial.print(nextCommand);
		}
		state = nextState;
	}
}

void GPRS::simpleStep(const char *expectedCellMessage, const char *sucessUserMessage, const String &nextCommand, State nextState)
{
	simpleStep(expectedCellMessage, sucessUserMessage, nextCommand.c_str(), nextState);
}

void GPRS::queryConnStatusWaitForConnection()
{
	if (!lastMessage.startsWith("+SOCKSTATUS:"))
	{
		return;
	}

	int first_comma = lastMessage.indexOf(',');
	if (first_comma < 0)
	{
		Serial.print("<<queryConnStatusWaitForConnection: ERROR: unexpected message>>\n");
		state = DONE;
		return;
	}

	connectionStatus = lastMessage.substring(first_comma + 1).toInt();
	state = QUERY_CONN_STATUS_WAIT_FOR_OK;
}

int GPRS::getRawRequestDataLength()
{
	return 4 + path.length() + 11 +
		6 + strlen(host) + 2 +
		12 + strlen(HTTP_USER_AGENT) + 4;
}

void GPRS::queryConnStatusWaitForOK()
{
	if (lastMessage != "OK")
	{
		return;
	}

	switch (connectionStatus)
	{
	case(0):
		Serial.print("<<Not connected yet>>\n");
		delay(CONNECTION_STATUS_LOOP_INTERVAL);
		cellSerial.print("AT+SDATASTATUS=1\r");
		state = QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS;
		break;
	case(1):
		Serial.print("<<Start sending data>>");
		cellSerial.print("AT+SDATATSEND=1,");
		cellSerial.print(String(getRawRequestDataLength()));
		cellSerial.print("\r");
		state = SEND_PACKET_DATA_SET_LENGTH;
		break;
	default:
		Serial.print("<<queryConnStatusWaitForConnection: ERROR: unknown connection status>>\n");
		state = DONE;
		break;
	}
}

void GPRS::sendPacketDataSendData()
{
	if (currentMessage != ">")
	{
		return;
	}

	Serial.print("<<Sending data>>\n");

	cellSerial.print("GET ");
	cellSerial.print(path);
	cellSerial.print(" HTTP/1.1\r\n");
	cellSerial.print("Host: ");
	cellSerial.print(host);
	cellSerial.print("\r\nUser-Agent: ");
	cellSerial.print(HTTP_USER_AGENT);
	cellSerial.print("\r\n\r\n");
	cellSerial.write(26); // Control+Z

	state = SEND_PACKET_DATA_RECEIVED;
}

void GPRS::loop(char incomingChar) {
	if (state != DONE)
	{
		processIncomingChar(incomingChar);
	}

	switch (state)
	{
	case(WAIT_FOR_AT_MODULE):
		simpleStep("+SIND: 4", "Query GPRS_STATUS", "AT+CGATT?\r", QUERY_GPRS);
		break;
	case(QUERY_GPRS):
		simpleStep(
			"OK",
			"Setup PDP Context",
			"AT+CGDCONT=1,\"IP\",\"" + String(apn) + "\"\r",
			SETUP_PDP_CONTEXT
		);
		break;
	case(SETUP_PDP_CONTEXT):
		simpleStep(
			"OK",
			"Set PDP Context user and password",
			"AT+CGPCO=0,\"" + String(apn_user) + "\",\"" + String(apn_password) + "\", 1\r",
			SET_PDP_CONTEXT_USER_PASS
		);
		break;
	case(SET_PDP_CONTEXT_USER_PASS):
		simpleStep("OK", "Activate PDP Context", "AT+CGACT=1,1\r", CONFIGURE_REMOTE_HOST);
		break;
	case(CONFIGURE_REMOTE_HOST):
		simpleStep(
			"OK",
			"Set Remote Host and Port for TCP Connection ",
			"AT+SDATACONF=1,\"TCP\",\"" + String(ip) + "\",80\r",
			START_TCP_CONNECTION
		);
		break;
	case(START_TCP_CONNECTION):
		simpleStep(
			"OK",
			"Start TCP Connection",
			"AT+SDATASTART=1,1\r",
			QUERY_CONN_STATUS_START
		);
		break;
	case(QUERY_CONN_STATUS_START):
		simpleStep(
			"OK",
			"Query Connection Status",
			"AT+SDATASTATUS=1\r",
			QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS
		);
		break;
	case(QUERY_CONN_STATUS_WAIT_FOR_CONN_STATUS):
		queryConnStatusWaitForConnection();
		break;
	case(QUERY_CONN_STATUS_WAIT_FOR_OK):
		queryConnStatusWaitForOK();
		break;
	case(SEND_PACKET_DATA_SET_LENGTH):
		sendPacketDataSendData();
		break;
	case(SEND_PACKET_DATA_WRITE):
		simpleStep(
			"OK",
			"Waiting for data receive confirmation",
			"",
			SEND_PACKET_DATA_RECEIVED
		);
		break;
	case(SEND_PACKET_DATA_RECEIVED):
		// TODO don't query for data status. It is useless
		simpleStep(
			"+STCPD:1",
			"Waiting for data from host",
			"AT+SDATATREAD=1\r",
			DONE
		);
		break;
	default:
		break;
	}
}


struct DNS_HEADER
{
	unsigned short id; // identification number

	unsigned char rd : 1; // recursion desired
	unsigned char tc : 1; // truncated message
	unsigned char aa : 1; // authoritive answer
	unsigned char opcode : 4; // purpose of message
	unsigned char qr : 1; // query/response flag

	unsigned char rcode : 4; // response code
	unsigned char cd : 1; // checking disabled
	unsigned char ad : 1; // authenticated data
	unsigned char z : 1; // its z! reserved
	unsigned char ra : 1; // recursion available

	unsigned short q_count; // number of question entries
	unsigned short ans_count; // number of answer entries
	unsigned short auth_count; // number of authority entries
	unsigned short add_count; // number of resource entries
};

//Constant sized fields of query structure
struct QUESTION
{
	unsigned short qtype;
	unsigned short qclass;
};

//Constant sized fields of the resource record structure
#pragma pack(push, 1)
struct R_DATA
{
	unsigned short type;
	unsigned short _class;
	unsigned int ttl;
	unsigned short data_len;
};
#pragma pack(pop)

//Pointers to resource record contents
struct RES_RECORD
{
	struct R_DATA *resource;
	unsigned char rdata[16];
};

//Structure of a Query
typedef struct
{
	unsigned char *name;
	struct QUESTION *ques;
} QUERY;


