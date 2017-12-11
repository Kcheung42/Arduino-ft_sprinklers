#include <WiFi101.h>
#include <ArduinoJson.h>
#include <SPI.h>

#define WIFI_SSID		"42US Guests"
#define WIFI_PASSWORD	"42Events"

#define FIREBASE_HOST	"sprink-3680f.firebaseio.com"
#define FIREBASE_AUTH	"aAxhPEhI5yKEcsQJ1AXjufYehiZ7Nm0RwdpCCBIn"

/* #define FIREBASE_HOST	"ft-sprinklers.firebaseio.com" */
/* #define FIREBASE_AUTH	"AIzaSyAiPg4F4t0kVQ4AqLZISYeJhOlQ8yf15pw" */

#define SCHEDULE_PATH	"/programSchedule.json"
#define OVERIDE_PATH	"/manualOverride.json"
#define PATH			"/.json"

#define LATCHPIN		3
#define CLOCKPIN		4
#define DATAPIN			5
#define REGISTERCOUNT	2 //Moteino IOShield has 2 daisy chained registers, if you have more adjust this number
#define LED				9 //pin connected to onboard LED
#define SERIAL_BAUD		115200
#define SERIAL_EN		//comment out if you don't want any serial output

#ifdef SERIAL_EN
	#define DEBUG(input)	 Serial.print(input)
	#define DEBUGln(input) Serial.println(input)
#else
	#define DEBUG(input)
	#define DEBUGln(input)
#endif
#define SCHEDULE_PATH	"/programSchedule.json"
#define OVERIDE_PATH	"/manualOverride.json"
#define PATH			"/.json"

#define LATCHPIN		3
#define CLOCKPIN		4
#define DATAPIN			5
#define REGISTERCOUNT	2 //Moteino IOShield has 2 daisy chained registers, if you have more adjust this number
#define LED				9 //pin connected to onboard LED
#define SERIAL_BAUD		115200
#define SERIAL_EN		//comment out if you don't want any serial output

#ifdef SERIAL_EN
	#define DEBUG(input)	 Serial.print(input)
	#define DEBUGln(input) Serial.println(input)
#else
	#define DEBUG(input)
	#define DEBUGln(input)
#endif

WiFiSSLClient client;
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 60L * 1000L; //60 seconds
int status = WL_IDLE_STATUS;

/* moteino */
byte programZone[7][32]; //used to store program data
unsigned int programSeconds[7][32]; //used to store program data
byte programLength=0; //how many zones in this program
byte programPointer=0; //keeps track of active zone (when >=0)
unsigned long programZoneStart=0;
unsigned int seconds=0;
byte whichZone;

/******************************************************/
/**              funtion prototypes                  **/
/******************************************************/
void setup();
void connectWiFi();
void printWifiStatus();
void httpRequest();
void httpStream();
String getResponse();
String getPath(String streamEvent);
boolean getStatus(String streamEvent);
byte getZone(String path);
void loop();
void printMacAddress();
void listNetworks();
void printEncryptionType(int thisType);
void zonesOFF();
void zoneON(byte which);
void registersClear();
void registersAllOn();
void registersWriteBit(byte whichPin);
void registerWriteBytes(const void* buffer, byte byteCount);
void Blink(byte PIN, byte DELAY_MS);
String getData(String streamEvent);
boolean boolstring( String b );

void setup()
{
	Serial.begin(SERIAL_BAUD);
	pinMode(LATCHPIN, OUTPUT);
	pinMode(DATAPIN, OUTPUT);
	pinMode(CLOCKPIN, OUTPUT);
	pinMode(LED, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);

	//Initialize serial and wait for port to open:
	Serial.begin(9600);
	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB port only
	}

	// check for the presence of the shield:
	if (WiFi.status() == WL_NO_SHIELD) {
		DEBUGln("WiFi shield not present");
		// don't continue:
		while (true);
	}
	
	// Print WiFi MAC address:
	printMacAddress();

	// scan for existing networks:
	DEBUGln("Scanning available networks...");
	listNetworks();
	connectWiFi();
	/* httpRequest(); */
	httpStream();
}

