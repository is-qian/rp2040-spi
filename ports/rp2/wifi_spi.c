/**
 * @file esp8285.c
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
#include "extmod/machine_spi.h"
#include "modnetwork.h"
//#include "plic.h"cd
//#include "sysctl.h"
//#include "atomic.h"
#include "hardware/spi.h"
//#include "spihs.h"
#include "wifi_spi.h"
//#include "sleep.h"
typedef struct _machine_spi_obj_t {
    mp_obj_base_t base;
    spi_inst_t *const spi;
    uint8_t spi_id;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    uint8_t sck;
    uint8_t mosi;
    uint8_t miso;
    uint32_t baudrate;
} machine_spi_obj_t;

STATIC void kmp_get_next(const char* targe, int next[])
{  
    int targe_Len = strlen(targe);  
    next[0] = -1;  
    int k = -1;  
    int j = 0;  
    while (j < targe_Len - 1)  
    {     
        if (k == -1 || targe[j] == targe[k])  
        {  
            ++j;  
            ++k;   
            if (targe[j] != targe[k])  
                next[j] = k;    
            else   
                next[j] = next[k];  
        }  
        else  
        {  
            k = next[k];  
        }  
    }  
}  
STATIC int kmp_match(char* src,int src_len, const char* targe, int* next)  
{  
    int i = 0;  
    int j = 0;  
    int sLen = src_len;  
    int pLen = strlen(targe);  
    while (i < sLen && j < pLen)  
    {     
        if (j == -1 || src[i] == targe[j])  
        {  
            i++;  
            j++;  
        }  
        else  
        {       
            j = next[j];  
        }  
    }  
    if (j == pLen)  
        return i - j;  
    else  
        return -1;  
} 
STATIC uint32_t kmp_find(char* src,uint32_t src_len, const char* tagert)
{
	uint32_t index = 0;
	uint32_t tag_len = strlen(tagert);
	int* next = (int*)malloc(sizeof(uint32_t) * tag_len);
	kmp_get_next(tagert,next);
	index = kmp_match(src,src_len,tagert, next);
	free(next);
	return index;
}
STATIC uint32_t data_find(uint8_t* src,uint32_t src_len, const char* tagert)
{
	return kmp_find((char*)src,src_len,tagert);
}

bool kick(esp8285_obj* nic)
{
    return eAT(nic);
}

bool reset(esp8285_obj* nic)
{
    unsigned long start;
    if (eATRST(nic)) {
        sleep_ms(2000);
        start = mp_hal_ticks_ms();
        while (mp_hal_ticks_ms() - start < 3000) {
            if (eAT(nic)) {
                sleep_ms(1500); /* Waiting for stable */
                return true;
            }
            sleep_ms(100);
        }
    }
    return false;
}

char* getVersion(esp8285_obj* nic)
{
    char* version;
    eATGMR(nic,&version);
    return version;
}

bool setOprToStation(esp8285_obj* nic, int set_mode)
{
    char mode;
    if (!qATCWMODE(nic,&mode)) {
        return false;
    }

    if (sATCWMODE(nic,set_mode)) {
        return true;
    } else {
        return false;
    }
}


bool joinAP(esp8285_obj* nic, const char* ssid, const char* pwd)
{
    return sATCWJAP(nic,ssid, pwd);
}

bool enableClientDHCP(esp8285_obj* nic,char mode, bool enabled)
{
    return sATCWDHCP(nic,mode, enabled);
}

bool leaveAP(esp8285_obj* nic)
{
    return eATCWQAP(nic);
}

char* getIPStatus(esp8285_obj* nic)
{
    char* list;
    eATCIPSTATUS(nic, &list);
    return list;
}

char* getLocalIP(esp8285_obj* nic)
{
    char* list;
    eATCIFSR(nic, &list);
    return list;
}

bool enableMUX(esp8285_obj* nic)
{
    return sATCIPMUX(nic,1);
}

bool disableMUX(esp8285_obj* nic)
{
    return sATCIPMUX(nic,0);
}

bool createTCP(esp8285_obj* nic,char* addr, uint32_t port)
{
    return sATCIPSTARTSingle(nic,"TCP", addr, port);
}

bool releaseTCP(esp8285_obj* nic)
{
    return eATCIPCLOSESingle(nic);
}

bool registerUDP(esp8285_obj* nic,char* addr, uint32_t port)
{
    return sATCIPSTARTSingle(nic,"UDP", addr, port);
}

bool unregisterUDP(esp8285_obj* nic)
{
    return eATCIPCLOSESingle(nic);
}

bool createTCP_mul(esp8285_obj* nic,char mux_id, char* addr, uint32_t port)
{
    return sATCIPSTARTMultiple(nic,mux_id, "TCP", addr, port);
}

bool releaseTCP_mul(esp8285_obj* nic,char mux_id)
{
    return sATCIPCLOSEMulitple(nic,mux_id);
}

bool registerUDP_mul(esp8285_obj* nic,char mux_id, char* addr, uint32_t port)
{
    return sATCIPSTARTMultiple(nic,mux_id, "UDP", addr, port);
}

bool unregisterUDP_mul(esp8285_obj* nic,char mux_id)
{
    return sATCIPCLOSEMulitple(nic,mux_id);
}

bool setTCPServerTimeout(esp8285_obj* nic,uint32_t timeout)
{
    return sATCIPSTO(nic,timeout);
}

bool startTCPServer(esp8285_obj* nic,uint32_t port)
{
    if (sATCIPSERVER(nic,1, port)) {
        return true;
    }
    return false;
}

bool stopTCPServer(esp8285_obj* nic)
{
    sATCIPSERVER(nic,0,0);
    reset(nic);
    return false;
}

bool startServer(esp8285_obj* nic,uint32_t port)
{
    return startTCPServer(nic,port);
}

bool stopServer(esp8285_obj* nic)
{
    return stopTCPServer(nic);
}

bool get_host_byname(esp8285_obj* nic, const char* host,uint32_t len,char* out_ip, uint32_t timeout_ms)
{
	int index = 0;
	if(false == sATCIPDOMAIN(nic,host, timeout_ms))
	{
		mp_printf(&mp_plat_print, "[wio rp2040] %s | get_host_byname failed\n",__func__);
		return false;
	}
	char IP_buf[16]={0};
	index = data_find(nic->buffer.buffer,ESP8285_BUF_SIZE,"+CIPDOMAIN:");
	sscanf((char*)nic->buffer.buffer + index,"+CIPDOMAIN:\"%[^\"]\"",IP_buf);
	mp_obj_t IP = mp_obj_new_str(IP_buf, strlen(IP_buf));
	nlr_buf_t nlr;
	if (nlr_push(&nlr) == 0)
	{
		netutils_parse_ipv4_addr(IP, (uint8_t*)out_ip,NETUTILS_BIG);
		nlr_pop();
	}
	return true;
}

