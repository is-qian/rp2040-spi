/**
 * @file ESP8266.h
 * @brief The definition of class ESP8266. 
 * @author Wu Pengfei<pengfei.wu@itead.cc> 
 * @date 2015.02
 * 
 * @par Copyright:
 * Copyright (c) 2015 ITEAD Intelligent Systems Co., Ltd. \n\n
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version. \n\n
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef __ESP8266_H__
#define __ESP8266_H__


#include "py/stream.h"
#include "py/runtime.h"
#include "py/misc.h"
#include "py/mphal.h"
#include "py/objstr.h"
#include "extmod/misc.h"
#include "lib/netutils/netutils.h"

//#include "utils.h"
#include "modmachine.h"
#include "mphalport.h"
#include "mpconfigboard.h"
#include "modnetwork.h"

#include "buffer.h"

////////////////////////// config /////////////////////////

#define ESP8266_MAX_ONCE_SEND 2048
#define ESP8266_BUF_SIZE 4096 
#define MICROPY_UART_NIC 1
//////////////////////////////////////////////////////////
#define WIFI_UART  3
#define STATION_MODE  1
#define SOFTAP_MODE  2
typedef struct _ipconfig_obj
{
	mp_obj_t ip;
	mp_obj_t gateway;
	mp_obj_t netmask;
	mp_obj_t ssid;
	mp_obj_t MAC;
}ipconfig_obj;
typedef struct _softap_config
{
	char ssid[50];
	int ssid_len;
	int ssid_hidden;
	int authmode;
	char password[64];
	int max_conn;
	int channel;
}softap_config;
typedef struct _station_config
{
	mp_obj_t ssid;
	mp_obj_t ssid_len;
	mp_obj_t ssid_hidden;
	mp_obj_t authmode;
	mp_obj_t password;
	mp_obj_t channel;
}station_config;
typedef struct _mqttconn_obj
{
	mp_obj_t LinkID;
	mp_obj_t state;
	mp_obj_t scheme;
	mp_obj_t host;
	mp_obj_t port;
	mp_obj_t path;
	mp_obj_t reconnect;
}mqttconn_obj;
typedef struct _esp8266_obj
{
	mp_obj_t uart_obj;
	Buffer_t buffer;
}esp8266_obj;
/*
 * Provide an easy-to-use way to manipulate ESP8266. 
 */

bool kick(esp8266_obj* nic);

/**
 * Restart ESP8266 by "AT+RST". 
 *
 * This method will take 3 seconds or more. 
 *
 * @retval true - success.
 * @retval false - failure.
 */
bool reset(esp8266_obj* nic);

/**
 * Get the version of AT Command Set. 
 * 
 * @return the char* of version. 
 */
 
char* getVersion(esp8266_obj* nic);

/**
 * Set operation mode to staion. 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
 
bool setOprToStation(esp8266_obj* nic, int mode);

/**
 * Join in AP. 
 *
 * @param ssid - SSID of AP to join in. 
 * @param pwd - Password of AP to join in. 
 * @retval true - success.
 * @retval false - failure.
 * @note This method will take a couple of seconds. 
 */
bool joinAP(esp8266_obj* nic, const char* ssid, const char* pwd);


/**
 * Enable DHCP for client mode. 
 *
 * @param mode - server mode (0=soft AP, 1=station, 2=both
 * @param enabled - true if dhcp should be enabled, otherwise false
 * 
 * @note This method will enable DHCP but only for client mode!
 */
bool enableClientDHCP(esp8266_obj* nic,char mode, bool enabled);

/**
 * Leave AP joined before. 
 *
 * @retval true - success.
 * @retval false - failure.
 */
bool leaveAP(esp8266_obj* nic);

/**
 * Get the current status of connection(UDP and TCP). 
 * 
 * @return the status. 
 */
char* getIPStatus(esp8266_obj* nic);

/**
 * Get the IP address of ESP8266. 
 *
 * @return the IP list. 
 */
char* getLocalIP(esp8266_obj* nic);

/**
 * Enable IP MUX(multiple connection mode). 
 *
 * In multiple connection mode, a couple of TCP and UDP communication can be builded. 
 * They can be distinguished by the identifier of TCP or UDP named mux_id. 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
bool enableMUX(esp8266_obj* nic);

/**
 * Disable IP MUX(single connection mode). 
 *
 * In single connection mode, only one TCP or UDP communication can be builded. 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
bool disableMUX(esp8266_obj* nic);


/**
 * Create TCP connection in single mode. 
 * 
 * @param addr - the IP or domain name of the target host. 
 * @param port - the port number of the target host. 
 * @retval true - success.
 * @retval false - failure.
 */
