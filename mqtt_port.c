/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include "MQTT/mqtt.h"

/*-----------------------------------------------------------*/
int  mqtt_write(struct mqtt_context* mqtt, uint8_t* ptr, int32_t len)
{
	/* Send the string to the socket. */
	return FreeRTOS_send(*(Socket_t*)mqtt->network_tag,		/* The socket being sent to. */
		(void*)ptr,												/* The data being sent. */
		len,													/* The length of the data being sent. */
		0);														/* No flags. */
}

int  mqtt_read(struct mqtt_context* mqtt, uint8_t* ptr, int32_t len)
{
	int32_t  xReceivedBytes = 0;
	int xReturned;

	while (xReceivedBytes < len)
	{
		xReturned = FreeRTOS_recv(*(Socket_t*)mqtt->network_tag,		/* The socket being received from. */
			ptr + xReceivedBytes,				/* The buffer into which the received data will be written. */
			len - xReceivedBytes,				/* The size of the buffer provided to receive the data. */
			0);									/* No flags. */
		configASSERT(xReturned >= 0);

		if (xReturned == 0)
		{
			/* Timed out. We must now exit*/
			break;
		}
		else
		{
			/* Keep a count of the bytes received so far. */
			xReceivedBytes += xReturned;
		}
	}
	return xReceivedBytes;
}


/*
 * What follows is an example of how a MQTT packet processor could be built as a middle layer.
 * This layer will receive MQTT messages (PUBLISH and others) from the network stream using 
 *   the read function and route the received messages to the appropriate processor.
 * While PUBLISH messages are routed based on their Topic according to a lookup table, other
 *   messages are routed by message type as contained in the header.
 *
*/

/* Prototype function pointer for callbacks */
typedef void (*processPacketFn_t)(uint8_t* data, int32_t len);

/* Two example functions to show how processing could happen in upstream modules. 
 * The function pointers could be stored statically as application level configuration or 
 *    be injected by the upstream modules at runtime by choice of the application.
 */
void topic1Function(uint8_t* data, int32_t len) 
{
	FreeRTOS_debug_printf(("Topic 1 data : %s\r\n", data));
}
void topic2Function(uint8_t* data, int32_t len) 
{
	FreeRTOS_debug_printf(("Topic 2 data : %s\r\n", data));
}

/* Create a routing table for topic data (Topics here are NOT Topic filters with wildcards! */
struct processingTable {
	char   topicName[32];  /* Topic name to route */
	processPacketFn_t fn;  /* Function for processing this topic */
} processingTable[2] = {
	{"MyTopic", topic1Function},
	{"OtherTopic", topic2Function}
};

/* Function to process and route packets. This function transforms the stream from TCP into
 *    a packet that lives in a statically allocated buffer
 */
int  mqtt_processPacket(struct mqtt_context* tag, struct mqtt_header* header)
{
	int status = MQTT_ERROR;
	static uint8_t buffer[128];

	if (header->remainingLength > 128)
	{
		return MQTT_ERROR;
	}

	// First always clear the buffer out
	memset(buffer, 0, sizeof(buffer));

	// Process our pubish packet. We could use a lookup table here to route by topic.
	if (header->type == MQTT_PACKET_TYPE_PUBLISH)
	{
		uint16_t topicLength;

		// Read the entire packet into the processing buffer
		if (mqtt_read(tag, buffer, header->remainingLength) == header->remainingLength)
		{
			topicLength = (buffer[0] << 8) + buffer[1];
			
			// If we get a valid topic length which does not excceed the packet length
			if (topicLength < header->remainingLength - 2)
			{
				// In case we do not find it
				// Route the packet to the right function
				for (int i = 0; i < sizeof(processingTable) / sizeof(processingTable[0]); i++)
				{
					if (strncmp(&buffer[2], processingTable[i].topicName, topicLength) == 0)
					{
						// We found a match, process it!
						status = MQTT_SUCCESS;
						processingTable[i].fn(&buffer[topicLength + 2], header->remainingLength - topicLength - 2);
					}
				}

				if (status == MQTT_ERROR)
				{
					// No match, just print it out
					FreeRTOS_debug_printf(("Unprocessed Publish : %s\r\n", &buffer[topicLength + 2]));
				}
			}
		}	
	}
	else if ((header->type == MQTT_PACKET_TYPE_SUBACK) ||
			 (header->type == MQTT_PACKET_TYPE_UNSUBACK))
	{
		// Just read the data and ignore. For SUBACK it has packet ID and QOS values, for UNSUBACK just ID 
		mqtt_read(tag, buffer, header->remainingLength);
		status = MQTT_SUCCESS;
	}
	else // Just dump all other packets for now
	{
		mqtt_read(tag, buffer, header->remainingLength);
		status = MQTT_SUCCESS;
	}
	
	return status;
}