bool get_ipconfig(esp8285_obj* nic, ipconfig_obj* ipconfig)
{

	if(0 == qATCIPSTA_CUR(nic))
		return false;
	char* cur = NULL;
	cur = strstr((char*)nic->buffer.buffer, "ip");
	if(cur == NULL)
	{
		mp_printf(&mp_plat_print, "[wio rp2040] %s | esp8285_ipconfig could'n get ip\n",__func__);
		return false;
	}
	char ip_buf[16] = {0};
	sscanf(cur,"ip:\"%[^\"]\"",ip_buf);
	ipconfig->ip = mp_obj_new_str(ip_buf,strlen(ip_buf));
	cur = strstr((char*)nic->buffer.buffer, "gateway");
	if(cur == NULL)
	{
		mp_printf(&mp_plat_print, "[wio rp2040] %s | esp8285_ipconfig could'n get gateway\n",__func__);
		return false;
	}
	char gateway_buf[16] = {0};
	sscanf(cur,"gateway:\"%[^\"]\"",gateway_buf);
	ipconfig->gateway = mp_obj_new_str(gateway_buf,strlen(gateway_buf));
	cur = strstr((char*)nic->buffer.buffer, "netmask");
	if(cur == NULL)
	{
		mp_printf(&mp_plat_print, "[wio rp2040] %s | esp8285_ipconfig could'n get netmask\n",__func__);
		return false;
	}
	char netmask_buf[16] = {0};
	sscanf(cur,"netmask:\"%[^\"]\"",netmask_buf);
	ipconfig->netmask = mp_obj_new_str(netmask_buf,strlen(netmask_buf));
	//ssid & mac
	if(false == qATCWJAP_CUR(nic))
	{
		return false;
	}
	char ssid[50] = {0};
	char MAC[17] = {0};
	cur = strstr((char*)nic->buffer.buffer, "+CWJAP:");
	sscanf(cur, "+CWJAP:\"%[^\"]\",\"%[^\"]\"", ssid, MAC);
	ipconfig->ssid = mp_obj_new_str(ssid,strlen(ssid));
	ipconfig->MAC = mp_obj_new_str(MAC,strlen(MAC));
	return true;
}
bool get_mqttconn(esp8285_obj* nic, mqttconn_obj* mqttconn)
{

	if(0 == qMQTTCONN(nic))
		return false;
	char* cur = NULL;
	cur = strstr((char*)nic->buffer.buffer, "+MQTTCONN:");
	if(cur == NULL)
	{
		mp_printf(&mp_plat_print, " %s | MQTTCONN could'n get \n",__func__);
		return false;
	}
	int LinkID;
	int state;
	int scheme;
	char host[128] = {0};
	int port;
	char path[32] = {0};
	int reconnect;
	sscanf(cur, "+MQTTCONN:%d,%d,%d,\"%[^\"]\",%d,\"%[^\"]\",%d", &mqttconn->LinkID, &mqttconn->state, &mqttconn->scheme, mqttconn->host, &mqttconn->port, mqttconn->path, &mqttconn->reconnect);
	return true;
}
bool get_mqttsubrecv(esp8285_obj*nic, uint32_t LinkID, mqtt_msg* mqttmsg)
{
	char* cur = NULL;
	int errcode;
	uint32_t iter = 0;
	memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);
    unsigned long start = mp_hal_ticks_ms();
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	machine_spi_obj_t *self = MP_OBJ_TO_PTR(nic->spi_obj);
	while (mp_hal_ticks_ms() - start < 3000) {
        while(gpio_get(SPI_HANDSHARK) > 0) {
            iter += readCmd(nic->spi_obj,&nic->buffer.buffer);
        }
	}
	cur = strstr((char*)nic->buffer.buffer, "+MQTTSUBRECV");
	if(cur == NULL)
	{
		//mp_printf(MP_PYTHON_PRINTER, "test get_mqttsubrecv none\n");
		return false;
	}
	int LinkID_recv;
	int len;
	char topic[255] = {0};
	char msg[4096] = {0};	
	sscanf(cur, "+MQTTSUBRECV:%d,\"%[^\"]\",%d,%[^\n]", &LinkID_recv, topic, &len, msg);
	msg[len] = NULL;
	mqttmsg->topic = mp_obj_new_str(topic,strlen(topic));
	mqttmsg->msg = mp_obj_new_str(msg,len);
	return true;
}
bool wifi_softap_get_config(esp8285_obj* nic, softap_config* apconfig)
{
	if(0 == qATCWSAP(nic))
		return false;
	char* cur = NULL;
	cur = strstr((char*)nic->buffer.buffer, "+CWSAP:");
	if(cur == NULL)
	{
		mp_printf(&mp_plat_print, "[wio rp2040] %s | esp8285_config could'n get ip\n",__func__);
		return false;
	}
	sscanf(cur, "+CWSAP:\"%[^\"]\",\"%[^\"]\",%d,%d,%d,%d", apconfig->ssid, apconfig->password, &apconfig->channel, &apconfig->authmode,  &apconfig->max_conn, &apconfig->ssid_hidden);
	//nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "qian connect to 1.%s 2.%s 3.%d 4.%d 5.%d 6.%d", apconfig->ssid,apconfig->password,&apconfig->channel,&apconfig->authmode,&apconfig->max_conn,&apconfig->ssid_hidden));
	return true;
}

bool wifi_softap_set_config(esp8285_obj* nic, softap_config* apconfig)
{
	apconfig->max_conn = 8;
	return sATCWSAP(nic, apconfig->ssid, apconfig->password, apconfig->channel, apconfig->authmode, apconfig->max_conn, apconfig->ssid_hidden);
}

bool esp_send(esp8285_obj* nic,const char* buffer, uint32_t len, uint32_t timeout)
{
    bool ret = false;
    uint32_t send_total_len = 0;
    uint16_t send_len = 0;

    while(send_total_len < len)
    {
        send_len = ((len-send_total_len) > ESP8285_MAX_ONCE_SEND)?ESP8285_MAX_ONCE_SEND : (len-send_total_len);
        ret = sATCIPSENDSingle(nic,buffer+send_total_len, send_len, timeout);
        if(!ret)
            return false;
        send_total_len += send_len;
    }
    return true;
}

bool esp_send_mul(esp8285_obj* nic,char mux_id, const char* buffer, uint32_t len)
{
    return sATCIPSENDMultiple(nic,mux_id, buffer, len);
}

int esp_recv(esp8285_obj* nic,char* buffer, uint32_t buffer_size, uint32_t* read_len, uint32_t timeout, bool* peer_closed, bool first_time_recv)
{
    return recvPkg(nic,buffer, buffer_size, read_len, timeout, NULL, peer_closed, first_time_recv);
}