void connectWiFi()
{
	DEBUGln("Connecting to WiFi");
	while (status != WL_CONNECTED){
		status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		delay(1000);
		DEBUG(".");
	}
	DEBUGln();
	DEBUG("connected: ");
	printWifiStatus();
}

void printWifiStatus()
{
	// print the SSID of the network you're attached to:
	DEBUG("SSID: ");
	DEBUGln(WiFi.SSID());

	// print your WiFi shield's IP address:
	IPAddress ip = WiFi.localIP();
	DEBUG("IP Address: ");
	DEBUGln(ip);

	// print the received signal strength:
	long rssi = WiFi.RSSI();
	DEBUG("signal strength (RSSI):");	
	DEBUG(rssi);
	DEBUGln(" dBm");
}

void httpRequest()
{
	/* close any connection before send a new request. */
	client.stop();

	DEBUGln("Connecting to Firebase httpRequest()...");
	client.connect(FIREBASE_HOST, 443);
	if (client.connected()){
		DEBUGln("connected to Firebase");
		client.println("GET " SCHEDULE_PATH "?auth=" FIREBASE_AUTH " HTTP/1.1");
		client.println("Host: " FIREBASE_HOST);
		client.println("Connection: close");
		client.println();
		lastConnectionTime = millis();
	}
	else{
		DEBUGln("Error connecting to Firebase");
	}
  DEBUGln(getResponse());
}

void httpStream()
{
	/* close any connection before send a new request. */
	client.stop();

	DEBUGln("Connecting to Firebase httpStream()...");
	client.connect(FIREBASE_HOST, 443);
	if (client.connected()){
		DEBUGln("connected to Firebase");
		/* client.println("GET " OVERIDE_PATH "?auth=" FIREBASE_AUTH " HTTP/1.1"); */
		client.println("GET " PATH "?auth=" FIREBASE_AUTH " HTTP/1.1");
		client.println("Host: " FIREBASE_HOST);
		client.println("Accept: text/event-stream");
		client.println("Connection: keep-alive");
		client.println();
	}
	else{
		DEBUGln("Error connecting to Firebase");
	}
}

String getResponse()
{
	delay(1000);
	int readSize = client.available();
  String result = "";
	while(client.available()){
		char c = client.read();
		result += c;
	}
	/* DEBUG(result); */
	return (result);
}

String getPath(String streamEvent)
{
	StaticJsonBuffer<200> jsonBuffer;
	streamEvent = streamEvent.substring(19); // extract json object
	JsonObject& root = jsonBuffer.parseObject(streamEvent);
	if(!root.success())
	{
		DEBUGln("parsingObject() failed");
		return "";
	}
	String path = root["path"];
	DEBUG("path is:");
	DEBUGln(path);
	return (path);
}

String getData(String streamEvent)
{
	StaticJsonBuffer<200> jsonBuffer;
	streamEvent = streamEvent.substring(19); // extract json object
	JsonObject& root = jsonBuffer.parseObject(streamEvent);
	if(!root.success())
	{
		DEBUGln("parsingObject() failed");
		return "";
	}
	String dat = root["data"];
	DEBUG("data is:");
	DEBUGln(dat);
	return(dat);
}

const char *getSchedule(char result[])
{
	DEBUGln("result is:");
	DEBUGln(result);
	StaticJsonBuffer<20000> jsonBuffer;
	JsonArray& root = jsonBuffer.parseArray(result);
	if(!root.success())
	{
		DEBUGln("parsingObject() failed");
	}
	const char *program = root[0];
	/* String path = root["path"]; */
	/* Serial.println(path); */
	DEBUG("program is:");
	DEBUGln(program);
	return (program);
}

