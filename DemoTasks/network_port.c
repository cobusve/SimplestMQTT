/*
* This file contains functions to connect and disconnect the network connection only
* 
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/*-----------------------------------------------------------*/

/* Rx and Tx time outs are used to ensure the sockets do not wait too long for
missing data. */
static const TickType_t xReceiveTimeOut = pdMS_TO_TICKS(600);
static const TickType_t xSendTimeOut = pdMS_TO_TICKS(300);

int prvTcpConnect(Socket_t* pxSocket)
{
	BaseType_t result;
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
	result = FreeRTOS_connect(*pxSocket, &xEchoServerAddress, sizeof(xEchoServerAddress));
	if (result == 0)
	{
		FreeRTOS_debug_printf(("Connected\r\n"));
		return 1;
	}
	else
	{
		FreeRTOS_debug_printf(("Connection error %d\r\n", result));
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
	FreeRTOS_debug_printf(("Disconnected.\r\n"));
}