uint32_t esp_recv_mul(esp8285_obj* nic,char mux_id, char* buffer, uint32_t buffer_size, uint32_t timeout)
{
    char id;
    uint32_t ret;
    ret = recvPkg(nic,buffer, buffer_size, NULL, timeout, &id, NULL, false);
    if (ret > 0 && id == mux_id) {
        return ret;
    }
    return 0;
}
uint32_t readdata(esp8285_obj* nic, char* data)
{
	static uint32_t read_time = 0;
	spi_trans_len trans_len;
	spi_trans_data trans_data;
	memset(&trans_len, 0x0, sizeof(trans_len));
	memset(&trans_data, 0x0, sizeof(trans_data)); 
	uint8_t addr = 0x0;
	const mp_machine_spi_p_t * spi_stream = (const mp_machine_spi_p_t *)((const mp_obj_base_t *)MP_OBJ_TO_PTR(nic->spi_obj))->type->protocol;
	int data_len = sizeof(trans_len);
	if(read_time == 0){
		trans_len.cmd = SPI_MASTER_READ_STATUS_FROM_SLAVE_CMD;
		trans_len.len = 0;
		int data_len = sizeof(trans_len);
		mp_hal_pin_output(SPI_CS);
		gpio_put(SPI_CS,0);
		spi_stream->transfer(nic->spi_obj,sizeof(trans_len.cmd),(uint8_t *)&trans_len.cmd,(uint8_t *)&trans_len.cmd);
		spi_stream->transfer(nic->spi_obj,sizeof(trans_len.len),(uint8_t *)&trans_len.len,(uint8_t *)&trans_len.len);
		gpio_put(SPI_CS,1);
		read_time = (trans_len.len + 63) / 64;
	}
	if(read_time > 0) {
		data_len = sizeof(trans_data);
		trans_data.cmd = SPI_MASTER_READ_DATA_FROM_SLAVE_CMD;
		trans_data.addr = 0x00;	
		sleep_us(50);
		gpio_put(SPI_CS,0);
		spi_stream->transfer(nic->spi_obj,data_len,(uint8_t *)&trans_data,(uint8_t *)&trans_data);
		gpio_put(SPI_CS,1);
		memcpy(data, trans_data.data, sizeof(trans_data.data));
		sleep_us(150);
		read_time--;
	}
	return read_time;
}

uint32_t esp_recv_mul_id(esp8285_obj* nic,char* coming_mux_id, char* buffer, uint32_t buffer_size, uint32_t timeout)
{
    return recvPkg(nic,buffer, buffer_size, NULL, timeout, coming_mux_id, NULL, false);
}
//#include "printf.h"
/*----------------------------------------------------------------------------*/
/* +IPD,<id>,<len>:<data> */
/* +IPD,<len>:<data> */
/**
 * 
 * @return -1: parameters error, -2: EOF, -3: timeout, -4:peer closed and no data in buffer
 */
uint32_t recvPkg(esp8285_obj*nic,char* out_buff, uint32_t out_buff_len, uint32_t *data_len, uint32_t timeout, char* coming_mux_id, bool* peer_closed, bool first_time_recv)
{
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);

    uint8_t temp_buff[16];
    uint8_t temp_buff2[16];
	uint8_t temp_read[64];
	uint8_t temp_read_len = 0;
    uint8_t temp_buff_len = 0;
    uint8_t temp_buff2_len = 0;
    uint8_t find_frame_flag_index = 0;
    static int8_t mux_id = -1;
    static int16_t frame_len = 0;
    static int32_t frame_len_sum = 0;//only for single socket TODO:
    // bool    overflow = false;
    int ret = 0;
    uint32_t size = 0;
	int errcode;
    mp_uint_t start = 0, start2 = 0;
    bool no_frame_flag = false;
    bool new_frame = false;
    mp_uint_t data_len_in_spi_buff = 0;
    bool peer_just_closed = false;
    
    // parameters check
    if (out_buff == NULL) {
        return -1;
    }
    // init vars
    memset(temp_buff, 0, sizeof(temp_buff));
    memset(temp_buff, 0, sizeof(temp_buff2));
    if(first_time_recv)
    {
        frame_len = 0;
        frame_len_sum = 0;
    }

    // required data already in buf, just return data
    uint32_t buff_size = Buffer_Size(&nic->buffer);
    if(buff_size >= out_buff_len)
    {
        Buffer_Gets(&nic->buffer, (uint8_t*)out_buff, out_buff_len);
        if(data_len)
            *data_len = out_buff_len;
        frame_len_sum -= size;
        if(frame_len_sum <= 0)
        {
            frame_len = 0;
            frame_len_sum = 0;
            Buffer_Clear(&nic->buffer);
            if(*peer_closed)//buffer empty, return EOF
            {
                return -2;
            }
        }
        return out_buff_len;
    }
    
    // read from spi buffer, if not frame start flag, put into nic buffer
    // and need wait for full frame flag in 200ms(can be fewer), frame format: '+IPD,id,len:data' or '+IPD,len:data'
    // wait data from spi buffer if not timeout
    start2 = mp_hal_ticks_ms();
	machine_spi_obj_t *self = MP_OBJ_TO_PTR(nic->spi_obj);
    do{
        //self->read_lock = true;
        //spi_drain_rx_fifo(self);
        //self->read_lock = false;
		mp_hal_pin_input(SPI_HANDSHARK);
        if(gpio_get(SPI_HANDSHARK) > 0)
        {
			if(temp_read_len == 0 || temp_read_len == 64){
				readdata(nic->spi_obj,temp_read);
				temp_read_len = 0;
			}
			temp_buff[temp_buff_len] = temp_read[++temp_read_len];
            if(find_frame_flag_index == 0 && temp_buff[temp_buff_len] == '+'){
                ++find_frame_flag_index;
                start = mp_hal_ticks_ms();
            }
            else if(find_frame_flag_index == 1 && temp_buff[temp_buff_len] == 'I'){
                ++find_frame_flag_index;
            }
            else if(find_frame_flag_index == 2 && temp_buff[temp_buff_len] == 'P'){
                ++find_frame_flag_index;
            }
            else if(find_frame_flag_index == 3 && temp_buff[temp_buff_len] == 'D'){
                ++find_frame_flag_index;
            }
            else if(find_frame_flag_index == 4 && temp_buff[temp_buff_len] == ','){
                ++find_frame_flag_index;
            }
            else if(find_frame_flag_index == 5){
                    if(temp_buff[temp_buff_len] == ':'){ // '+IPD,3,1452:' or '+IPD,1452:'
                        temp_buff[temp_buff_len+1] = '\0';
                        char* index = strstr((char*)temp_buff+5, ",");
                        if(index){ // '+IPD,3,1452:'
                            ret = sscanf((char*)temp_buff,"+IPD,%hhd,%hd:",&mux_id,&frame_len);
                            if (ret!=2 || mux_id < 0 || mux_id > 4 || frame_len<=0) { // format not satisfy, it's data
                                no_frame_flag = true;
                            }else{// find frame start flag, although it may also data
                                new_frame = true;
                            }
                        }else{ // '+IPD,1452:'
                            ret = sscanf((char*)temp_buff,"+IPD,%hd:",&frame_len);
                            if (ret !=1 || frame_len<=0) { // format not satisfy, it's data
                                no_frame_flag = true;
                            }else{// find frame start flag, although it may also data
                                new_frame = true;
                                // printk("new frame:%d\r\n", frame_len);
                            }
                        }
                    }
            }
            else{ // not match frame start flag, put into nic buffer
                no_frame_flag = true;
            }
            // new frame or data
            // or wait for frame start flag timeout(300ms, can be fewer), maybe they're data
            if(new_frame || no_frame_flag || temp_buff_len >= 12 ||
                (find_frame_flag_index && (mp_hal_ticks_ms() - start > 300) )
            ) // '+IPD,3,1452:'
            {
                if(!new_frame){
                    if(frame_len_sum > 0){
                        if(!Buffer_Puts(&nic->buffer, temp_buff, temp_buff_len+1))
                        {
                            // overflow = true;
                            // break;//TODO:
                        }
                    }
                    else{
                        if(temp_buff[0]=='C'){
                            memset(temp_buff2, 0, sizeof(temp_buff2));
                        }
                        temp_buff2[temp_buff2_len++] = temp_buff[0];
                        // printk("%c", temp_buff[0]); //TODO: optimize spi overflow, if spi overflow, uncomment this will print some data
                        // printk("-%d:%s\r\n", temp_buff2_len, temp_buff2);
                        if(strstr((const char*)temp_buff2, "CLOSED\r\n") != NULL){
                            // printk("pear closed\r\n");
                            *peer_closed = true;
                            peer_just_closed = true;
                            break;
                        }
                    }
                }else{
                    frame_len_sum += frame_len;
                }
                find_frame_flag_index = 0;
                temp_buff_len = 0;
                new_frame = false;
                no_frame_flag = false;
                // enough data as required
                size = Buffer_Size(&nic->buffer);
                if( size >= out_buff_len) // data enough
                    break;
                if(frame_len_sum!=0 && frame_len_sum <= size) // read at least one frame ok
                {
                    break;
                }
                continue;
            }
            ++temp_buff_len;
        }
        if(timeout!=0 && (mp_hal_ticks_ms() - start2 > timeout) && !find_frame_flag_index )
        {
            return -3;
        }
        //self->read_lock = true;
        //spi_drain_rx_fifo(self);
        //self->read_lock = false;
		mp_hal_pin_input(SPI_HANDSHARK);
    }while( (timeout || find_frame_flag_index) && (!*peer_closed || gpio_get(SPI_HANDSHARK) > 0) );
    size = Buffer_Size(&nic->buffer);
    if( size == 0 && !peer_just_closed && *peer_closed)//peer closed and no data in buffer
    {
        frame_len = 0;
        frame_len_sum = 0;
        return -4;
    }
    size = size > out_buff_len ? out_buff_len : size;
    Buffer_Gets(&nic->buffer, (uint8_t*)out_buff, size);
    if(data_len)
        *data_len = size;
    frame_len_sum -= size;
    if(frame_len_sum <= 0 || peer_just_closed)
    {
        frame_len = 0;
        frame_len_sum = 0;
        Buffer_Clear(&nic->buffer);
        if(peer_just_closed)
        {
            return -2;
        }
    }
    return size;
}
char *strncpy_data(char *s1, const char *s2, size_t n) {
     char *dst = s1;
     const char *src = s2;
     /* Copy bytes, one at a time.  */
     while (n > 0) {
        n--;
        *dst++ = *src++;
     }
     return s1;
 }