boolean getStatus(String streamEvent)
{
	StaticJsonBuffer<200> jsonBuffer;
	streamEvent = streamEvent.substring(19); // extract json object
	JsonObject& root = jsonBuffer.parseObject(streamEvent);
	if(!root.success())
	{
		DEBUGln("parsingObject() failed");
		return "";
	}
	boolean dat = root["data"]["active"];
	DEBUG("status is:");
	DEBUGln(dat);
	return(dat);
}

byte getZone(String path)
{
	int first = path.indexOf('/');
	int second = path.indexOf('/',first+1);
	if (path.substring(first+1, second)[0] == 'z')
		whichZone = path.substring(first+2, second).toInt();
	return(whichZone);
}

String response = "";
String result = "";
void loop()
{
	response = getResponse();
	if (response.startsWith("event: patch"))
	{
		result += response;
		DEBUGln("patching!");
		if (result.indexOf("manualOverride") != -1)
		{
			String path = getPath(result);
			String zone = path.substring(path.indexOf("manualOverride" + 14)); //trim off manualOverride
			boolean active = getStatus(result); //get zone number from zone name: (i.e z00)
			whichZone = getZone(zone);
			if (!active)
			{
				DEBUG("Turning OFF Zone: ");
				DEBUGln(whichZone);
				zonesOFF();
			}
			else if (whichZone > 0) {
				DEBUG("Turning on Zone: ");
				DEBUGln(whichZone);
				zoneON(whichZone - 1);
			}
			else {
				DEBUG("Invalid ON");
			}
		}
		else if (result.indexOf("programSchedule") != -1)
		{
			DEBUGln("handle change in schedule");
		}
		result = "";
	}
	/* else if (result.startsWith("event: put")) */
	else if (response.indexOf("event: put") != -1)
	{
		int start = response.indexOf("event: put");
		result += response.substring(start + 11);
	}
	else if (response.length() > 0)
	{
		DEBUGln("Collecting response...");
		result += response;
	}
	else if (result.startsWith("event: keep-alive"))
	{
		DEBUGln(result);
		result = "";
	}
	else if(result.startsWith("data"))
	{
	 DEBUGln("Work with schedule");
	 result = result.substring(6);
	 result.trim();
	 int str_len = result.length() + 1;
	 char result_array[str_len];
	 result.toCharArray(result_array, str_len);
	 DEBUGln(getSchedule(result_array));
	 result = "";
	}
	/* if (!client.connected()){ */
	/* 	DEBUGln(); */
	/* 	DEBUGln("disconnecting from server."); */
	/* 	client.stop(); */
	/* } */
	
	/* if (millis() - lastConnectionTime > postingInterval){ */
		/* DEBUGln("updating"); */
		/* httpRequest(); */
		/* httpStream(); */
	/* } */
}

void printMacAddress()
{
	// the MAC address of your Wifi shield
	byte mac[6];

	// print your MAC address:
	WiFi.macAddress(mac);
	Serial.print("MAC: ");
	Serial.print(mac[5], HEX);
	Serial.print(":");
	Serial.print(mac[4], HEX);
	Serial.print(":");
	Serial.print(mac[3], HEX);
	Serial.print(":");
	Serial.print(mac[2], HEX);
	Serial.print(":");
	Serial.print(mac[1], HEX);
	Serial.print(":");
	Serial.println(mac[0], HEX);
}

void listNetworks()
{
	// scan for nearby networks:
	DEBUGln("** Scan Networks **");
	int numSsid = WiFi.scanNetworks();
	if (numSsid == -1)
	{
		DEBUGln("Couldn't get a wifi connection");
		while (true);
	}

	// print the list of networks seen:
	DEBUG("number of available networks:");
	DEBUGln(numSsid);

	// print the network number and name for each network found:
	for (int thisNet = 0; thisNet < numSsid; thisNet++) {
		DEBUG(thisNet);
		DEBUG(") ");
		DEBUG(WiFi.SSID(thisNet));
		DEBUG("\tSignal: ");
		DEBUG(WiFi.RSSI(thisNet));
		DEBUG(" dBm");
		DEBUG("\tEncryption: ");
		printEncryptionType(WiFi.encryptionType(thisNet));
		Serial.flush();
	}
}

