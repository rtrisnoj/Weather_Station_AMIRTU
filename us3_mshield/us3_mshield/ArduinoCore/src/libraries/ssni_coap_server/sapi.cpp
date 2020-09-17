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

#include "sapi.h"
#include "sapiprivate.h"
#include "sapi_error.h"
#include "errors.h"
#include "arduino_pins.h"

#include <SPIMemory.h>
#include <ArduinoUniqueID.h>
#define Serial SERIAL_PORT_USBVIRTUAL
#define BLOCKSIZE 256
#define debug;
SPIFlash flash;
int sendInterval = 0;
int sampleRate = 0;
int Digital10 = 0;
int Digital11 = 0;
int Relay1 = 0;
int Relay2 = 0;
int Analog4 = 0;
int Analog5 = 0;

// Used to tell CoAP Server to use the SAPI dispatcher and handler
uint8_t	is_sapi = 1;


// Sensor Registration Info
sensor_reg_info_t sensor_info[SAPI_MAX_DEVICES];

// Next empty slot in the sensor info table
static	uint8_t sensor_info_index = 0;

extern char		classifier[CLASSIFIER_MAX_LEN];


//////////////////////////////////////////////////////////////////////////
//
// SAPI Initialization.
//
//////////////////////////////////////////////////////////////////////////
void sapi_initialize(char *url_classifier)
{
	
	is_sapi = 1;
	sensor_info_index = 0;
	

	// Use classifier if provided.
	if (url_classifier == NULL)
	{
		strncpy(classifier, DEFAULT_CLASSIFIER, CLASSIFIER_MAX_LEN);
	}
	else
	{
		strncpy(classifier, url_classifier, CLASSIFIER_MAX_LEN);
	}

	// Initialize the RTC and set the local time zone
	rtc_time_init(LOCAL_TIME_ZONE);
	
	// Initialize CoAP server logging.
	log_init(SER_MON_PTR, SER_MON_BAUD_RATE, LOG_LEVEL);
	
	
	// Set mNIC wake-up pin to HIGH, so that we can toggle it 0 -> 1
	pinMode(MNIC_WAKEUP_PIN, OUTPUT);
	digitalWrite(MNIC_WAKEUP_PIN, HIGH);
	
	// Initialize the CoAP Server
	coap_s_init(UART_PTR, COAP_MSG_MAX_AGE_IN_SECS, HDLC_UART_TIMEOUT_IN_MS, HDLC_MAX_PAYLOAD_LEN, "", NULL);
	
	// Send the reboot event to the milli nic
	coap_put_ic_reboot_event();
	delay(50);

	sapi_log_banner();
}




//////////////////////////////////////////////////////////////////////////
//
// SPI FLASH Functions
//
//////////////////////////////////////////////////////////////////////////



bool eraseBlock (){
	if (! flash . eraseBlock64K (1))
	{
		Serial.println("Erase Failed");
		return false ;
	}
	else
	{
		Serial.println("Erase Succeed");
		return true ;
	}
}
String readSerialStr() {
	String str = "";
	
	//  Serial.println("Waiting...");
	char inChar = 0;
	inChar = Serial.read();
	while (inChar != '.') {
		if (inChar != 255)
		str += inChar;
		if (inChar == '$')
		return "$";
		if (inChar == '#')
		return "#";
		inChar = Serial.read();
	}
	str += inChar;
	// Serial.println("end run");
	str += '\0';
	return str;
}
String getString(int addr){

	String output = "";
	uint8_t data_buffer[BLOCKSIZE];
	flash.readByteArray(addr, &data_buffer[0], BLOCKSIZE);
	for (int i = 2; i < BLOCKSIZE; i++)
	{
		if (data_buffer[i] == 255 )
		return output;
		output += (char)data_buffer[i];
	}
	return "No Data";
	
}
String getID(){

	String ID1 = "";
	//  Serial.println(F("Initialising"));
	//  UniqueIDdump(Serial);
	for (size_t i = 0; i < UniqueIDsize; i++)
	{
		ID1 += String(UniqueID[i],HEX);
		if (i%4==3 && i < (UniqueIDsize - 1))
		{
			ID1 += ("-");
		}
	}
	ID1.toUpperCase();
	return ID1;
}