bool createTCP(esp8266_obj* nic,char* addr, uint32_t port);
/**
 * Release TCP connection in single mode. 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
bool releaseTCP(esp8266_obj* nic);

/**
 * Register UDP port number in single mode.
 * 
 * @param addr - the IP or domain name of the target host. 
 * @param port - the port number of the target host. 
 * @retval true - success.
 * @retval false - failure.
 */
bool registerUDP(esp8266_obj* nic,char* addr, uint32_t port);

/**
 * Unregister UDP port number in single mode. 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
bool unregisterUDP(esp8266_obj* nic);

/**
 * Create TCP connection in multiple mode. 
 * 
 * @param mux_id - the identifier of this TCP(available value: 0 - 4). 
 * @param addr - the IP or domain name of the target host. 
 * @param port - the port number of the target host. 
 * @retval true - success.
 * @retval false - failure.
 */
bool createTCP_mul(esp8266_obj* nic,char mux_id, char* addr, uint32_t port);

/**
 * Release TCP connection in multiple mode. 
 * 
 * @param mux_id - the identifier of this TCP(available value: 0 - 4). 
 * @retval true - success.
 * @retval false - failure.
 */
bool releaseTCP_mul(esp8266_obj* nic,char mux_id);

/**
 * Register UDP port number in multiple mode.
 * 
 * @param mux_id - the identifier of this TCP(available value: 0 - 4). 
 * @param addr - the IP or domain name of the target host. 
 * @param port - the port number of the target host. 
 * @retval true - success.
 * @retval false - failure.
 */
bool registerUDP_mul(esp8266_obj* nic,char mux_id, char* addr, uint32_t port);

/**
 * Unregister UDP port number in multiple mode. 
 * 
 * @param mux_id - the identifier of this TCP(available value: 0 - 4). 
 * @retval true - success.
 * @retval false - failure.
 */
bool unregisterUDP_mul(esp8266_obj* nic,char mux_id);

/**
 * Set the timeout of TCP Server. 
 * 
 * @param timeout - the duration for timeout by second(0 ~ 28800, default:180). 
 * @retval true - success.
 * @retval false - failure.
 */
bool setTCPServerTimeout(esp8266_obj* nic,uint32_t timeout);

/**
 * Start TCP Server(Only in multiple mode). 
 * 
 * After started, user should call method: getIPStatus to know the status of TCP connections. 
 * The methods of receiving data can be called for user's any purpose. After communication, 
 * release the TCP connection is needed by calling method: releaseTCP with mux_id. 
 *
 * @param port - the port number to listen(default: 333).
 * @retval true - success.
 * @retval false - failure.
 *
 * @see char* getIPStatus(void);
 * @see uint32_t recv(char* coming_mux_id, char* buffer, uint32_t len, uint32_t timeout);
 * @see bool releaseTCP(char mux_id);
 */
bool startTCPServer(esp8266_obj* nic,uint32_t port);

/**
 * Stop TCP Server(Only in multiple mode). 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
bool stopTCPServer(esp8266_obj* nic);

/**
 * Start Server(Only in multiple mode). 
 * 
 * @param port - the port number to listen(default: 333).
 * @retval true - success.
 * @retval false - failure.
 *
 * @see char* getIPStatus(void);
 * @see uint32_t recv(char* coming_mux_id, char* buffer, uint32_t len, uint32_t timeout);
 */
bool startServer(esp8266_obj* nic,uint32_t port);

/**
 * Stop Server(Only in multiple mode). 
 * 
 * @retval true - success.
 * @retval false - failure.
 */
bool stopServer(esp8266_obj* nic);

bool get_host_byname(esp8266_obj* nic, const char* host,uint32_t len,char* out_ip, uint32_t timeout_ms);

/**
 * Send data based on TCP or UDP builded already in single mode. 
 * 
 * @param buffer - the buffer of data to send. 
 * @param len - the length of data to send. 
 * @retval true - success.
 * @retval false - failure.
 */
bool esp_send(esp8266_obj* nic,const char* buffer, uint32_t len, uint32_t timeout);
        
