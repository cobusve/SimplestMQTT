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

/* The size of the buffers is a multiple of the MSS - the length of the data
sent is a pseudo random size between 20 and echoBUFFER_SIZES. */
#define echoBUFFER_SIZES			( ipconfigTCP_MSS * 3 )

/*-----------------------------------------------------------*/

/* Rx and Tx time outs are used to ensure the sockets do not wait too long for
missing data. */
static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 600 );
static const TickType_t xSendTimeOut = pdMS_TO_TICKS( 300 );

/* Counters for each created task - for inspection only. */
static uint32_t ulConnections =  0;

Socket_t xSocket;
struct mqtt_context  mqtt1 =
{
	&xSocket,			// Void Pointer to network context
    {"ClientID"},			// MQTT Client ID
	65535U				// Requested Keepalive timeout
};

static uint8_t testdata[10] = { '1','2','3','4','5','6','7','8','9' };

/*-----------------------------------------------------------*/
int   prvTcpConnect(Socket_t* pxSocket);
void  prvTcpDisconnect(Socket_t* pxSocket);

static void prvEchoClientTask(void* pvParameters)
{
	(pvParameters);
	char testBuffer[ echoBUFFER_SIZES ];

	for ( ; ; )
	{
		ulConnections++;
		FreeRTOS_debug_printf( ( "\n\n\rConnecting...(%d) \r\n", ulConnections ) );

		if ( prvTcpConnect( &xSocket ) == 1)  /* Connect to the echo server. */
		{
			FreeRTOS_debug_printf(("Sending MQTT Connect... \r\n"));
			eMqttConnectResult_t result = mqtt_Connect( &mqtt1 );

			configASSERT(result == MQTT_CONNECT_ACCEPTED);

			mqtt_PingReq(&mqtt1);
			mqtt_pollInput(&mqtt1);                   /* Receive incoming MQTT PINGRES packet */

			mqtt_subscribe(&mqtt1, "MyTopic");
			mqtt_pollInput(&mqtt1);                   /* Receive incoming MQTT SUBACK packet */

			mqtt_subscribe(&mqtt1, "OtherTopic");
			mqtt_pollInput(&mqtt1);                   /* Receive incoming MQTT SUBACK packet */

			result = mqtt_publish(&mqtt1, "MyTopic", testdata, 9);
			result = mqtt_publish(&mqtt1, "OtherTopic", testdata, 9);

			vTaskDelay(500);

			mqtt_pollInput(&mqtt1);                  /* Receive incoming MQTT Publish packet */
			mqtt_pollInput(&mqtt1);                  /* Receive incoming MQTT Publish packet */

			FreeRTOS_debug_printf(("Request Disconnect\r\n"));
			mqtt_Disconnect(&mqtt1);
			vTaskDelay(100);
		}

		prvTcpDisconnect( &xSocket );
		vTaskDelay(7000);
	}
}


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