void GoHere(){
	// Serial.println("Entering Loop");
	int addr = 1;
	String str = readSerialStr();
	if (str == "$"){
	Serial.println(getString(addr));
	}
	else if (str == "#"){
	Serial.println(getID());
	}
	else
	{
		eraseBlock();
		if (flash.writeStr(addr, str)) {
			Serial.println("complete");
		}
		else {
			Serial.print("failed");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Idle loop run.
//
//////////////////////////////////////////////////////////////////////////
char input;
bool initBoot = true;
bool init1 = true;
bool init2 = false;
int l = 0;
void sapi_run()
{
	if(initBoot){ 
		if(init1){
		Serial.println("Enter any key to go to BootProgram before it counts to 10"); 
		flash.begin(); 
		init1 = false;
		}
		
		//wait for 10 seconds
		if (!init2){
			for (l = 1; l < 11; l++){
				if(Serial.available()){
					input = Serial.read();
					Serial.println(input);
					init2 = true;
					break;
				
				}
				Serial.print(l);
				delay(1000);
				if(l == 10){
					initBoot = false;
					break;
					
				}
			}
		} 
		else{
		GoHere();
		}
		//initBoot = false;
		
	} 
	else { 
		//Coap Code
	coap_s_run();
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Load all the parameters from SPI FLASH
//
//////////////////////////////////////////////////////////////////////////
int ParamSendInterval(){
	return sampleRate*sendInterval;
}

int ParamSampleRate(){
	return sampleRate;
}

bool setValue(String parameter, String value)
{
	if (parameter == "SendInterval")
	{
		Serial.println("Send Interval: " + value);
		//� �� �sendInterval = (int)value;
		sendInterval = value.toInt();
	}
	else if (parameter == "SampleRate")
	{
		Serial.println("Sample Rate: " + value);
		//� �� �� �sampleRate = (int)value;
		sampleRate = value.toInt();
	}
	else if (parameter == "Digital10")
	{
		Digital10 = value.toInt();
		if (Digital10 == 0){
			//pinMode(D10,INPUT);
		}
		else if(Digital10 == 1){
			//pinMode(D10,OUTPUT);
			//digitalWrite(D10, HIGH);
		}
		Serial.println("Digital10: " + value);
	}
	else if (parameter == "Digital11")
	{
		Digital11 = value.toInt();
		if (Digital11 == 0){
			//pinMode(D11,INPUT);
		}
		else if(Digital11 == 1){
			//pinMode(D11,OUTPUT);
			//digitalWrite(D11, HIGH);
		}
		Serial.println("Digital11: " + value);
	}
	
	else if (parameter == "Relay1")
	{
		Relay1 = value.toInt();
		
		if (Relay1 == 0){
			pinMode(D6,OUTPUT);
			pinMode(D7,OUTPUT);
			digitalWrite(D6, LOW);
			digitalWrite(D7, HIGH);
		}
		else if (Relay1 == 1){
			pinMode(D6,OUTPUT);
			pinMode(D7,OUTPUT);
			digitalWrite(D6, HIGH);
			digitalWrite(D7, LOW);
		}
		Serial.println("Relay1: " + value);
	}
	else if (parameter == "Relay2")
	{
		Relay2 = value.toInt();
		
		if (Relay2 == 0){
			pinMode(D8,OUTPUT);
			pinMode(D9,OUTPUT);
			digitalWrite(D8, LOW);
			digitalWrite(D9, HIGH);
		}
		else if (Relay2 == 1){
			pinMode(D8,OUTPUT);
			pinMode(D9,OUTPUT);
			digitalWrite(D8, HIGH);
			digitalWrite(D9, LOW);
		}
		Serial.println("Relay2: " + value);
	}
		
	else if (parameter == "Analog4")
	{
		
		Analog4 = value.toInt();
		
		if (Analog4 == 0){
			//pinMode(A4,INPUT);
			
		}
		else if(Analog4 == 1){
			//pinMode(A4,OUTPUT);
		}
		Serial.println("Analog4: " + value);
	}
	
	else if (parameter == "Analog5")
	{
		
		Analog5 = value.toInt();
		
		if (Analog5 == 0){
			//pinMode(A5,INPUT);
		}
		else if(Analog5 == 1){
			//pinMode(A5,OUTPUT);
		}
		Serial.println("Analog5: " + value);
	}
	
}

void loadGlobalVariables() {
	flash.begin(); 
	
	String parameter = "";
	String value = "";
	String strVars = getString(1);
	
	String inChar = "";
	int i = 0;
	bool readingParam = true;
	//� �Serial.println(strVars);
	while (inChar != ".")
	{
		inChar = strVars[i++];
		if (inChar != ":" & inChar != ",")
		{
			if (readingParam)
			parameter += inChar;
			else
			value += inChar;
		}
		else
		{
			if (inChar == ":")
			{
				readingParam = false;
			}
			else
			{
				//Serial.println(parameter + ":" + value);
				setValue(parameter,value);
				parameter = "";
				value = "";
				readingParam = true;
			}
			
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//
// Register a sensor.
//
//////////////////////////////////////////////////////////////////////////
uint8_t sapi_register_sensor(char *sensor_type, SensorInitFuncPtr sensor_init, SensorReadFuncPtr sensor_read, SensorReadCfgFuncPtr sensor_readcfg,
							 SensorWriteCfgFuncPtr sensor_writecfg, uint8_t is_observer, uint32_t frequency)
{
	uint8_t sensor_id = sensor_info_index;
	
	strcpy(sensor_info[sensor_id].devicetype, sensor_type);
	sensor_info[sensor_id].init = sensor_init;
	sensor_info[sensor_id].read = sensor_read;
	sensor_info[sensor_id].readcfg = sensor_readcfg;
	sensor_info[sensor_id].writecfg = sensor_writecfg;
	sensor_info[sensor_id].frequency = frequency;
	
	sensor_info[sensor_id].observer = 0;
	if (is_observer == 1)
	{
		sensor_info[sensor_id].observer = 1;
		
		// Set the URI used for obtaining token etc in CoAP Observe response msg and set the observe handler, frequency, sensor id.
		sensor_info[sensor_id].observer_id = set_observer_sapi(sensor_type, sapi_observation_handler, frequency, sensor_id);
		dlog(LOG_DEBUG, "Set Observer Id: %d", sensor_info[sensor_id].observer_id);
	}
	sensor_info_index++;
	dlog(LOG_DEBUG, "Registered sensor: %s", sensor_type);
	return sensor_id;
}


//////////////////////////////////////////////////////////////////////////
//
// Initialize a sensor (hardware) and sensor related code.
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t sapi_init_sensor(uint8_t sensor_id)
{
	// Initialize Sensor
	SensorInitFuncPtr pInitFunc = sensor_info[sensor_id].init;
	sapi_error_t rcode = (*pInitFunc)();
	
	return rcode;
}


//////////////////////////////////////////////////////////////////////////
//
// Function used to push a notification.
//
//////////////////////////////////////////////////////////////////////////
sapi_error_t sapi_push_notification(uint8_t sensor_id)
{
	// Simple here. Just make the call. Heavy lifting is done in the CoAP Server
	// Does the milli hardware handshake if needed.
	error_t rc = coap_observe_rsp(sensor_info[sensor_id].observer_id);

	if (rc == ERR_NO_ENTRY)
		return SAPI_ERR_NO_ENTRY;
	if (rc == ERR_NO_MEM)
		return SAPI_ERR_NO_MEM;
	
	return SAPI_ERR_OK;
}


//////////////////////////////////////////////////////////////////////////
//
// Sensor CoAP Server Dispatcher.
//
//////////////////////////////////////////////////////////////////////////
error_t crsapi( struct coap_msg_ctx *req, struct coap_msg_ctx *rsp )
{
	struct optlv *o;
	void *it = NULL;
	error_t rc;

	// Isolate the leaf of the URI. Do this by skipping the first part of the URI.
	copt_get_next_opt_type((const sl_co*) & (req->oh), COAP_OPTION_URI_PATH, &it);
	if ((o = copt_get_next_opt_type((const sl_co*) & (req->oh), COAP_OPTION_URI_PATH, &it)))
	{
		// Look through the sensor registration table looking for a match. Match on device type.
		// When found dispatch its resource handler.
		for (uint8_t indx = 0 ; indx < sensor_info_index ; indx++)
		{
			if (!coap_opt_strcmp(o, sensor_info[indx].devicetype))
			{
				rc = crresourcehandler(req, rsp, it, indx);
				return rc;
			}
		}

		// No URI path is supported, so reject with not found
		rsp->code = COAP_RSP_404_NOT_FOUND;
	}

	rsp->plen = 0;
	return ERR_OK;
}


//////////////////////////////////////////////////////////////////////////
//
// SAPI CoAP Server resource handler.
//
//////////////////////////////////////////////////////////////////////////
error_t crresourcehandler(struct coap_msg_ctx *req, struct coap_msg_ctx *rsp, void *it, uint8_t sensor_id)
{
    struct optlv *o;
	uint8_t obs = false;


    /* No URI path beyond /temp is supported, so reject if present. */
    o = copt_get_next_opt_type((const sl_co*)&(req->oh), COAP_OPTION_URI_PATH, &it);
    if (o)
    {
        rsp->code = COAP_RSP_404_NOT_FOUND;
        goto err;
    }

    /* All methods require a query, so return an error if missing. */
    if (!(o = copt_get_next_opt_type((const sl_co*)&(req->oh), COAP_OPTION_URI_QUERY, NULL))) 
    {
        rsp->code = COAP_RSP_405_METHOD_NOT_ALLOWED;
        goto err;
    }
    
    /*
     * GET for reading sensor or config information
     */
    if (req->code == COAP_REQUEST_GET)
    {
        uint8_t rc;
        uint8_t len = 0;

        // Get Config values - cfg query
        if (!coap_opt_strcmp(o, "cfg"))
        {
            char payload[SAPI_MAX_PAYLOAD_LEN];
            uint8_t payloadlen = 0;
            SensorReadCfgFuncPtr pReadCfgFunc = sensor_info[sensor_id].readcfg;
            sapi_error_t rcode = (*pReadCfgFunc)(payload, &payloadlen);
            
            // Assemble the CoAP response message
            rc = build_rsp_msg(rsp->msg, &len, payload, payloadlen, sensor_id);
        }
		// Get Sensor values - sens query
        else if (!coap_opt_strcmp(o, "sens"))
        {
			// Handle observation options
			if ((o = copt_get_next_opt_type((sl_co*)&(req->oh), COAP_OPTION_OBSERVE, NULL))) 
			{
				uint32_t obsval = co_uint32_n2h(o);
				switch(obsval)
				{
					case COAP_OBS_REG:
						rc = coap_obs_reg_sapi(sensor_info[sensor_id].observer_id);
						//dlog(LOG_DEBUG, "Reg sensor observe for Id: %d", sensor_info[sensor_id].observer_id);
						obs = true;
						break;
					
					case COAP_OBS_DEREG:
						rc = coap_obs_dereg_sapi(sensor_info[sensor_id].observer_id);
						//dlog(LOG_DEBUG, "Dereg sensor observe for Id: %d", sensor_info[sensor_id].observer_id);
						break;
					
					default:
						rc = ERR_INVAL;
				}
			}
			// Get sensor value. Package as CoAP response.
			else
			{
				char payload[SAPI_MAX_PAYLOAD_LEN];
				uint8_t payloadlen = 0;
				SensorReadFuncPtr pReadSensor = sensor_info[sensor_id].read;
				sapi_error_t rcode = (*pReadSensor)(payload, &payloadlen);
				
				// Assemble the CoAP response message
				rc = build_rsp_msg(rsp->msg, &len, payload, payloadlen, sensor_id);
			}
        }
        // Don't support other queries
        else
		{
            rsp->code = COAP_RSP_501_NOT_IMPLEMENTED;
            goto err;
        }
		
        dlog(LOG_DEBUG, "crresourcehandler: GET status: %d len: %d bytes", rc, len);
        if (!rc)
		{
			if (obs)
			{                        
			/* Good code, but no content. */
				rsp->code = COAP_RSP_203_VALID;
				rsp->plen = 0;
			}
			else
			{
				rsp->plen = len;
				rsp->cf = COAP_CF_CSV;
				rsp->code = COAP_RSP_205_CONTENT;
			}
        }
		else
		{
            switch (rc)
			{
            case ERR_BAD_DATA:
            case ERR_INVAL:
				rsp->code = COAP_RSP_406_NOT_ACCEPTABLE;
				break;
            default:
				rsp->code = COAP_RSP_500_INTERNAL_ERROR;
				break;
            }
            goto err;
        }
    }
    /*
     * PUT for writing configuration or enabling sensors
     */
    else if (req->code == COAP_REQUEST_PUT) 
    {
	    char payload[SAPI_MAX_PAYLOAD_LEN];
        uint8_t len = 0;
        error_t rc = ERR_OK;
		
		SensorWriteCfgFuncPtr pSetCfgSensor = sensor_info[sensor_id].writecfg;
		len = o->ol;
		strncpy(payload, (char*)o->ov, len);
		payload[len] = '\0';
		
		sapi_error_t rcode = (*pSetCfgSensor)(payload, &len);
		if (rcode == SAPI_ERR_OK)
		{
			rsp->code = COAP_RSP_204_CHANGED;
			rsp->plen = 0;
		}
		else if (rcode == SAPI_ERR_NOT_IMPLEMENTED)
		{
			rsp->code = COAP_RSP_501_NOT_IMPLEMENTED;
			goto err;
		}
		else if (rcode == SAPI_ERR_BAD_DATA)
		{
			rsp->code = COAP_RSP_406_NOT_ACCEPTABLE;
			goto err;
		}
		else
		{
			rsp->code = COAP_RSP_500_INTERNAL_ERROR;
			goto err;
		}
    }
    else 
    {
        /* no other operation is supported */
        rsp->code = COAP_RSP_405_METHOD_NOT_ALLOWED;
        goto err;
    }

done:
    return ERR_OK;

err:
    rsp->plen = 0;
    return ERR_OK;
}


//////////////////////////////////////////////////////////////////////////
//
// Callback to handle generation of an observation notification.
// Called by coap_observe_rsp (the CoAP Server observation handler).
//
//////////////////////////////////////////////////////////////////////////
error_t sapi_observation_handler(struct mbuf *m, uint8_t *len, uint8_t sensor_id)
{
	char payload[SAPI_MAX_PAYLOAD_LEN];
	uint8_t payloadlen = 0;
	
	dlog(LOG_DEBUG, "SAPI observe for sensor: %s", sensor_info[sensor_id].devicetype);
	SensorReadFuncPtr pReadFunc = sensor_info[sensor_id].read;
	sapi_error_t rcode = (*pReadFunc)(payload, &payloadlen);
	
	// Assemble the CoAP response message
	error_t rc = build_rsp_msg(m, len, payload, payloadlen, sensor_id);
	return rc;
}


// This key denotes NIC type, which is used to determine how to store in MQTT broker.
#define NAMESPACE_NIC_TYPE_KEY          0
// Denotes device-specific data to follow in the CBOR payload
#define NAMESPACE_DEVICE_SPECIFIC_KEY   1

//////////////////////////////////////////////////////////////////////////
//
// Add sensor type to a CBOR payload wrapper
//
//////////////////////////////////////////////////////////////////////////
uint8_t cbor_enc_nic_type(struct cbor_buf *cbuf, char *sensor_type)
{
	uint8_t rcode;

	// Top level map, first element is for device type.
	// Device type used to provide a namespace for use at the MQTT broker.
	if ((rcode = cbor_enc_map(cbuf, 2)))
	{
		return rcode;
	}

	// NIC type, key and value
	if ((rcode = cbor_enc_int(cbuf, NAMESPACE_NIC_TYPE_KEY)))
	{
		return rcode;
	}
	if ((rcode = cbor_enc_text(cbuf, sensor_type, strlen(sensor_type))))
	{
		return rcode;
	}

	// Remaining device-specific key
	rcode = cbor_enc_int(cbuf, NAMESPACE_DEVICE_SPECIFIC_KEY);
	return rcode;
}


//////////////////////////////////////////////////////////////////////////
//
// Build the CoAP response message for a sensor. Called on these CoAP requests:
//   Get sensor value
//   Observe sensor value
//   Get sensor config
//
// Payload agnostic. Wrap the payload with a CBOR Map for MQTT routing.
// The CBOR payload is a CBOR wrapper containing the device type followed by the payload.
//
// A typical CBOR payload: {0:"temp",1:<text payload>"}
//
//////////////////////////////////////////////////////////////////////////
error_t build_rsp_msg(struct mbuf *m, uint8_t *len, char *payload, uint32_t payloadlen, uint8_t sensor_id)
{
	char		cbor_payload[SAPI_MAX_PAYLOAD_LEN];
	char  		*p;
	uint8_t 	l;
	
	// If payload needs to be in CBOR format we add the CBOR wrapper.
	error_t	rcode = ERR_FAIL;
	struct cbor_buf cbuf;
		
	cbor_enc_init(&cbuf, cbor_payload, SAPI_MAX_PAYLOAD_LEN);
	
	if (!cbor_enc_nic_type(&cbuf, sensor_info[sensor_id].devicetype))
	{
		if (!cbor_enc_text(&cbuf, payload, payloadlen))
		{
			rcode = ERR_OK;
		}
	}
	if (rcode == ERR_FAIL)
	{
		return rcode;
	}
		
	// Get CBOR payload length and allocate memory for the CBOR payload
	l = cbor_buf_get_len(&cbuf);
	p = (char *) m_append(m, l);
	if (!p)
	{
		return ERR_NO_MEM;
	}

	// Copy CBOR payload to response
	memcpy(p, cbor_payload, l);
	*len = l;
		
	dlog(LOG_DEBUG, "CBOR Payload Dump:");
	ddump(LOG_DEBUG, "Payload", cbor_payload, l);
	
	int freeram = free_ram();
	dlog(LOG_DEBUG, "Free Ram: %d", freeram);
	return ERR_OK;
}


//////////////////////////////////////////////////////////////////////////
//
// Original CoAP Server Dispatcher. This is provided in the Arduino sketch example with CoAP Server
//   Prior to version 1.4.6. We include it here for backward compatibility. Do not remove!
//
//////////////////////////////////////////////////////////////////////////
error_t crarduino( struct coap_msg_ctx *req, struct coap_msg_ctx *rsp )
{
	// Dummy. Should not be called!
	rsp->code = COAP_RSP_404_NOT_FOUND;
	rsp->plen = 0;
	return ERR_FAIL;
}


//////////////////////////////////////////////////////////////////////////
//
// Helper function to print a banner in the log.
//
//////////////////////////////////////////////////////////////////////////
void sapi_log_banner()
{
	// Log Banner: version number, time and date
	char ver[64];
	strcpy(ver, COAP_SERVER_VERSION_STRING);
	strcat(ver, COAP_SERVER_VERSION_NUMBER);
	println(ver);
	
	strcpy(ver, SAPI_VERSION_STRING);
	strcat(ver, SAPI_VERSION_NUMBER);
	println(ver);
	
	sprintf(ver, "Build Time: %s  Date: %s", __TIME__, __DATE__);
	println(ver);
	println("");
	
	// Log free memory
	int freeram = free_ram();
	dlog(LOG_DEBUG, "Free Ram: %d", freeram);
}