/**
 * Send data based on one of TCP or UDP builded already in multiple mode. 
 * 
 * @param mux_id - the identifier of this TCP(available value: 0 - 4). 
 * @param buffer - the buffer of data to send. 
 * @param len - the length of data to send. 
 * @retval true - success.
 * @retval false - failure.
 */
bool esp_send_mul(esp8266_obj* nic,char mux_id, const char* buffer, uint32_t len);

/**
 * Receive data from TCP or UDP builded already in single mode. 
 *
 * @param buffer - the buffer for storing data. 
 * @param buffer_size - the length of the buffer. 
 * @param timeout - the time waiting data. 
 * @return the length of data received actually. 
 */
int esp_recv(esp8266_obj* nic,char* buffer, uint32_t buffer_size, uint32_t* read_len, uint32_t timeout, bool* peer_closed, bool first_time_recv);

/**
 * Receive data from one of TCP or UDP builded already in multiple mode. 
 *
 * @param mux_id - the identifier of this TCP(available value: 0 - 4). 
 * @param buffer - the buffer for storing data. 
 * @param buffer_size - the length of the buffer. 
 * @param timeout - the time waiting data. 
 * @return the length of data received actually. 
 */
uint32_t esp_recv_mul(esp8266_obj* nic,char mux_id, char* buffer, uint32_t buffer_size, uint32_t timeout);

/**
 * Receive data from all of TCP or UDP builded already in multiple mode. 
 *
 * After return, coming_mux_id store the id of TCP or UDP from which data coming. 
 * User should read the value of coming_mux_id and decide what next to do. 
 * 
 * @param coming_mux_id - the identifier of TCP or UDP. 
 * @param buffer - the buffer for storing data. 
 * @param buffer_size - the length of the buffer. 
 * @param timeout - the time waiting data. 
 * @return the length of data received actually. 
 */
uint32_t esp_recv_mul_id(esp8266_obj* nic,char* coming_mux_id, char* buffer, uint32_t buffer_size, uint32_t timeout);

/* 
 * Empty the buffer or UART RX.
 */
void rx_empty(esp8266_obj* nic);

/* 
 * Recvive data from uart and search first target. Return true if target found, false for timeout.
 */
bool recvFind(esp8266_obj* nic, const char* target, uint32_t timeout);

/* 
 * Recvive data from uart and search first target and cut out the subchar* between begin and end(excluding begin and end self). 
 * Return true if target found, false for timeout.
 */
bool recvFindAndFilter(esp8266_obj* nic,const char* target, const char* begin, const char* end, char** data, uint32_t timeout);

/*
 * Receive a package from uart. 
 *
 * @param buffer - the buffer storing data. 
 * @param buffer_size - guess what!
 * @param data_len - the length of data actually received(maybe more than buffer_size, the remained data will be abandoned).
 * @param timeout - the duration waitting data comming.
 * @param coming_mux_id - in single connection mode, should be NULL and not NULL in multiple. 
 */
uint32_t recvPkg(esp8266_obj*nic,char* buffer, uint32_t buffer_size, uint32_t *data_len, uint32_t timeout, char* coming_mux_id, bool* peer_closed, bool first_time_recv);


bool eAT(esp8266_obj* nic);
bool eATE(esp8266_obj* nic,bool enable);
bool eATRST(esp8266_obj* nic);
bool eATGMR(esp8266_obj* nic,char** version);
bool qATCWMODE(esp8266_obj* nic,char* mode);
bool sATCWMODE(esp8266_obj* nic,char mode);
bool sATCWJAP(esp8266_obj* nic, const char* ssid, const char* pwd);
bool sATCWDHCP(esp8266_obj* nic,char mode, bool enabled);
bool eATCWQAP(esp8266_obj* nic);

