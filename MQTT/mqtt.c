/*
* This file implements the core MQTT protocol
*
*/
#include <string.h>
#include "mqtt.h"

#define MQTT_VERSION_3_1_1    4U 

// Applies to QOS1/2 packets only
#define MQTT_PACKET_TYPE_PUBACK                                ( ( uint8_t ) 0x40U ) /**< @brief PUBACK (server-to-client). */

static uint8_t* encodeRemainingLength( uint8_t* pDestination, int32_t length );
struct mqtt_header parseHeader( struct mqtt_context* tag );


int mqtt_Connect( struct mqtt_context* tag )
{   
	uint16_t len;
	eMqttConnectResult_t  status;

	len = (uint16_t)strlen((char*)tag->clientId);

	uint8_t buffer[ 15 ] = { MQTT_PACKET_TYPE_CONNECT,				// Packet Type
							 0,										// Remaining Length placeholder
							 0, 4, 'M', 'Q', 'T' , 'T',				// MQTT Protocol name
							 MQTT_VERSION_3_1_1,					// Protocol level
							 0b00000000 };							// Connect Flags 


	buffer[ 1 ] = 12 + len;
	
    // Do we get a clean session? Set the cleanSession flag
	buffer[ 9 ] |= ( tag->dontRequestCleanSession > 0 ? 0 : 2 );

	// Keepalive Time
	buffer[ 10 ] = 0xFF & ( tag->keepaliveTimeout >> 8 );   // MSB 
	buffer[ 11 ] = 0xFF & tag->keepaliveTimeout;			// LSB 

	// ClientID Length
	buffer[ 12 ] = 0xFF & ( len >> 8 );
	buffer[ 13 ] = 0xFF & len;

	// Send fixed and variable length headers up to ClientID Length
	if ( 14 != mqtt_write( tag, buffer, 14 ) )
	{
		return MQTT_ERROR;
	}

	// Send the actual ClientID
	if ( len != mqtt_write( tag, tag->clientId, len ) )
	{
		return MQTT_ERROR;
	}

	struct mqtt_header header = parseHeader( tag );

	// Valid CONNACK is the only packet we may accept at this point
	if ( header.type == MQTT_PACKET_TYPE_CONNACK )
	{
		if ( mqtt_read( tag, buffer, 2 ) != 2 )
		{
			status = MQTT_ERROR;
		}
		else 
		{
			tag->sessionPresent = buffer[ 0 ];
			status = buffer[ 1 ];
		}
	}
	else {
		status = MQTT_ERROR;
	}
	return status;
}

int mqtt_Disconnect( struct mqtt_context* tag )
{
	uint8_t buffer[ 2 ] = { MQTT_PACKET_TYPE_DISCONNECT,  0 };						

	// Send packet
	if ( 2 == mqtt_write( tag, buffer, 2 ) )
	{
		return MQTT_SUCCESS;
	}
	else 
	{
		return MQTT_ERROR;
	}
}

int mqtt_PingReq( struct mqtt_context* tag )
{
	uint8_t buffer[ 2 ] = { MQTT_PACKET_TYPE_PINGREQ, 0 };							
						
	// Send packet
	if ( 2 == mqtt_write( tag, buffer, 2 ) )
	{
		return MQTT_SUCCESS;
	}
	else 
	{
		return MQTT_ERROR;
	}
}

int mqtt_publish( struct mqtt_context* tag, char* topic, uint8_t* pData, int32_t len )
{
	int32_t status = 0;
	uint8_t* pCursor;
	uint16_t topiclen = (uint16_t)strlen( topic );
	int32_t remainingLength = len + topiclen + 2;
	uint8_t buffer[ 7 ] = { MQTT_PACKET_TYPE_PUBLISH };
	
	pCursor = encodeRemainingLength( &buffer[1], remainingLength );
	*( pCursor++ ) = topiclen >> 8;
	*( pCursor++ ) = topiclen & 0xFF;

	// Write the fixed header
	status = mqtt_write( tag, buffer, pCursor - buffer );
	if ( status != pCursor - buffer )
	{
		return MQTT_ERROR;
	}

	status = mqtt_write( tag, ( uint8_t* )topic, topiclen );
	if ( status != topiclen )
	{
		return MQTT_ERROR;
	}

	// Write the payload
	status = mqtt_write( tag, pData, len );
	if ( status != len )
	{
		return MQTT_ERROR;
	}
	else 
	{
		return MQTT_SUCCESS;
	}
}