static uint32_t read_time = 0;
uint32_t readCmd(esp8285_obj* nic, char* data)
{
	static uint32_t read_last_len = 0;
	uint8_t tmp = 0;
	spi_trans_len trans_len;
	spi_trans_data trans_data;
	memset(&trans_len, 0x0, sizeof(trans_len));
	memset(&trans_data, 0x0, sizeof(trans_data)); 
	uint8_t addr = 0x0;
	const mp_machine_spi_p_t * spi_stream = (const mp_machine_spi_p_t *)((const mp_obj_base_t *)MP_OBJ_TO_PTR(nic->spi_obj))->type->protocol;
	int data_len = sizeof(trans_len);
	if(read_time == 0){
		trans_len.cmd = SPI_MASTER_READ_STATUS_FROM_SLAVE_CMD;
		trans_len.len = 0;
		int data_len = sizeof(trans_len);
		mp_hal_pin_output(SPI_CS);
		gpio_put(SPI_CS,0);
		spi_stream->transfer(nic->spi_obj,sizeof(trans_len.cmd),(uint8_t *)&trans_len.cmd,(uint8_t *)&trans_len.cmd);
		spi_stream->transfer(nic->spi_obj,sizeof(trans_len.len),(uint8_t *)&trans_len.len,(uint8_t *)&trans_len.len);
		gpio_put(SPI_CS,1);
		read_time = (trans_len.len + 63) / 64;
		read_last_len = (trans_len.len + 64) % 64;
	}
	if(read_time > 0) {
		data_len = sizeof(trans_data);
		trans_data.cmd = SPI_MASTER_READ_DATA_FROM_SLAVE_CMD;
		trans_data.addr = 0x00;	
		sleep_us(50);
		gpio_put(SPI_CS,0);
		spi_stream->transfer(nic->spi_obj,data_len,(uint8_t *)&trans_data,(uint8_t *)&trans_data);
		gpio_put(SPI_CS,1);
		memcpy(data, trans_data.data, sizeof(trans_data.data));
		sleep_us(180);
		tmp = 64;
		read_time--;
		data += 64;
	}
	if(!read_time){
		*(data+read_last_len) = "\0";
		return read_last_len;
	}
	return tmp;
}

void sendCmd(esp8285_obj* nic, char* data, uint32_t data_size)
{
	uint32_t send_time = 0;
    spi_trans_data trans_data;
	spi_trans_len trans_len;
	const mp_machine_spi_p_t * spi_stream = mp_get_stream(nic->spi_obj);
 	memset(&trans_len, 0x0, sizeof(trans_len));
	trans_len.cmd = SPI_MASTER_WRITE_STATUS_TO_SLAVE_CMD;
	trans_len.len = data_size;
	int data_len = sizeof(spi_trans_len);
	mp_hal_pin_output(SPI_CS);
	gpio_put(SPI_CS,0);
	spi_stream->transfer(nic->spi_obj,sizeof(trans_len.cmd),(uint8_t *)&trans_len.cmd,(uint8_t *)&trans_len.cmd);
	spi_stream->transfer(nic->spi_obj,sizeof(trans_len.len),(uint8_t *)&trans_len.len,(uint8_t *)&trans_len.len);
	gpio_put(SPI_CS,1);
	unsigned long start = 0;
	send_time = (data_size + 63) / 64;
	while(send_time > 0) {
		sleep_us(100);
		memset(&trans_data, 0x0, sizeof(trans_data));           // clear all bit
		data_len = sizeof(trans_data);
		trans_data.cmd = SPI_MASTER_WRITE_DATA_TO_SLAVE_CMD;
		trans_data.addr = 0x0;
		if(send_time = 1)
			memcpy(&trans_data.data, data, strlen(data));
		else
			memcpy(&trans_data.data, data, 64);
		start = mp_hal_ticks_ms();
		while( gpio_get(SPI_HANDSHARK) == 0){
			if(mp_hal_ticks_ms() - start < 1000)
				return false;
		}
		gpio_put(SPI_CS,0);
		spi_stream->transfer(nic->spi_obj,data_len,(uint8_t *)&trans_data,0);
		gpio_put(SPI_CS,1);
		sleep_us(100);
		send_time--;
		data += 64;
	}	

	start = mp_hal_ticks_ms();
	while( gpio_get(SPI_HANDSHARK) == 0){
		if(mp_hal_ticks_ms() - start < 1000)
			return false;
	}
 	memset(&trans_len, 0x0, sizeof(trans_len));
	trans_len.cmd = SPI_MASTER_WRITE_STATUS_TO_SLAVE_CMD;
	trans_len.len = 0;
	data_len = sizeof(spi_trans_len);
	gpio_put(SPI_CS,0);
	spi_stream->transfer(nic->spi_obj,sizeof(trans_len.cmd),(uint8_t *)&trans_len.cmd,(uint8_t *)&trans_len.cmd);
	spi_stream->transfer(nic->spi_obj,sizeof(trans_len.len),(uint8_t *)&trans_len.len,(uint8_t *)&trans_len.len);
	gpio_put(SPI_CS,1);
	read_time = 0;
}

