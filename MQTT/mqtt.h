/*
* This file contains the MQTT interface specification
*
*/
#include <stdint.h>

#define MQTT_SUCCESS 1
#define MQTT_ERROR   0

/*
 * MQTT control packet type and flags. Always the first byte of an MQTT packet.
 * For details, see
 *  http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/csprd02/mqtt-v3.1.1-csprd02.html#_Toc385349757
 */
#define MQTT_PACKET_TYPE_CONNECT                               ( ( uint8_t ) 0x10U ) /**< @brief CONNECT (client-to-server). */
#define MQTT_PACKET_TYPE_CONNACK                               ( ( uint8_t ) 0x20U ) /**< @brief CONNACK (server-to-client). */
#define MQTT_PACKET_TYPE_PUBLISH                               ( ( uint8_t ) 0x30U ) /**< @brief PUBLISH (bi-directional). */
#define MQTT_PACKET_TYPE_PINGREQ                               ( ( uint8_t ) 0xc0U ) /**< @brief PINGREQ (client-to-server). */
#define MQTT_PACKET_TYPE_DISCONNECT                            ( ( uint8_t ) 0xe0U ) /**< @brief DISCONNECT (client-to-server). */
#define MQTT_PACKET_TYPE_PINGRESP                              ( ( uint8_t ) 0xd0U ) /**< @brief PINGRESP (server-to-client). */
#define MQTT_PACKET_TYPE_SUBSCRIBE                             ( ( uint8_t ) 0x82U ) /**< @brief SUBSCRIBE (client-to-server). */
#define MQTT_PACKET_TYPE_SUBACK                                ( ( uint8_t ) 0x90U ) /**< @brief SUBACK (server-to-client). */

#define MQTT_PACKET_TYPE_UNSUBSCRIBE                           ( ( uint8_t ) 0xa2U ) /**< @brief UNSUBSCRIBE (client-to-server). */
#define MQTT_PACKET_TYPE_UNSUBACK                              ( ( uint8_t ) 0xb0U ) /**< @brief UNSUBACK (server-to-client). */

// Contains MQTT settings, an opague to the network connection instance and some session state
struct mqtt_context {
	// Conneciton Configuration (input)
	void*    network_tag;               // Used by read and write functions to pass network connection reference
	uint8_t	 clientId[23];              // MQTT Client ID as per MQTT Spec
	uint16_t keepaliveTimeout;			// Keepalive Timeout period to request when connecting
	uint8_t  dontRequestCleanSession;	// Set this to non-zero if you do NOT want a clean session
	// Connection Status Data (output)
	uint8_t  sessionPresent;			// After successful connection this will be set to indicate "session present"
};

struct mqtt_header {
	uint8_t  type;
	int32_t remainingLength;
};

typedef enum {
	MQTT_CONNECT_ACCEPTED = 0,
	MQTT_CONNECT_REFUSED_PROTVERSION = 1,
	MQTT_CONNECT_REFUSED_CLIENTIDREJECTED = 2,
	MQTT_CONNECT_REFUSED_SERVERUNAVAILABLE = 3,
	MQTT_CONNECT_REFUSED_BADUSERNAMEPASSWORD = 4,
	MQTT_CONNECT_REFUSED_NOTAUTHORIZED = 5,
	MQTT_CONNECT_RESERVED = 255
}  eMqttConnectResult_t;


// These functions must be supplied by the application
int  mqtt_write( struct mqtt_context* tag, uint8_t* ptr, int32_t len );
int  mqtt_read( struct mqtt_context* tag, uint8_t* ptr, int32_t len );
int  mqtt_processPacket( struct mqtt_context* tag, struct mqtt_header* header );

// The application needs to call this to check for incoming packets
int  mqtt_pollInput( struct mqtt_context* tag );

// General MQTT interface functions
int mqtt_Connect( struct mqtt_context* tag );
int mqtt_Disconnect( struct mqtt_context* tag );
int mqtt_PingReq( struct mqtt_context* tag );
int mqtt_subscribe(struct mqtt_context* tag, char* topicFilter);
int mqtt_publish(
	struct mqtt_context* tag,
	char* topic,
	uint8_t* pData,
	int32_t len );