int mqtt_subscribe(struct mqtt_context* tag, char* topicFilter)
{
	int32_t status = 0;
	uint8_t* pCursor;
	uint16_t topicFilterlen = (uint16_t)strlen( topicFilter );
	int32_t remainingLength =  topicFilterlen + 5;
	uint8_t buffer[9] = { MQTT_PACKET_TYPE_SUBSCRIBE };

	pCursor = encodeRemainingLength( &buffer[ 1 ], remainingLength );

	static uint16_t packetId = 0;
	*(pCursor++) = 0xFF & ( ++packetId ) >> 8;
	*(pCursor++) = 0xFF & packetId;

	*(pCursor++) = 0xFF & ( topicFilterlen >> 8 );
	*(pCursor++) = 0xFF & topicFilterlen;

	// Write the fixed header
	status = mqtt_write( tag, buffer, pCursor - buffer );
	if ( status != pCursor - buffer )
	{
		return MQTT_ERROR;
	}

	// Write the Topic Filter
	status = mqtt_write( tag, (uint8_t*)topicFilter, topicFilterlen+1 );
	if ( status != topicFilterlen )
	{
		return MQTT_ERROR;
	}
	else
	{
		return MQTT_SUCCESS;
	}
}

int mqtt_pollInput( struct mqtt_context* tag )
{
	int status = MQTT_SUCCESS;
	uint8_t buffer[32];
	struct mqtt_header header = parseHeader(tag);

	// Here we decide which packets to pass up for processing and which to just swallow 
	if ( ( header.type == MQTT_PACKET_TYPE_UNSUBACK ) )
	{
		// Just swallow the packet
		if ( mqtt_read( tag, buffer, header.remainingLength ) != header.remainingLength )
		{
			status = MQTT_ERROR;
		}
	}
	else if ( (header.type == MQTT_PACKET_TYPE_PINGRESP) ||
		      (header.type == MQTT_PACKET_TYPE_SUBACK) ||
		      (header.type == MQTT_PACKET_TYPE_PUBLISH) )
	{
		status = mqtt_processPacket( tag, &header );
	}
	else 
	{
		status = MQTT_ERROR;
	}
	return status;
}

// Encode the length according to MQTT variable length spec and return the pointer after encoding
static uint8_t* encodeRemainingLength( uint8_t* pDestination, int32_t length )
{
	uint8_t lengthByte = 0, * pLengthEnd = pDestination;

	do
	{
		lengthByte = length % 128;
		length = length / 128;

		/* Set the high bit of this byte if there is more data. */
		*( pLengthEnd++ ) = ( length > 0 ) ? lengthByte | 0x80 : lengthByte;
	} while ( length > 0 );

	return pLengthEnd;
}

// This function will parse an MQTT fixed header to the end of RemainingLength
struct mqtt_header parseHeader( struct mqtt_context* tag )
{
	char status = MQTT_SUCCESS, multiplier = 1;
	struct mqtt_header retVal = { 0 };

	if ( 1 == mqtt_read( tag, &retVal.type, 1 ) )
	{
		uint8_t value;
		do {
			if ( 1 != mqtt_read( tag, &value, 1 ) ) 
			{ 
				status = MQTT_ERROR;
				break;
			}
			else 
			{
				retVal.remainingLength  += ( value & 0x7F ) * multiplier;
				multiplier *= 128;
			}
		} while ( ( value & 0x80 ) && ( multiplier < 2097152 ) );
	}
	else 
	{
		status = MQTT_ERROR;
	}

	if ( status == MQTT_ERROR ) {
		retVal.type = 0;
		retVal.remainingLength = 0;
	}

	return retVal;
}