void rx_empty(esp8285_obj* nic) 
{
	int errcode;
	char *data;
    mp_hal_pin_input(SPI_HANDSHARK);
    while(gpio_get(SPI_HANDSHARK) > 0){
		readCmd(nic, data); 
    }
}

char* recvString_1(esp8285_obj* nic, const char* target1,uint32_t timeout)
{
	int errcode;
	uint32_t iter = 0;
	memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);
    unsigned long start = mp_hal_ticks_ms();
	machine_spi_obj_t *self = MP_OBJ_TO_PTR(nic->spi_obj);
	while (mp_hal_ticks_ms() - start < timeout) {
        while(gpio_get(SPI_HANDSHARK) > 0){
			iter += readCmd(nic, nic->buffer.buffer+iter);	
        }
        if (data_find(nic->buffer.buffer,iter,target1) != -1) {
            return (char*)nic->buffer.buffer;
        } 
	}
    return NULL;
}


char* recvString_2(esp8285_obj* nic,char* target1, char* target2, uint32_t timeout, int8_t* find_index)
{
	int errcode;
	uint32_t iter = 0;
    *find_index = -1;
	memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);
    unsigned int start = mp_hal_ticks_ms();
	machine_spi_obj_t *self = MP_OBJ_TO_PTR(nic->spi_obj);
    while (mp_hal_ticks_ms() - start < timeout) {
        while(gpio_get(SPI_HANDSHARK) > 0 && iter < ESP8285_BUF_SIZE) {
			iter += readCmd(nic, nic->buffer.buffer+iter);	
        }
        if (data_find(nic->buffer.buffer,iter,target1) != -1) {
            *find_index = 0;
			if (data_find(nic->buffer.buffer,iter,target2) != -1) 
				*find_index = 1;
            return (char*)nic->buffer.buffer;
        } else if (data_find(nic->buffer.buffer,iter,target2) != -1) {
            *find_index = 1;
            return (char*)nic->buffer.buffer;
        }
    }
    return NULL;
}

char* recvString_3(esp8285_obj* nic,char* target1, char* target2,char* target3,uint32_t timeout, int8_t* find_index)
{

	int errcode;
	uint32_t iter = 0;
    *find_index = -1;
	memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);
    unsigned long start = mp_hal_ticks_ms();
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	machine_spi_obj_t *self = MP_OBJ_TO_PTR(nic->spi_obj);
    while (mp_hal_ticks_ms() - start < timeout) {
        //self->read_lock = true;
        //spi_drain_rx_fifo(self);
        //self->read_lock = false;
        while(gpio_get(SPI_HANDSHARK) > 0) {
            iter += readCmd(nic, nic->buffer.buffer+iter);
        }
        if (data_find(nic->buffer.buffer,iter,target1) != -1) {
            *find_index = 0;
            return (char*)nic->buffer.buffer;
        } else if (data_find(nic->buffer.buffer,iter,target2) != -1) {
            *find_index = 1;
            return (char*)nic->buffer.buffer;
        } else if (data_find(nic->buffer.buffer,iter,target3) != -1) {
            *find_index = 2;
            return (char*)nic->buffer.buffer;
        }
    }
    return NULL;
}

bool recvFind(esp8285_obj* nic, const char* target, uint32_t timeout)
{
    recvString_1(nic, target, timeout);
    if (data_find(nic->buffer.buffer,ESP8285_BUF_SIZE,target) != -1) {
        return true;
    }
    return false;
}

bool recvFindAndFilter(esp8285_obj* nic,const char* target, const char* begin, const char* end, char** data, uint32_t timeout)
{
    recvString_1(nic,target, timeout);
    if (data_find(nic->buffer.buffer,ESP8285_BUF_SIZE,target) != -1) {
        int32_t index1 = data_find(nic->buffer.buffer,ESP8285_BUF_SIZE,begin);
        int32_t index2 = data_find(nic->buffer.buffer,ESP8285_BUF_SIZE,end);
        if (index1 != -1 && index2 != -1) {
            index1 += strlen(begin);
			*data = m_new(char, index2 - index1);
			memcpy(*data,nic->buffer.buffer+index1, index2 - index1);
            return true;
        }
    }
    return false;
}

bool eAT(esp8285_obj* nic)
{	
	int errcode = 0;
	const char* cmd = "AT\r\n";
    rx_empty(nic);// clear rx
	sendCmd(nic,cmd,strlen(cmd));
	if(recvFind(nic,"OK",1000))
		return true;
    rx_empty(nic);// clear rx
	sendCmd(nic,cmd,strlen(cmd));
	if(recvFind(nic,"OK",1000))
		return true;
    rx_empty(nic);// clear rx
	sendCmd(nic,cmd,strlen(cmd));
	if(recvFind(nic,"OK",1000))
		return true;
	return false;
}

bool eATE(esp8285_obj* nic,bool enable)
{	
	int errcode = 0;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    rx_empty(nic);// clear rx
    if(enable)
    {
    	const char* cmd = "ATE0\r\n";
		sendCmd(nic,cmd,strlen(cmd));
    	return recvFind(nic,"OK",1000);
    }
	else
	{
    	const char* cmd = "ATE1\r\n";
		sendCmd(nic,cmd,strlen(cmd));
    	return recvFind(nic,"OK",1000);		
	}
}


bool eATRST(esp8285_obj* nic) 
{
	int errcode = 0;
	const char* cmd = "AT+RST\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    rx_empty(nic);// clear rx
	sendCmd(nic,cmd,strlen(cmd));
    return recvFind(nic,"OK",1000);
}

bool eATGMR(esp8285_obj* nic,char** version)
{

	int errcode = 0;
	const char* cmd = "AT+GMR\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    rx_empty(nic);// clear rx
	sendCmd(nic,cmd,strlen(cmd));

    return recvFindAndFilter(nic,"OK", "\r\r\n", "\r\n\r\nOK", version, 5000); 
}

bool qATCWMODE(esp8285_obj* nic,char* mode) 
{
	int errcode = 0;
	const char* cmd = "AT+CWMODE?\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char* str_mode;
    bool ret;
    if (!mode) {
        return false;
    }
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    ret = recvFindAndFilter(nic,"OK", "+CWMODE:", "\r\n\r\nOK", &str_mode,1000); 
    if (ret) {
        *mode = atoi(str_mode);
        return true;
    } else {
        return false;
    }
}

