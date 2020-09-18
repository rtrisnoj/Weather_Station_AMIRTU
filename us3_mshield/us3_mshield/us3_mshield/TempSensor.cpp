/*

Copyright (c) Silver Spring Networks, Inc.
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the ""Software""), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED AS IS, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Silver Spring Networks, Inc.
shall not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from Silver Spring
Networks, Inc.

*/
#include "TempSensor.h"
#include <Filters.h>


// DHT11 Sensor Object
#define DHT_TYPE           DHT11
DHT_Unified dht(A1, DHT_TYPE);

// Sensor Context. Contains the unit of measure and alert state.
static temp_ctx_t context;



//////////////////////////////////////////////////////////////////////////
//
// Initialization functions. Callback functions. Payload building functions.
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Initialize DHT11 temp sensor. Callback called by sapi_init_sensor function.
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t temp_init_sensor()
{
	// Set context defaults and enable the sensor
	context.scalecfg = FAHRENHEIT_SCALE;
	context.alertstate = tsat_disabled;
	temp_sensor_enable();

	// Initialize temperature/humidity sensor
	dht.begin();

	// Log a banner for the sensor with sensor details
	println("DHT11 Sensor Initialized!");

	sensor_t sensor;
	dht.temperature().getSensor(&sensor);
	println("");
	println("------------------------------------");
	print  ("Sensor:       "); println(sensor.name);
	print  ("Driver Ver:   "); printnum(sensor.version);    println("");
	print  ("Unique ID:    "); printnum(sensor.sensor_id);  println("");
	print  ("Max Value:    "); printnum(sensor.max_value);  println(" C");
	print  ("Min Value:    "); printnum(sensor.min_value);  println(" C");
	print  ("Resolution:   "); printnum(sensor.resolution); println(" C");
	println("------------------------------------");        println("");

	return SAPI_ERR_OK;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a DHT11 sensor. Builds and returns the payload. Callback called on
//  CoAP Get sensor value
//  CoAP Observe notification of sensor value
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t temp_read_sensor(char *payload, uint8_t *len)
{
	float reading = 0.0;
    sapi_error_t rc;
	char buffer[128];

	// Read temp sensor, already in network order
    rc = read_dht11(&reading);
	if (rc != SAPI_ERR_OK)
	{
		return rc;
	}

	// Assemble the Payload
	rc = temp_build_payload(buffer, &reading);
	strcpy(payload, buffer);
	*len = strlen(buffer);
    return rc;
}


//////////////////////////////////////////////////////////////////////////
//
// Reads a DHT11 sensor. Read sensor configuration. Builds and returns the payload. Callback called on
//   CoAP Get configuration value
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t temp_read_cfg(char *payload, uint8_t *len)
{
	// Assemble the Payload
	// Trick - if sensor value is NULL than the payload builder just returns the UOM.
	sapi_error_t rc = temp_build_payload(payload, NULL);
	strcpy(payload, payload);
	*len = strlen(payload);

    return rc;
}


//////////////////////////////////////////////////////////////////////////
//
// Write sensor configuration. Processes payload sent from client. Callback called on
//  CoAP Put configuration value
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t temp_write_cfg(char *payload, uint8_t *len)
{
	if (!strcmp(payload, "cfg=C"))
	{
		context.scalecfg = CELSIUS_SCALE;
	}
	else if (!strcmp(payload, "cfg=F"))
	{
		context.scalecfg = FAHRENHEIT_SCALE;
	}
	// Config not supported
	else
	{
		return SAPI_ERR_NOT_IMPLEMENTED;
	}

	return SAPI_ERR_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// Measure Ultrasonic data (RS485) and Float
// Created 09/11/2020
// By Ryan Trisnojoyo
// PINS LAYOUT
// D2 = RX						RO
// D3 = TX						DI
// D4 = DIGITAL OUTPUT			RE
// D5 = DIGITAL OUTPUT			DE
// Ultrasonic sensor : 
// RS485_TX = B		--->		A RS485 Ultrasonic Sensor	
// RS485_RX = A		--->		B RS485 Ultrasonic Sensor
// Relay NO			--->		VIN Ultrasonic Sensor
// Relay COM		--->		V+ Battery 12V
// GND				--->		GND Ultrasnonic Sensor
// Float sensor:
// D10				--->		Signal Float pin
// GND				--->		GND pin
//////////////////////////////////////////////////////////////////////////
float resultTemp = 0;
float resultUltra = 0;

byte temp_data[12];
byte sendRS485Request[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27}; //Send Request for STATUS Command
byte sendAirTemperatureRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendHumidityRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendPressureRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendWindSpeedRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendWindDirectionRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendAvgWindSpeedRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendAvgWindDirectionRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendGustSpeedRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendGustDirectionRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};
byte sendRainfallRequest[8]={0x3C,0x03,0x00,0x01,0x00,0x01,0xD1,0x27};

size_t bytes;

float Send(byte * cmd, byte* ret) {
	// use default send function
	//turn on relay
	digitalWrite(D6, HIGH);
	digitalWrite(D7, LOW);
	delay(5000);
	sendCommand(cmd);
	
	 // receive answer
  if (Serial3.available()){  //Read return data package (NOTE: Demo is just for your reference, the data package haven't be calibrated yet)
	while(Serial3.read() != 0x3C);
	ret[0] = 0x3C; //Slave ID
	for(int j=2; j < 18; j++){
		ret[j++]=(Serial3.read());
	}
	
    Serial.println("Data Begin");
    Serial.println(ret[0],HEX); //byte 1    //Slave ID
    Serial.println(ret[2],HEX); //byte 2    //Function Code
    Serial.println(ret[4],HEX); //byte 3    //2 bytes
    Serial.println(ret[6],HEX); //byte 4    //Hex First Register
    Serial.println(ret[8],HEX); //byte 5    //Hex Second Register
    Serial.println(ret[10],HEX); //byte 6   //Register
	Serial.println(ret[12],HEX); //byte 7   //Register
	Serial.println(ret[14],HEX); //byte 8   //Checksum CRC
	Serial.println(ret[16],HEX); //byte 9   //Checksum CRC
	
    Serial.println("Data End");

    Serial3.flush();
    Serial.println("Received data Done");
	
	resultTemp = (float(ret[6])*256)+ float(ret[8]);
	Serial.print("Temperature: ");
	Serial.print(resultTemp);
	Serial.println(" C");
	
	/*
    resultTemp = (float(ret[8])*0.48876)-50;
    Serial.print("Temperature: ");
    Serial.print(resultTemp);
    Serial.println(" C");
	
    resultUltra = (float(ret[6]*256)+float(ret[4]))/128;
    Serial.print("Level: ");
    Serial.print(resultUltra);
    Serial.println(" In");
	*/
  }
  else{
    Serial.println("Error reading RS485");
  }
    delay(1000);
	//turn off relay
	digitalWrite(D6, LOW);
	digitalWrite(D7, HIGH);
	
	return resultUltra;
}

/*sendCommand(...)******************************************************************************
 * General function that sends command to RS485 peripheral device
 * <CR> is added to all commands
 * For RS485 communication, RTS pin has to be HIGH to allow writing to peripheral
 **********************************************************************************************/
void sendCommand(byte *cmd) {

//check if transmit the correct data
/*
 Serial.println("Send Command");
 for(int i=0; i < 6; i++){
    Serial.print(cmd[i]);
    Serial.println("");
 }
 */
	// set RTS to HIGH to allow writing to MAX485
 	digitalWrite(D4, HIGH);
 	digitalWrite(D5, HIGH); 
	delay(100);

	for(int i=0; i < 8; i++){
		Serial3.write(cmd[i]); 
  //Serial.print(cmd[i]);
	}	
 // send command
	Serial3.flush(); // Make sure message is fully transferred
	// set RTS to LOW to receive answer
	digitalWrite(D4, LOW);
	digitalWrite(D5, LOW);
 
	delay(50);
}
 
 /*
float ultra_temp = 12.00;
float calculate_Ultra(){
	 //turn on relay
	 digitalWrite(D6, HIGH);
	 digitalWrite(D7, LOW);
	 delay(5000);
	 
	 ultra_temp = analogRead(A5);
	 
	 //turn off relay
	 digitalWrite(D6, LOW);
	 digitalWrite(D7, HIGH);
	 return ultra_temp;
 }
 */
 
 //Float Code
 int temp_float = 0;
 int calculate_Float(){
	 pinMode(D10, INPUT_PULLUP);
	 temp_float = digitalRead(D10);
	 
	 if (temp_float == 1){
		 temp_float = 0;
	 }
	 else if (temp_float == 0){
		 temp_float = 1;
	 }
	 
	 return temp_float;
 }
//////////////////////////////////////////////////////////////////////////
//
// Code to build the sensor payload. Temp payload is text with this format:
//   <epoch>,<temp>,<uom>
//     <epoch> is a 4 byte (8 hex char) timestamp
//     <temp> is a decimal number
//     <uom> is a char: C or F
//
//  Note that the payload is text. Payloads can also be a byte array of binary data.
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t temp_build_payload(char *buf, float *reading)
{
	char 		payload[128]; // REMEMBER!! maximum payload 118 character payload character
	char		reading_buf[32];
	char		temp_payload[128];
	char        datatype_Ultra[] = "3,"; //datatype for LEVEL is 3
	char		datatype_Float[] = "7,"; //datatype for DI state is 7
	char		unitUltra[] = "In";
	char		unitFloat[] = "Al";
	char		unit_buf[4];
	float		ultra_temp = 10.00;
	time_t     	epoch;
	uint32_t	indx;
	char    rultra1[] = "12.00,";
	char    rfloat[] = "2,"; //if it shows 2, float not connected properly. The value should be 0 or 1.
	char    temp_epoch[20];
	

	Serial3.flush();
	
	ultra_temp = Send(sendRS485Request, temp_data);
	sprintf(rultra1, "%.2f,", ultra_temp);
	sprintf(rfloat, "%d,", calculate_Float());

	// Create string containing the UNIX epoch
	epoch = get_rtc_epoch();
	sprintf(temp_epoch, "%d,", epoch);
	
	
	//construct ultrasonic data payload
	sprintf(payload, "%d,", epoch);
	strcat(payload, datatype_Ultra);
	strcat(payload, rultra1);
	strcpy(unit_buf, unitUltra);
	strcat(payload, unit_buf);
	strcat(payload, ";");

	//construct float data payload
	strcat(payload,temp_epoch);
	strcat(payload, datatype_Float);
	strcat(payload, rfloat);
	strcpy(unit_buf, unitFloat);
	strcat(payload, unit_buf);

	strcpy(buf, payload);
	
	dlog(LOG_DEBUG, "Temp Payload: %s", payload);
	return SAPI_ERR_OK;
}



//////////////////////////////////////////////////////////////////////////
//
// Sensor (hardware) access functions.
//
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Read the temp value from the DHT11 sensor.
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t read_dht11(float *reading)
{
	sapi_error_t rc = SAPI_ERR_OK;
	float re = INVALID_TEMP;

	// Get temperature event
	sensors_event_t event;
	dht.temperature().getEvent(&event);

	// Check for NaN
	if (isnan(event.temperature))
	{
		re = NO_SENSOR_TEMP;
		rc = SAPI_ERR_OK;
	}
	else
	{
		re = event.temperature;
		rc = SAPI_ERR_OK;
	}

	// Reading is in C. Convert to F if needed.
	if (context.scalecfg == FAHRENHEIT_SCALE)
	{
		// Convert from Celsius to Fahrenheit
		re *= 1.8;
		re += 32;
	}

	// Assign output
	*reading = re;
	return rc;
}


//////////////////////////////////////////////////////////////////////////
//
// Context and Alert functions. Support for sensor enable and disable. Support for alerts.
//
//////////////////////////////////////////////////////////////////////////

sapi_error_t temp_sensor_enable(void)
{
	context.enable = 1;
	context.alertstate = tsat_cleared;
	return SAPI_ERR_OK;
}


sapi_error_t temp_sensor_disable(void)
{
	context.enable = 0;
	context.alertstate = tsat_disabled;
	return SAPI_ERR_OK;
}