void printEncryptionType(int thisType)
{
	// read the encryption type and print out the name:
	switch (thisType) {
		case ENC_TYPE_WEP:
			DEBUGln("WEP");
			break;
		case ENC_TYPE_TKIP:
			DEBUGln("WPA");
			break;
		case ENC_TYPE_CCMP:
			DEBUGln("WPA2");
			break;
		case ENC_TYPE_NONE:
			DEBUGln("None");
			break;
		case ENC_TYPE_AUTO:
			DEBUGln("Auto");
			break;
	}
}

void stopAndResetProgram()
{
	programLength=0;
	programPointer=0;
	programZoneStart=0;
}

//all zones OFF
void zonesOFF()
{
	/* stopAndResetProgram(); //stop any running programs */
	/* if (radio.ACKRequested()) radio.sendACK(); */
	registersClear();
	/* if (radio.sendWithRetry(GATEWAYID, "ZONES:OFF", 9)) */
	/* 	{DEBUGln(F("..OK"));} */
	/* else {DEBUGln(F("..NOK"));} */
}

//turns ON one zone only, all others off
void zoneON(byte which)
{
	//stopAndResetProgram(); //stop any running programs
	/* if (radio.ACKRequested()) radio.sendACK(); */
	registersWriteBit(which);
	/* sprintf(buff, "ZONE:%d", which); */
	/* if (radio.sendWithRetry(GATEWAYID, buff, strlen(buff))) */
	/* 	{DEBUGln(F("..OK"));} */
	/* else {DEBUGln(F("..NOK"));} */
}

void registersClear()
{
	digitalWrite(LATCHPIN, LOW);
	for(byte i=0;i<REGISTERCOUNT;i++)
		shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0x0);
	digitalWrite(LATCHPIN, HIGH); 
}

void registersAllOn()
{
	digitalWrite(LATCHPIN, LOW);
	for(byte i=0;i<REGISTERCOUNT;i++)
		shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0xFF);	
	digitalWrite(LATCHPIN, HIGH);
}

//writes a single bit to a daisy chain of up to 32 shift registers (max 16 IOShields) chained via LATCHPIN, CLOCKPIN, DATAPIN
void registersWriteBit(byte whichPin)
{
		byte bitPosition = whichPin % 8;
		int zeroFills = (REGISTERCOUNT - 1) - (whichPin/8);

		if (zeroFills<0) { //whichPin was "out of bounds"
			DEBUGln("requested bit out of bounds (ie learger than available register bits to set)");
			registersClear();
			return; 
		}

		digitalWrite(LATCHPIN, LOW);
		for (byte i=0;i<zeroFills;i++)
			shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0x0);

		byte byteToWrite = 0;
		bitSet(byteToWrite, bitPosition);
		shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, byteToWrite);

		for (byte i=0;i<REGISTERCOUNT-zeroFills-1;i++)
			shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, 0x0);
		digitalWrite(LATCHPIN, HIGH);
}

//writes a byte stream to the shift register daisy chain (up to 256 daisy chained shift registers)
void registerWriteBytes(const void* buffer, byte byteCount)
{
	digitalWrite(LATCHPIN, LOW);
	for (byte i = 0; i < byteCount; i++)
		shiftOut(DATAPIN, CLOCKPIN, MSBFIRST, ((byte*)buffer)[i]);	
	digitalWrite(LATCHPIN, HIGH);
}
 
void Blink(byte PIN, byte DELAY_MS)
{
	pinMode(PIN, OUTPUT);
	digitalWrite(PIN,HIGH);
	delay(DELAY_MS/2);
	digitalWrite(PIN,LOW);
	delay(DELAY_MS/2);	
}