bool sATCWMODE(esp8285_obj* nic,char mode)
{
	int errcode = 0;
	const char* cmd = "AT+CWMODE=";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
	char mode_str[10] = {0};
    int8_t find;
	itoa(mode, mode_str, 10);
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,mode_str,strlen(mode_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));  
    if(recvString_2(nic,"OK", "no change",1000, &find) != NULL)
        return true;
    return false;
}

bool sATCWJAP(esp8285_obj* nic, const char* ssid, const char* pwd)
{
	int errcode = 0;
	const char* cmd = "AT+CWJAP=\"";
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));	
	sendCmd(nic,ssid,strlen(ssid));
	sendCmd(nic,"\",\"",strlen("\",\""));
	sendCmd(nic,pwd,strlen(pwd));
	sendCmd(nic,"\"",strlen("\""));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_2(nic,"OK", "ERROR", 20000, &find) != NULL && find==0 )
        return true;
    return false;
}

bool sATCWDHCP(esp8285_obj* nic,char mode, bool enabled)
{
	int errcode = 0;
	const char* cmd = "AT+CWDHCP=";
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
	char strEn[2] = {0};
	if (enabled) {
		strcpy(strEn, "1");
	}
	else
	{
		strcpy(strEn, "0");
	}
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,strEn,strlen(strEn));
	sendCmd(nic,",",strlen(","));
	sendCmd(nic, &mode,1);
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if( recvString_2(nic,"OK", "FAIL", 10000, &find) != NULL && find==0)
        return true;    
    return false;
}


bool eATCWQAP(esp8285_obj* nic)
{
	int errcode = 0;
	const char* cmd = "AT+CWQAP\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    return recvFind(nic,"OK",1000);
}

bool eATCIPSTATUS(esp8285_obj* nic,char** list)
{
    int errcode = 0;
	const char* cmd = "AT+CIPSTATUS\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
	sendCmd(nic,cmd,strlen(cmd));
    sleep_ms(100);
    rx_empty(nic);
    sendCmd(nic,cmd,strlen(cmd));
    return recvFindAndFilter(nic,"OK", "\r\r\n", "\r\n\r\nOK", list,1000);
}
bool sATCIPSTARTSingle(esp8285_obj* nic,const char* type, char* addr, uint32_t port)
{
    int errcode = 0;
	const char* cmd = "AT+CIPSTART=\"";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	mp_obj_t IP = netutils_format_ipv4_addr((uint8_t*)addr,NETUTILS_BIG);
	const char* host = mp_obj_str_get_str(IP);
	char port_str[10] = {0};
    int8_t find_index;
	itoa(port, port_str, 10);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,type,strlen(type));
	sendCmd(nic,"\",\"",strlen("\",\""));
	sendCmd(nic,host,strlen(host));
	sendCmd(nic,"\",",strlen("\","));
	sendCmd(nic,port_str,strlen(port_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_3(nic,"OK", "ERROR", "ALREADY CONNECT", 10000, &find_index)!=NULL && (find_index==0 || find_index==2) )
        return true;
    return false;
}
bool sATCIPSTARTMultiple(esp8285_obj*nic,char mux_id, char* type, char* addr, uint32_t port)
{
    int errcode = 0;
	const char* cmd = "AT+CIPSTART=";
	char port_str[10] = {0};
    int8_t find_index;
	itoa(port,port_str ,10);
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,&mux_id,1);
	sendCmd(nic,",\"",strlen(",\""));
	sendCmd(nic,type,strlen(type));
	sendCmd(nic,"\",\"",strlen("\",\""));
	sendCmd(nic,addr,strlen(addr));
	sendCmd(nic,"\",",strlen("\","));
	sendCmd(nic,port_str,strlen(port_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_3(nic,"OK", "ERROR", "ALREADY CONNECT", 10000, &find_index) != NULL  && (find_index==0 || find_index==2) )
        return true;
    return false;
}
bool sATCIPSENDSingle(esp8285_obj*nic,const char* buffer, uint32_t len, uint32_t timeout)
{
	int errcode = 0;
	const char* cmd = "AT+CIPSEND=";
	char len_str[10] = {0};
	itoa(len,len_str ,10);
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,len_str,strlen(len_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if (recvFind(nic,">", 5000)) {
        rx_empty(nic);
		sendCmd(nic,buffer,len);
        return recvFind(nic,"SEND OK", timeout);
    }
    return false;
}
bool sATCIPSENDMultiple(esp8285_obj* nic,char mux_id, const char* buffer, uint32_t len)
{
   	int errcode = 0;
	const char* cmd = "AT+CIPSEND=";
	char len_str[10] = {0};
	itoa(len,len_str ,10);
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,&mux_id,1);
	sendCmd(nic,",",strlen(","));
	sendCmd(nic,len_str,strlen(len_str));
    if (recvFind(nic,">", 5000)) {
        rx_empty(nic);
		sendCmd(nic,buffer,len);
		sendCmd(nic,"\r\n",strlen("\r\n"));
        return recvFind(nic,"SEND OK", 10000);
    }
    return false;
}
bool sATCIPCLOSEMulitple(esp8285_obj* nic,char mux_id)
{
	int errcode = 0;
	const char* cmd = "AT+CIPCLOSE=";
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,(const char*)&mux_id,1);
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_2(nic,"OK", "link is not", 5000, &find) != NULL)
        return true;
    return false;
}
bool eATCIPCLOSESingle(esp8285_obj* nic)
{

    int8_t find;
	int errcode = 0;
	const char* cmd = "AT+CIPCLOSE\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    if (recvString_2(nic, "OK", "ERROR", 5000, &find) != NULL)
    {
        if( find == 0)
            return true;
    }
    return false;
}
bool eATCIFSR(esp8285_obj* nic,char** list)
{
	int errcode = 0;
	const char* cmd = "AT+CIFSR\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    return recvFindAndFilter(nic,"OK", "\r\r\n", "\r\n\r\nOK", list,5000);
}
bool sATCIPMUX(esp8285_obj* nic,char mode)
{
	int errcode = 0;
	const char* cmd = "AT+CIPMUX=";
	char mode_str[10] = {0};
    int8_t find;
	itoa(mode, mode_str, 10);
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,mode_str,strlen(mode_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_2(nic,"OK", "Link is builded",5000, &find) != NULL && find==0)
        return true;
    return false;
}
bool sATCIPSERVER(esp8285_obj* nic,char mode, uint32_t port)
{
	int errcode = 0;
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    if (mode) {
		const char* cmd = "AT+CIPSERVER=1,";
		char port_str[10] = {0};
		itoa(port, port_str, 10);
        rx_empty(nic);
		sendCmd(nic,cmd,strlen(cmd));
		sendCmd(nic,port_str,strlen(port_str));
		sendCmd(nic,"\r\n",strlen("\r\n"));
        if(recvString_2(nic,"OK", "no change",1000, &find) != NULL)
            return true;
        return false;
    } else {
        rx_empty(nic);
		const char* cmd = "AT+CIPSERVER=0";
		sendCmd(nic,cmd,strlen(cmd));
		sendCmd(nic,"\r\n",strlen("\r\n"));
        return recvFind(nic,"\r\n",1000);
    }
}
bool sATCIPSTO(esp8285_obj* nic,uint32_t timeout)
{

	int errcode = 0;
	const char* cmd = "AT+CIPSTO=";
	char timeout_str[10] = {0};
	itoa(timeout, timeout_str, 10);
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,timeout_str,strlen(timeout_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    return recvFind(nic,"OK",1000);
}

bool sATCIPMODE(esp8285_obj* nic,char mode)
{

	int errcode = 0;
	const char* cmd = "AT+CIPMODE=";
	char mode_str[10] = {0};
	itoa(mode, mode_str, 10);
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,mode_str,strlen(mode_str));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    return recvFind(nic,"OK",1000);
}