bool eATCIPSTATUS(esp8266_obj* nic,char** list);
bool sATCIPSTARTSingle(esp8266_obj* nic, const char* type, char* addr, uint32_t port);
bool sATCIPSTARTMultiple(esp8266_obj*nic,char mux_id, char* type, char* addr, uint32_t port);
bool sATCIPSENDSingle(esp8266_obj*nic,const char* buffer, uint32_t len, uint32_t timeout);
bool sATCIPSENDMultiple(esp8266_obj* nic,char mux_id, const char* buffer, uint32_t len);
bool sATCIPCLOSEMulitple(esp8266_obj* nic,char mux_id);
bool eATCIPCLOSESingle(esp8266_obj* nic);
bool eATCIFSR(esp8266_obj* nic,char** list);
bool sATCIPMUX(esp8266_obj* nic,char mode);
bool sATCIPSERVER(esp8266_obj* nic,char mode, uint32_t port);
bool sATCIPSTO(esp8266_obj* nic,uint32_t timeout);
bool sATCIPMODE(esp8266_obj* nic,char mode);
bool sATCIPDOMAIN(esp8266_obj* nic, const char* domain_name, uint32_t timeout);
bool qATCWJAP_CUR(esp8266_obj* nic);
bool sATCIPSTA_CUR(esp8266_obj* nic, const char* ip,char* gateway,char* netmask);
bool qATCIPSTA_CUR(esp8266_obj* nic);
bool qATCWSAP(esp8266_obj* nic);
bool eINIT(esp8266_obj* nic, int mode);
bool get_ipconfig(esp8266_obj* nic, ipconfig_obj* ipconfig);
bool eATCWLAP(esp8266_obj* nic);
bool eATCWSAP(esp8266_obj* nic, const char* ssid, const char* key, int chl, int ecn);
bool sATCWSAP(esp8266_obj* nic, const char* ssid, const char* key, int chl, int ecn, int max_conn, int ssid_hidden);
bool eATCWLAP_Start(esp8266_obj* nic);
bool eATCWLAP_Get(esp8266_obj* nic, bool* end);
bool sMQTTUSERCFG(esp8266_obj* nic,int LinkID, int scheme, const char* client_id, const char* username, const char* password, int cert_key_ID, int CA_ID, const char* path);
bool sMQTTUSERNAME(esp8266_obj* nic,int LinkID, int username);
bool sMQTTPASSWORD(esp8266_obj* nic,int LinkID, int password);
bool sMQTTCONNCFG(esp8266_obj* nic,int LinkID, int keepalive, int disable_clean_session, const char* lwt_topic, const char* wt_msg, int lwt_qos, int lwt_retain);
bool sMQTTCONN(esp8266_obj* nic,int LinkID, const char* host, int port, int reconnect);
bool qMQTTCONN(esp8266_obj* nic);
bool sMQTTPUB(esp8266_obj*nic, uint32_t LinkID,  const char* topic,  const char* data, uint32_t qos, uint32_t retain);
bool qMQTTSUB(esp8266_obj*nic, uint32_t LinkID,  const char* topic, uint32_t qos);
bool eMQTTSUB_Start(esp8266_obj* nic);
bool eMQTTSUB_Get(esp8266_obj* nic, bool* end);
bool sMQTTUNSUB(esp8266_obj*nic, uint32_t LinkID,  const char* topic);
bool sMQTTCLEAN(esp8266_obj*nic, uint32_t LinkID);
/*
 * +IPD,len:data
 * +IPD,id,len:data
 */
 
/*
* bool sATCWSAP(char* ssid, char* pwd, char chl, char ecn);
* bool eATCWLIF(char* &list);

*/

/**
 * Set operation mode to softap. 
 * 
 * @retval true - success.
 * @retval false - failure.
 * bool setOprToSoftAP(void);
 */

/**
 * Set operation mode to station + softap. 
 * 
 * @retval true - success.
 * @retval false - failure.
 * bool setOprToStationSoftAP(void);
 */


/**
 * Search available AP list and return it.
 * 
 * @return the list of available APs. 
 * @note This method will occupy a lot of memeory(hundreds of Bytes to a couple of KBytes). 
 * Do not call this method unless you must and ensure that your board has enough memery left.
 * char* getAPList(void);
 */

/**
 * Set SoftAP parameters. 
 * 
 * @param ssid - SSID of SoftAP. 
 * @param pwd - PASSWORD of SoftAP. 
 * @param chl - the channel (1 - 13, default: 7). 
 * @param ecn - the way of encrypstion (0 - OPEN, 1 - WEP, 
 *  2 - WPA_PSK, 3 - WPA2_PSK, 4 - WPA_WPA2_PSK, default: 4). 
 * @note This method should not be called when station mode.
 * bool setSoftAPParam(char* ssid, char* pwd, char chl = 7, char ecn = 4);
*/

/**
 * Get the IP list of devices connected to SoftAP. 
 * 
 * @return the list of IP.
 * @note This method should not be called when station mode. 
 * char* getJoinedDeviceIP(void);
 */
//bool eATCWLAP(char* &list);
#endif
