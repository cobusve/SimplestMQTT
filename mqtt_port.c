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


/*-----------------------------------------------------------*/

int  mqtt_processPacket(struct mqtt_context* tag, struct mqtt_header* header)
{
	int status = MQTT_SUCCESS;
	uint8_t buffer[128] = { 0 };
	if (mqtt_read(tag, buffer, header->remainingLength) != header->remainingLength)
	{
		status = MQTT_ERROR;
	}
	else
	{
		// Process our pubish packet. We could use a lookup table here to route by topic.
		if (header->type == MQTT_PACKET_TYPE_PUBLISH)
		{
			uint8_t topic;
			uint16_t topicLength;

			topicLength = (buffer[0] << 8) + buffer[1];

			FreeRTOS_debug_printf(("Incoming Publish : %s\r\n", &buffer[topicLength + 2]));
		}
	}

	return status;
}