bool sATCIPDOMAIN(esp8285_obj* nic, const char* domain_name, uint32_t timeout)
{
	int errcode = 0;
	const char* cmd = "AT+CIPDOMAIN=";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\"",strlen("\""));
	sendCmd(nic,domain_name,strlen(domain_name));
	sendCmd(nic,"\"",strlen("\""));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    return recvFind(nic,"OK",timeout);
}

bool qATCIPSTA_CUR(esp8285_obj* nic)
{
	int errcode = 0;
	const char* cmd = "AT+CIPSTA?";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));
	return recvFind(nic,"OK",1000);
}
bool sATCIPSTA_CUR(esp8285_obj* nic, const char* ip,char* gateway,char* netmask)
{
	int errcode = 0;
	const char* cmd = "AT+CIPSTA_CUR=";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	if(NULL == ip)
	{
		return false;
	}
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,ip,strlen(ip));
	if(NULL == gateway)
	{
		sendCmd(nic,"\r\n",strlen("\r\n"));
		return recvFind(nic,"OK",1000);
	}
	sendCmd(nic,",",strlen(","));
	sendCmd(nic,gateway,strlen(gateway));
	if(NULL == netmask)
	{
		sendCmd(nic,"\r\n",strlen("\r\n"));
		return recvFind(nic,"OK",1000);
	}
	sendCmd(nic,",",strlen(","));
	sendCmd(nic,netmask,strlen(netmask));
	sendCmd(nic,"\r\n",strlen("\r\n"));
	return recvFind(nic,"OK",1000);
}

bool qATCWJAP_CUR(esp8285_obj* nic)
{
	int errcode = 0;
	const char* cmd = "AT+CWJAP?";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));
	return recvFind(nic,"OK",1000);
}

bool eINIT(esp8285_obj* nic, int mode)
{
	bool init_flag = 1;
	init_flag = init_flag && eAT(nic);
	init_flag = init_flag && eATE(nic,1);
	init_flag = init_flag && disableMUX(nic);
	init_flag = init_flag && sATCIPMODE(nic,0);
	init_flag = init_flag && setOprToStation(nic, mode);
   if(mode & SOFTAP_MODE){
		init_flag = init_flag && enableMUX(nic);
	}
	else{
		init_flag = init_flag && disableMUX(nic);
	}
	if(!mode & SOFTAP_MODE){
		init_flag = init_flag && leaveAP(nic);
	}
	return init_flag;
}

bool eATCWLAP(esp8285_obj* nic)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    const char cmd[] = {"AT+CWLAP"};

    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\n\r\nOK", 10000) != NULL)
        return true;
    
    return false;
}

bool eATCWLAP_Start(esp8285_obj* nic)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    const char cmd[] = {"AT+CWLAP\r\n"};

    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    return true;
}

bool eATCWLAP_Get(esp8285_obj* nic, bool* end)
{
    int8_t find;
    *end = false;
    if (recvString_2(nic, "\r\n+CWLAP:", "\r\n\r\nOK", 10000, &find) != NULL)
    {
        if( find == 1)
            *end = true;
        return true;
    }
    return false;
}

