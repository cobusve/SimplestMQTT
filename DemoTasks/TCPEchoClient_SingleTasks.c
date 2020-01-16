/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/*
 * A set of tasks are created that send TCP echo requests to the standard echo
 * port (port 7) on the IP address set by the configECHO_SERVER_ADDR0 to
 * configECHO_SERVER_ADDR3 constants, then wait for and verify the reply
 * (another demo is avilable that demonstrates the reception being performed in
 * a task other than that from with the request was made).
 *
 * See the following web page for essential demo usage and configuration
 * details:
 * http://www.FreeRTOS.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#include "MQTT/mqtt.h"

#define echoLOOP_DELAY	( ( TickType_t ) 5000 / portTICK_PERIOD_MS )

/* The size of the buffers is a multiple of the MSS - the length of the data
sent is a pseudo random size between 20 and echoBUFFER_SIZES. */
#define echoBUFFER_SIZES			( ipconfigTCP_MSS * 3 )

/*-----------------------------------------------------------*/

/*
 * Uses a socket to send data to, then receive data from, the standard echo
 * port number 7.
 */
static void prvEchoClientTask( void *pvParameters );

/*
 * Creates a pseudo random sized buffer of data to send to the echo server.
 */
static BaseType_t prvCreateTxData( char *ucBuffer, uint32_t ulBufferLength );

/*-----------------------------------------------------------*/

/* Rx and Tx time outs are used to ensure the sockets do not wait too long for
missing data. */
static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 600 );
static const TickType_t xSendTimeOut = pdMS_TO_TICKS( 300 );

/* Counters for each created task - for inspection only. */
static uint32_t ulTxRxCycles  =  0 ,
				ulTxRxFailures = 0 ,
				ulConnections =  0 ;

Socket_t xSocket;
struct mqtt_context  mqtt1 =
{
	&xSocket,			// Void Pointer to network context
    {"ClientID"},			// MQTT Client ID
	65535U				// Requested Keepalive timeout
};

static uint8_t testdata[10] = { '1','2','3','4','5','6','7','8','9' };

/*-----------------------------------------------------------*/


void vStartTCPEchoClientTasks_SingleTasks( uint16_t usTaskStackSize, UBaseType_t uxTaskPriority )
{
	/* Create the echo client tasks. */
	xTaskCreate( 	prvEchoClientTask,	/* The function that implements the task. */
					"Echo0",			/* Just a text name for the task to aid debugging. */
					usTaskStackSize,	/* The stack size is defined in FreeRTOSIPConfig.h. */
					( void * ) 0,		/* The task parameter, not used in this case. */
					uxTaskPriority,		/* The priority assigned to the task is defined in FreeRTOSConfig.h. */
					NULL );				/* The task handle is not used. */
}
/*-----------------------------------------------------------*/

int prvTcpConnect(Socket_t *pxSocket)
{
	WinProperties_t xWinProps;
	struct freertos_sockaddr xEchoServerAddress;

	/* Fill in the buffer and window sizes that will be used by the socket. */
	xWinProps.lTxBufSize = 6 * ipconfigTCP_MSS;
	xWinProps.lTxWinSize = 3;
	xWinProps.lRxBufSize = 6 * ipconfigTCP_MSS;
	xWinProps.lRxWinSize = 3;

	// We connect to port 80 on httpbin.org
	xEchoServerAddress.sin_port = FreeRTOS_htons(1883);
	xEchoServerAddress.sin_addr = FreeRTOS_inet_addr_quick(
		5,
		196,
		95,
		208);

	/* Create a TCP socket. */
	*pxSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
	configASSERT(*pxSocket != FREERTOS_INVALID_SOCKET);

	/* Set a time out so a missing reply does not cause the task to block indefinitely. */
	FreeRTOS_setsockopt(*pxSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof(xReceiveTimeOut));
	FreeRTOS_setsockopt(*pxSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof(xSendTimeOut));

	/* Set the window and buffer sizes. */
	FreeRTOS_setsockopt(*pxSocket, 0, FREERTOS_SO_WIN_PROPERTIES, (void*)& xWinProps, sizeof(xWinProps));

	/* Connect to the echo server. */
	if (FreeRTOS_connect(*pxSocket, &xEchoServerAddress, sizeof(xEchoServerAddress)) == 0)
	{
		return 1;
	}
	else 
	{
		return 0;
	}
}

/*-----------------------------------------------------------*/

void  prvTcpDisconnect(Socket_t* pxSocket)
{
	TickType_t xTimeOnEntering;
	BaseType_t xReturned;
	uint8_t buffer[32];

	/* Finished using the connected socket, initiate a graceful close: FIN, FIN+ACK, ACK. */
	FreeRTOS_shutdown(*pxSocket, FREERTOS_SHUT_RDWR);

	/* Expect FreeRTOS_recv() to return an error once the shutdown is
	complete. */
	xTimeOnEntering = xTaskGetTickCount();
	do
	{
		xReturned = FreeRTOS_recv(*pxSocket,	/* The socket being received from. */
			buffer,		                        /* The buffer into which the received data will be written. */
			32,			                        /* The size of the buffer provided to receive the data. */
			0);

		if (xReturned < 0)
		{
			break;
		}

	} while ((xTaskGetTickCount() - xTimeOnEntering) < xReceiveTimeOut);

	/* Close this socket before looping back to create another. */
	FreeRTOS_closesocket(*pxSocket);
	FreeRTOS_debug_printf( ( "Disconnected ... \r\n" ) );
}

/*-----------------------------------------------------------*/

int  mqtt_processPacket(struct mqtt_context* tag, struct mqtt_header* header)
{
	int status = MQTT_SUCCESS;
	uint8_t buffer[128] = { 0 };
	if ( mqtt_read( tag, buffer, header->remainingLength ) != header->remainingLength )
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


static void prvEchoClientTask( void *pvParameters )
{
	(pvParameters);
	char testBuffer[echoBUFFER_SIZES];

	for( ;; )
	{
		ulConnections++;
		FreeRTOS_debug_printf(("\n\n\n\rConnecting...(%d) \r\n", ulConnections));

		if( prvTcpConnect( &xSocket ) == 1 )  /* Connect to the echo server. */
		{
			FreeRTOS_debug_printf( ( "Sending MQTT Connect... \r\n") );
			eMqttConnectResult_t result = mqtt_Connect( &mqtt1  );

			configASSERT(result == MQTT_CONNECT_ACCEPTED );
			FreeRTOS_debug_printf( ( "Connected\r\n", result ) );

			vTaskDelay( 100 );
			mqtt_PingReq( &mqtt1 );
			vTaskDelay( 100 );

			/* Receive incoming MQTT PINGRES packet */
			mqtt_pollInput( &mqtt1 );

			mqtt_subscribe( &mqtt1, "MyTopic" );
			vTaskDelay( 500 );

			/* Receive incoming MQTT SUBACK packet */
			mqtt_pollInput( &mqtt1 );

			result = mqtt_publish( &mqtt1, "MyTopic", testdata, 9 );
			vTaskDelay( 200 );

			/* Clear the buffer for the received publish */
			memset( ( void * )testBuffer, 0x00, echoBUFFER_SIZES );
			
			/* Receive incoming MQTT Publish packet */
			mqtt_pollInput( &mqtt1 );
	
			mqtt_Disconnect( &mqtt1 );
			vTaskDelay( 100 );
		}

		prvTcpDisconnect( &xSocket );
		vTaskDelay( 15000 );
	}
}