bool qATCWSAP(esp8285_obj* nic)
{
	int errcode = 0;
	const char* cmd = "AT+CWSAP?";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));
	return recvFind(nic,"OK",1000);
}
bool eATCWSAP(esp8285_obj* nic, const char* ssid, const char* key, int chl, int ecn)
{
    int errcode;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char ap_cmd[128] = {0};

    if (sATCWMODE(nic, 3) == false)
    {
        return false;
    }

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(ap_cmd, "AT+CWSAP=\"%s\",\"%s\",%d,%d", ssid, key, chl, ecn);
	sendCmd(nic,ap_cmd,strlen(ap_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }

    return true;
}
bool sATCWSAP(esp8285_obj* nic, const char* ssid, const char* key, int chl, int ecn, int max_conn, int ssid_hidden)
{
    int errcode;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char ap_cmd[128] = {0};

    if (sATCWMODE(nic, 3) == false)
    {
        return false;
    }

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(ap_cmd, "AT+CWSAP=\"%s\",\"%s\",%d,%d,%d,%d", ssid, key, chl, ecn, max_conn, ssid_hidden);
	sendCmd(nic,ap_cmd,strlen(ap_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }

    return true;
}
bool sCIPSTAMAC(esp8285_obj* nic, const char* mac)
{
	int errcode = 0;
	const char* cmd = "AT+CIPSTAMAC=\"";
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);	
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,mac,strlen(mac));
	sendCmd(nic,"\"",strlen("\""));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_2(nic,"OK", "ERROR", 10000, &find) != NULL && find==0 )
        return true;
    return false;
}
bool sCIPAPMAC(esp8285_obj* nic, const char* mac)
{
	int errcode = 0;
	const char* cmd = "AT+CIPAPMAC=\"";
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);	
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,mac,strlen(mac));
	sendCmd(nic,"\"",strlen("\""));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_2(nic,"OK", "ERROR", 10000, &find) != NULL && find==0 )
        return true;
    return false;
}
bool qCIPSTAMAC(esp8285_obj* nic,char* mac) 
{
	int errcode = 0;
    char* cur = NULL;
	const char* cmd = "AT+CIPSTAMAC?\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    bool ret;
    if (!mac) {
        return false;
    }
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    ret = recvFind(nic,"OK",1000); 
    cur = strstr((char*)nic->buffer.buffer, "+CIPSTAMAC:");
	sscanf(cur, "+CIPSTAMAC:\"%[^\"]\"", mac);
    if (ret) {
        return true;
    } else {
        return false;
    }
}
bool qCIPAPMAC(esp8285_obj* nic,char* mac) 
{
	int errcode = 0;
    char* cur = NULL;
	const char* cmd = "AT+CIPAPMAC?\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    bool ret;
    if (!mac) {
        return false;
    }
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    ret = recvFind(nic,"OK",1000); 
    cur = strstr((char*)nic->buffer.buffer, "+CIPAPMAC:");
	sscanf(cur, "+CIPAPMAC:\"%[^\"]\"", mac);
    if (ret) {
        return true;
    } else {
        return false;
    }
}
bool sCWHOSTNAME(esp8285_obj* nic, const char* ssid)
{
	int errcode = 0;
	const char* cmd = "AT+CWHOSTNAME=\"";
    int8_t find;
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);	
    rx_empty(nic);	
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,ssid,strlen(ssid));
	sendCmd(nic,"\"",strlen("\""));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    if(recvString_2(nic,"OK", "ERROR", 10000, &find) != NULL && find==0 )
        return true;
    return false;
}
bool qCWHOSTNAME(esp8285_obj* nic,char* ssid) 
{
	int errcode = 0;
    char* cur = NULL;
	const char* cmd = "AT+CWHOSTNAME?\r\n";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    bool ret;
    if (!ssid) {
        return false;
    }
    rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
    ret = recvFind(nic,"OK",1000); 
    cur = strstr((char*)nic->buffer.buffer, "+CWHOSTNAME:");
	sscanf(cur, "+CWHOSTNAME:%s", ssid);
    if (ret) {
        return true;
    } else {
        return false;
    }
}
bool sMQTTUSERCFG(esp8285_obj* nic,int LinkID, int scheme, const char* client_id, const char* username, const char* password, int cert_key_ID, int CA_ID, const char* path)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTUSERCFG=%d,%d,\"%s\",\"%s\",\"%s\",%d,%d,\"%s\"", LinkID, scheme, client_id, username, password, cert_key_ID, CA_ID, path);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }

    return true;
}
bool sMQTTUSERNAME(esp8285_obj* nic,int LinkID, int username)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTUSERNAME=%d,\"%s\"", LinkID, username);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }

    return true;
}
bool sMQTTPASSWORD(esp8285_obj* nic,int LinkID, int password)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTUSERNAME=%d,\"%s\"", LinkID, password);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }
    return true;
}
bool sMQTTCONNCFG(esp8285_obj* nic,int LinkID, int keepalive, int disable_clean_session, const char* lwt_topic, const char* wt_msg, int lwt_qos, int lwt_retain)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTCONNCFG=%d,%d,%d,\"%s\",\"%s\",%d,%d", LinkID, keepalive, disable_clean_session, lwt_topic, wt_msg, lwt_qos, lwt_retain);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }

    return true;
}
bool sMQTTCONN(esp8285_obj* nic,int LinkID, const char* host, int port, int reconnect)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTCONN=%d,\"%s\",%d,%d", LinkID, host, port, reconnect);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }
    return true;
}
bool qMQTTCONN(esp8285_obj* nic)
{
	int errcode = 0;
	const char* cmd = "AT+MQTTCONN?";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));
	return recvFind(nic,"OK",1000);
}
bool sMQTTPUB(esp8285_obj*nic, uint32_t LinkID,  const char* topic,  const char* data, uint32_t qos, uint32_t retain)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTPUB=%d,\"%s\",\"%s\",%d,%d", LinkID, topic, data, qos, retain);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }
    return true;
}
bool qMQTTSUB(esp8285_obj*nic, uint32_t LinkID, const char* topic, uint32_t qos)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    sprintf(mqtt_cmd, "AT+MQTTSUB=%d,\"%s\",%d", LinkID, topic, qos);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));
    return recvFind(nic,"OK",1000);
}
bool eMQTTSUB_Start(esp8285_obj* nic)
{
	int errcode = 0;
	const char* cmd = "AT+MQTTSUB?";
	const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	rx_empty(nic);
	sendCmd(nic,cmd,strlen(cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));
	return true;
}
bool eMQTTSUB_Get(esp8285_obj* nic, bool* end)
{
    int8_t find;
    *end = false;
    if (recvString_2(nic, "\r\n+MQTTSUB:", "\r\n\r\nOK", 10000, &find) != NULL)
    {
        if( find == 1)
            *end = true;
        return true;
    }
    return false;
}
bool sMQTTUNSUB(esp8285_obj*nic, uint32_t LinkID,  const char* topic)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTUNSUB=%d,\"%s\"", LinkID, topic);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }
    return true;
}
bool sMQTTCLEAN(esp8285_obj*nic, uint32_t LinkID)
{
    int errcode = 0;
    const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
    char mqtt_cmd[128] = {0};

    rx_empty(nic);
    memset(nic->buffer.buffer,0,ESP8285_BUF_SIZE);

    sprintf(mqtt_cmd, "AT+MQTTCLEAN=%d", LinkID);
	sendCmd(nic,mqtt_cmd,strlen(mqtt_cmd));
	sendCmd(nic,"\r\n",strlen("\r\n"));

    if (recvString_1(nic, "\r\nOK", 3000) == NULL)
    {
        return false;
    }
    return true;
}
// bool sMQTTPUBRAW(esp8285_obj*nic, uint32_t LinkID, const char* topic, uint32_t length, uint32_t qos, uint32_t retain)
// {
	// int errcode = 0;
	// const char* cmd = "AT+MQTTPUBRAW=";
	// char len_str[10] = {0};
	// itoa(len,len_str ,10);
	// const mp_stream_p_t * spi_stream = mp_get_stream(nic->spi_obj);
	// rx_empty(nic);
    // sprintf(ap_cmd, "AT+MQTTPUB=%d,\"%s\",%d,%d,%d", LinkID, topic, length, qos, retain);
    // spi_stream->write(nic->spi_obj,ap_cmd, strlen(ap_cmd), &errcode);
    // spi_stream->write(nic->spi_obj, "\r\n", strlen("\r\n"), &errcode);
    // if (recvFind(nic,">", 5000)) {
        // rx_empty(nic);
		// spi_stream->write(nic->spi_obj,buffer,len,&errcode);
        // return recvFind(nic,"SEND OK", timeout);
    // }
    // return false;
// }
//bool setOprToSoftAP(void)
//{
//    char mode;
//    if (!qATCWMODE(&mode)) {
//        return false;
//    }
//    if (mode == 2) {
//        return true;
//    } else {
//        if (sATCWMODE(2) && reset()) {
//            return true;
//        } else {
//            return false;
//        }
//    }
//}

//bool setOprToStationSoftAP(void)
//{
//    char mode;
//    if (!qATCWMODE(&mode)) {
//        return false;
//    }
//    if (mode == 3) {
//        return true;
//    } else {
//        if (sATCWMODE(3) && reset()) {
//            return true;
//        } else {
//            return false;
//        }
//    }
//}

//String getAPList(void)
//{
//    String list;
//    eATCWLAP(list);
//    return list;
//}


//bool setSoftAPParam(String ssid, String pwd, char chl, char ecn)
//{
//    return sATCWSAP(ssid, pwd, chl, ecn);
//}

//String getJoinedDeviceIP(void)
//{
//    String list;
//    eATCWLIF(list);
//    return list;
//}

//bool eATCWLAP(String &list)
//{
//    String data;
//    rx_empty();
//    m_pspi->println("AT+CWLAP");
//    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", list, 10000);
//}


//bool sATCWSAP(String ssid, String pwd, char chl, char ecn)
//{
//    String data;
//    rx_empty();
//    m_pspi->print("AT+CWSAP=\"");
//    m_pspi->print(ssid);
//    m_pspi->print("\",\"");
//    m_pspi->print(pwd);
//    m_pspi->print("\",");
//    m_pspi->print(chl);
//    m_pspi->print(",");
//    m_pspi->println(ecn);
//    
//    data = recvString("OK", "ERROR", 5000);
//    if (data.indexOf("OK") != -1) {
//        return true;
//    }
//    return false;
//}

//bool eATCWLIF(String &list)
//{
//    String data;
//    rx_empty();
//    m_pspi->println("AT+CWLIF");
//    return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", list);
//}

