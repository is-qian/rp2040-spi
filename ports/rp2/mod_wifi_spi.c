/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include "py/objtuple.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "lib/netutils/netutils.h"
#include "modnetwork.h"
#include "modmachine.h"
#include "wifi_spi.h"
#include "mpconfigboard.h"
#include "buffer.h"
typedef struct _machine_spi1_obj_t {
    int id;
    int flag_table;
    int flag;
} machine_spi1_obj_t;
STATIC bool nic_connected = false;
typedef struct _nic_spi_obj_t
{
    mp_obj_base_t base;
#if MICROPY_SPI_NIC
    mp_obj_t spi_obj;
    esp8285_obj esp8285;
#endif
    mp_obj_t mode;
} nic_spi_obj_t;
typedef struct _stream_obj_t
{
    unsigned char stream[2048];
    int stream_cur;
    int stream_len;
} stream_obj_t;

stream_obj_t stream;

STATIC void esp8285_socket_close(mod_network_socket_obj_t *socket)
{
    if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
    {
        return;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(socket->nic);
    releaseTCP(&self->esp8285);
}

STATIC mp_uint_t esp8285_socket_recv(mod_network_socket_obj_t *socket, byte *buf, mp_uint_t len, int *_errno)
{
    if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
    {
        *_errno = MP_EPIPE;
        return MP_STREAM_ERROR;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(socket->nic);
    int ret = 0;
    uint32_t read_len = 0;
    ret = esp_recv(&self->esp8285, (char *)buf, len, &read_len, (uint32_t)(socket->timeout * 1000), &socket->peer_closed, socket->first_read_after_write);
    socket->first_read_after_write = false;
    if (ret == -1)
    {
        *_errno = MP_EPIPE;
        return MP_STREAM_ERROR;
    }
    else if (ret == -2) // EOF
    {
        *_errno = MP_EAGAIN; // MP_EAGAIN or MP_EWOULDBLOCK according to `mp_is_nonblocking_error()`
        return MP_STREAM_ERROR;
    }
    else if (ret == -3) // timeout
    {
        *_errno = MP_ETIMEDOUT;
        return MP_STREAM_ERROR;
    }
    else if (ret == -4) //peer closed
    {
        *_errno = MP_ENOTCONN;
        return MP_STREAM_ERROR;
    }
    return (mp_uint_t)read_len;
}

STATIC mp_uint_t esp8285_socket_send(mod_network_socket_obj_t *socket, const byte *buf, mp_uint_t len, int *_errno)
{

    if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
    {
        *_errno = MP_EPIPE;
        return MP_STREAM_ERROR;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(socket->nic);
    if (socket->peer_closed)
    {
        *_errno = MP_ENOTCONN;
        return MP_STREAM_ERROR;
    }
    Buffer_Clear(&self->esp8285.buffer); //clear receive buffer
    socket->first_read_after_write = true;
    if (0 == esp_send(&self->esp8285, (const char *)buf, len, (uint32_t)(socket->timeout * 1000)))
    {
        *_errno = MP_EPIPE;
        return MP_STREAM_ERROR;
    }
    return len;
}

STATIC int esp8285_socket_socket(mod_network_socket_obj_t *socket, int *_errno)
{

    return 0;
}

STATIC int esp8285_socket_connect(mod_network_socket_obj_t *socket, byte *ip, mp_uint_t port, int *_errno)
{
    if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(socket->nic)))
    {
        *_errno = -1;
        return -1;
    }
    socket->peer_closed = false;
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(socket->nic);
    switch (socket->u_param.type)
    {
    case MOD_NETWORK_SOCK_STREAM:
    {
        if (false == createTCP(&self->esp8285, (char *)ip, port))
        {
            *_errno = -1;
            return -1;
        }
        break;
    }
    case MOD_NETWORK_SOCK_DGRAM:
    {
        if (false == registerUDP(&self->esp8285, (char *)ip, port))
        {
            *_errno = -1;
            return -1;
        }
        break;
    }
    default:
    {
        if (false == createTCP(&self->esp8285, (char *)ip, port))
        {
            *_errno = -1;
            return -1;
        }
        break;
    }
    }
    return 0;
}

STATIC int esp8285_socket_gethostbyname(mp_obj_t nic, const char *name, mp_uint_t len, uint8_t *out_ip)
{
    if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 == mp_obj_get_type(nic))
    {
        if (get_host_byname(&((nic_spi_obj_t *)nic)->esp8285, name, len, (char *)out_ip, 3000))
            return 0;
        else
            return MP_EINVAL;
    }
    return MP_EPERM;
}

STATIC int esp8285_mqtt(mqtt_obj_t *mqtt, int *_errno)
{
    return 0;
}
STATIC int esp8285_mqtt_setcfg(mqtt_obj_t *mqtt, const char* client_id, const char* username, const char* password, int cert_key_ID, int CA_ID, const char* path)
{

	if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(mqtt->nic)))
    {
        return -1;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(mqtt->nic);
    if (false == sMQTTUSERCFG(&self->esp8285, 0, 1, (char *)client_id, (char *)username, (char *)password,  cert_key_ID,  CA_ID, (char *)path))
    {
        return -1;
    }  
    return 0;
}

STATIC int esp8285_mqtt_set_last_will(mqtt_obj_t *mqtt, const char *name, mp_uint_t len, uint8_t *out_ip)
{

}

STATIC int esp8285_mqtt_connect(mqtt_obj_t *mqtt, const char *server, mp_uint_t port, uint8_t reconnect)
{
	if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(mqtt->nic)))
    {
        return -1;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(mqtt->nic);
    if (false == sMQTTCONN(&self->esp8285, 0, (char *)server, port, reconnect))
    {
        return -1;
    }  
    return 0;
}

STATIC int esp8285_mqtt_disconnect(mqtt_obj_t *mqtt)
{
	if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(mqtt->nic)))
    {
        return -1;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(mqtt->nic);
    if (false == sMQTTCLEAN(&self->esp8285, 0))
    {
        return -1;
    }  
    return 0;
}

STATIC int esp8285_mqtt_ping(mqtt_obj_t *mqtt, const char *name, mp_uint_t len, uint8_t *out_ip)
{

}

STATIC int esp8285_mqtt_publish(mqtt_obj_t *mqtt, const char *topic, const char *data, uint8_t qos, uint8_t retain)
{
	if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(mqtt->nic)))
    {
        return -1;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(mqtt->nic);
    if (false == sMQTTPUB(&self->esp8285, 0, (char *)topic, (char *)data, qos, retain))
    {
        return -1;
    }  
    return 0;
}

STATIC int esp8285_mqtt_subscribe(mqtt_obj_t *mqtt, const char *topic, uint8_t qos)
{
	if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(mqtt->nic)))
    {
        return -1;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(mqtt->nic);
    if (false == qMQTTSUB(&self->esp8285, 0, (char *)topic, qos))
    {
        return 0;
    }  
    return 0;
}

STATIC int esp8285_mqtt_wait_msg(mqtt_obj_t *mqtt, mqtt_msg* mqttmsg)
{	
	if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 != mp_obj_get_type(MP_OBJ_TO_PTR(mqtt->nic)))
    {
        return -1;
    }
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(mqtt->nic);
    if (false == get_mqttsubrecv(&self->esp8285, 0, mqttmsg)) 
    {
        return 0;
    } 
    return 1;
}

STATIC int esp8285_mqtt_check_msg(mqtt_obj_t *mqtt, const char *name, mp_uint_t len, uint8_t *out_ip)
{

}

STATIC mp_obj_t esp8285_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args)
{

#if MICROPY_SPI_NIC
    int idx = 0;
    idx = mp_obj_get_int(args[0]);
	machine_spi1_obj_t spi_wifi = {730, 3};
    //machine_spi1_obj_t spi_wifi = { 3, 3362, 4000001, 5346, 1, 5306, 1, 3378, 17, 4066, 3, 4970, 268675108, 4898, 268675084, 5834, 268675100};
    mp_obj_t wifi_spi = machine_spi_type.make_new((mp_obj_type_t *)&machine_spi_type, 0, 1, &spi_wifi);
    if (&machine_spi_type != mp_obj_get_type(wifi_spi))
    {
        mp_raise_ValueError("invalid uart stream");
    }
    //mp_get_stream_raise(wifi_spi, MP_STREAM_OP_READ | MP_STREAM_OP_WRITE | MP_STREAM_OP_IOCTL);
    nic_spi_obj_t *nic_obj = m_new_obj(nic_spi_obj_t);
    uint8_t *buff = m_new(uint8_t, ESP8285_BUF_SIZE);
    Buffer_Init(&nic_obj->esp8285.buffer, buff, ESP8285_BUF_SIZE);
    nic_obj->base.type = (mp_obj_type_t *)&mod_network_nic_type_esp8285;
    nic_obj->esp8285.spi_obj = wifi_spi;
    nic_obj->spi_obj = wifi_spi;
    if (idx < 0 || idx > 2)
    {
        mp_raise_ValueError(NULL);
    }
    nic_obj->mode = idx;
    return (mp_obj_t)nic_obj;
#endif
}
STATIC mp_obj_t esp8285_active(size_t n_args, const mp_obj_t *args)
{
    static uint32_t mode = 0;
    nic_spi_obj_t *self = MP_OBJ_TO_PTR(args[0]);
	if(0)
	{
		if (!qATCWMODE(&self->esp8285, &mode))
		{
			nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't init nic esp8285 ,try again \n"));
		}
	}
    if (n_args > 1)
    {
        int mask = self->mode == 1 ? STATION_MODE : SOFTAP_MODE;
        if (mp_obj_get_int(args[1]) != 0)
        {
            mode |= mask;
        }
        else
        {
            mode &= ~mask;
        }
        if (mode != 0)
        {	
			//esp8285 power on
			mp_hal_pin_output(21);
			mp_hal_pin_output(22);
			mp_hal_pin_output(24);
			mp_hal_pin_output(25);
			mp_hal_pin_output(SPI_CS);
			gpio_put(21,1);
			gpio_put(24,1);
			gpio_put(22,1);
			gpio_put(SPI_CS,0);
			gpio_put(25,0);
			mp_hal_pin_input(SPI_HANDSHARK);
			gpio_put(SPI_CS,1);
            if (0 == eINIT(&self->esp8285, mode))
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't init nic esp8285 ,try again please\n"));
            }
            mod_network_register_nic((mp_obj_t)self);
        }
        if (mode == 0)
        {
			// esp8285 power down
			mp_hal_pin_output(25);
			mp_hal_pin_output(21);
			mp_hal_pin_output(22);
			mp_hal_pin_output(24);
			gpio_put(25,1);
			gpio_put(21,0);
			gpio_put(24,0);
			gpio_put(22,0);
			mp_printf(MP_PYTHON_PRINTER, "esp8285 power off\n");
        }
        return mp_const_none;
    }
    if (self->mode == 1)
    {
        return mp_obj_new_bool(mode & STATION_MODE);
    }
    else
    {
        return mp_obj_new_bool(mode & SOFTAP_MODE);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp8285_active_obj, 1, 2, esp8285_active);

STATIC mp_obj_t esp8285_config(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs)
{
    if (n_args != 1 && kwargs->used != 0)
    {
        mp_raise_TypeError(MP_ERROR_TEXT("either pos or kw args are allowed"));
    }

    nic_spi_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    union
    {
        station_config sta;
        softap_config ap;
    } cfg;

    if (self->mode == STATION_MODE)
    {
        // if (false == wifi_station_get_config(&self->esp8285, &cfg.sta))
        // {
        //     mp_raise_TypeError(MP_ERROR_TEXT("can't get STA config"));
        // }
    }
    else
    {
        if (false == wifi_softap_get_config(&self->esp8285, &cfg.ap))
        {
            mp_raise_TypeError(MP_ERROR_TEXT("can't get AP config"));
        }
    }
    int req_if = -1;

    if (kwargs->used != 0)
    {

        for (mp_uint_t i = 0; i < kwargs->alloc; i++)
        {
            if (mp_map_slot_is_filled(kwargs, i))
            {
#define QS(x) (uintptr_t) MP_OBJ_NEW_QSTR(x)
                switch ((uintptr_t)kwargs->table[i].key)
                {
                case QS(MP_QSTR_mac):
                {
                    mp_buffer_info_t bufinfo;
                    mp_get_buffer_raise(kwargs->table[i].value, &bufinfo, MP_BUFFER_READ);
                    if (bufinfo.len != 17)
                    {
                        mp_raise_ValueError(MP_ERROR_TEXT("invalid buffer length"));
                    }
                    if (self->mode == STATION_MODE)
                    {
                        if (false == sCIPSTAMAC(&self->esp8285, bufinfo.buf))
                        {
                            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't set MAC ,try again \n"));
                        }
                    }
                    if (self->mode == SOFTAP_MODE)
                    {
                        if (false == sCIPAPMAC(&self->esp8285, bufinfo.buf))
                        {
                            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't set MAC ,try again please\n"));
                        }
                    }
                    break;
                }
                case QS(MP_QSTR_essid):
                {
                    req_if = SOFTAP_MODE;
                    size_t len;
                    const char *s = mp_obj_str_get_data(kwargs->table[i].value, &len);
                    len = MIN(len, sizeof(cfg.ap.ssid));
                    memcpy(cfg.ap.ssid, s, len);
                    cfg.ap.ssid[len] = NULL;
                    cfg.ap.ssid_len = len;
                    break;
                }
                case QS(MP_QSTR_hidden):
                {
                    req_if = SOFTAP_MODE;
                    cfg.ap.ssid_hidden = mp_obj_get_int(kwargs->table[i].value);
                    break;
                }
                case QS(MP_QSTR_authmode):
                {
                    req_if = SOFTAP_MODE;
                    cfg.ap.authmode = mp_obj_get_int(kwargs->table[i].value);
                    break;
                }
                case QS(MP_QSTR_password):
                {
                    req_if = SOFTAP_MODE;
                    size_t len;
                    const char *s = mp_obj_str_get_data(kwargs->table[i].value, &len);
                    len = MIN(len, sizeof(cfg.ap.password) - 1);
                    memcpy(cfg.ap.password, s, len);
                    cfg.ap.password[len] = NULL;
                    break;
                }
                case QS(MP_QSTR_channel):
                {
                    req_if = SOFTAP_MODE;
                    cfg.ap.channel = mp_obj_get_int(kwargs->table[i].value);
                    break;
                }
                case QS(MP_QSTR_hostname):
                {
                    req_if = STATION_MODE;
                    if (self->mode == STATION_MODE)
                    {
                        const char *s = mp_obj_str_get_str(kwargs->table[i].value);
                        if (false == sCWHOSTNAME(&self->esp8285, s))
                        {
                            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't set hostname ,try again please\n"));
                        }
                    }
                    break;
                }
                default:
                    goto unknown;
                }
#undef QS
            }
        }
        if (req_if >= 0)
        {
            self->mode = req_if;
            setOprToStation(&self->esp8285, req_if);
        }

        if (self->mode == STATION_MODE)
        {
            // if (false == wifi_station_set_config(&self->esp8285,&cfg.ap)
            // {
            //     mp_raise_TypeError(MP_ERROR_TEXT("can't set AP config"));
            // }
        }
        else
        {
            if (false == wifi_softap_set_config(&self->esp8285, &cfg.ap))
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "can't set AP config"));
            }
        }

        return mp_const_none;
    }

    // Get config

    if (n_args != 2)
    {
        mp_raise_TypeError(MP_ERROR_TEXT("can query only one param"));
    }

    mp_obj_t val;

    qstr key = mp_obj_str_get_qstr(args[1]);
    switch (key)
    {
    case MP_QSTR_mac:
    {
        char mac[17];
        if (self->mode == STATION_MODE)
        {
            if (false == qCIPSTAMAC(&self->esp8285, mac))
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't read MAC ,try again \n"));
            }
        }
        if (self->mode == SOFTAP_MODE)
        {
            if (false == qCIPAPMAC(&self->esp8285, mac))
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't read MAC ,try again please\n"));
            }
        }
        return mp_obj_new_str(mac, sizeof(mac));
    }
    case MP_QSTR_essid:
        if (self->mode == SOFTAP_MODE)
        {
            val = mp_obj_new_str((char *)cfg.ap.ssid, strlen((char *)cfg.ap.ssid));
        }
        // else {
        //     val = mp_obj_new_str((char *)cfg.sta.ssid, cfg.sta.ssid_len);
        // }
        break;
    case MP_QSTR_hidden:
        req_if = SOFTAP_MODE;
        val = MP_OBJ_NEW_SMALL_INT(cfg.ap.ssid_hidden);
        break;
    case MP_QSTR_authmode:
        req_if = SOFTAP_MODE;
        val = MP_OBJ_NEW_SMALL_INT(cfg.ap.authmode);
        break;
    case MP_QSTR_channel:
        req_if = SOFTAP_MODE;
        val = MP_OBJ_NEW_SMALL_INT(cfg.ap.channel);
        break;
    case MP_QSTR_hostname:
    {
        req_if = STATION_MODE;
        char s[64];
        if (false == qCWHOSTNAME(&self->esp8285, s))
        {
            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "couldn't read hostname ,try again please\n"));
        }
        if (s == NULL)
        {
            val = MP_OBJ_NEW_QSTR(MP_QSTR_);
        }
        else
        {
            val = mp_obj_new_str(s, strlen(s));
        }
        break;
    }
    default:
        goto unknown;
    }

    // // We post-check interface requirements to save on code size
    if (req_if >= 0)
    {
        self->mode = req_if;
        setOprToStation(&self->esp8285, req_if);
    }

    return val;

unknown:
    mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp8285_config_obj, 1, esp8285_config);

STATIC mp_obj_t esp8285_status(mp_obj_t self_in)
{   
    mp_obj_t list = mp_obj_new_list(0, NULL);
    // should we check return value?
    char *key = NULL;
    nic_spi_obj_t *self = self_in;
    key = getIPStatus(&self->esp8285);
    
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp8285_status_obj, esp8285_status);

STATIC mp_obj_t esp8285_nic_connect(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args)
{
    enum
    {
        ARG_ssid,
        ARG_key
    };
    nic_spi_obj_t *self = NULL;
    static const mp_arg_t allowed_args[] = {
        {MP_QSTR_ssid, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL}},
        {MP_QSTR_key, MP_ARG_OBJ, {.u_obj = mp_const_none}},
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    //get nic
    if ((mp_obj_type_t *)&mod_network_nic_type_esp8285 == mp_obj_get_type(pos_args[0]))
    {
        self = pos_args[0];
    }
    // get ssid
    size_t ssid_len = 0;
    const char *ssid = NULL;
    if (args[ARG_key].u_obj != mp_const_none)
    {
        ssid = mp_obj_str_get_data(args[ARG_ssid].u_obj, &ssid_len);
    }
    // get key
    size_t key_len = 0;
    const char *key = NULL;
    if (args[ARG_key].u_obj != mp_const_none)
    {
        key = mp_obj_str_get_data(args[1].u_obj, &key_len);
    }
    // connect to AP

    if (0 == joinAP(&self->esp8285, ssid, key))
    {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "could not connect to ssid=%s\n", ssid));
    }
    nic_connected = 1;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(esp8285_nic_connect_obj, 1, esp8285_nic_connect);

STATIC mp_obj_t esp8285_nic_disconnect(mp_obj_t self_in)
{
    // should we check return value?
    nic_spi_obj_t *self = self_in;
    if (false == leaveAP(&self->esp8285))
    {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, "conldn't disconnect wifi,plase try again or reboot nic\n"));
    }
    nic_connected = 0;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp8285_nic_disconnect_obj, esp8285_nic_disconnect);

STATIC mp_obj_t esp8285_nic_isconnected(mp_obj_t self_in)
{
    return mp_obj_new_bool(nic_connected);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp8285_nic_isconnected_obj, esp8285_nic_isconnected);

STATIC mp_obj_t esp8285_nic_reset(mp_obj_t self_in)
{
    nic_spi_obj_t *self = self_in;
    return mp_obj_new_bool(reset(&self->esp8285));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp8285_nic_reset_obj, esp8285_nic_reset);

STATIC mp_obj_t esp8285_nic_ifconfig(mp_obj_t self_in)
{
    nic_spi_obj_t *self = self_in;
    ipconfig_obj esp_ipconfig;
    esp_ipconfig.gateway = mp_const_none;
    esp_ipconfig.ip = mp_const_none;
    esp_ipconfig.MAC = mp_const_none;
    esp_ipconfig.netmask = mp_const_none;
    esp_ipconfig.ssid = mp_const_none;
    if (false == get_ipconfig(&self->esp8285, &esp_ipconfig))
    {
        return mp_const_none;
    }
    mp_obj_t tuple[5] = {esp_ipconfig.ip,
                         esp_ipconfig.netmask,
                         esp_ipconfig.gateway,
                         esp_ipconfig.MAC,
                         esp_ipconfig.ssid};
    return mp_obj_new_tuple(MP_ARRAY_SIZE(tuple), tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp8285_nic_ifconfig_obj, esp8285_nic_ifconfig);

STATIC mp_obj_t esp8285_scan_wifi(mp_obj_t self_in)
{
    nic_spi_obj_t *self = self_in;
    mp_obj_t list = mp_obj_new_list(0, NULL);
    char fail_str[30];
    char *buf = (char *)self->esp8285.buffer.buffer;
    bool end = 0;
    int err_code = 0;

    if (!eATCWLAP_Start(&self->esp8285))
    {
        err_code = -1;
        goto err;
    }
	if (!eATCWLAP_Get(&self->esp8285, &end))
    {
        err_code = -2;
        goto err;
    }
	char *index1 = buf;
    while (1)
    {
/* 		if (!end && index1 - buf > 4000){
			if (!eATCWLAP_Get(&self->esp8285, &end))
			{
				err_code = -2;
				goto err;
			}
		} */
        index1 = strstr(index1, "(");
        if (!index1)
        {
            err_code = -3;
            goto err;
        }
        char *index2 = strstr(index1, ")");
        if (!index2)
        {
            err_code = -4;
            goto err;
        }
        *index2 = '\0';
        index1 += 1;
        mp_obj_list_append(list, mp_obj_new_str(index1, strlen(index1)));
		index1 = index2 + 1;
		if(end){
			index2 = strstr(index1, "\r\nOK");
			if(index2 - index1 < 5)
			{	
				break;
			}
		}
    }
    return list;
err:
    snprintf(fail_str, sizeof(fail_str), "wifi scan fail:%d", err_code);
	nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_OSError, fail_str));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp8285_scan_wifi_obj, esp8285_scan_wifi); 

STATIC const mp_rom_map_elem_t esp8285_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&esp8285_nic_connect_obj)},
    {MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&esp8285_nic_disconnect_obj)},
    {MP_ROM_QSTR(MP_QSTR_isconnected), MP_ROM_PTR(&esp8285_nic_isconnected_obj)},
    {MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&esp8285_nic_reset_obj)},
    {MP_ROM_QSTR(MP_QSTR_ifconfig), MP_ROM_PTR(&esp8285_nic_ifconfig_obj)},
    {MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&esp8285_scan_wifi_obj)},
    {MP_ROM_QSTR(MP_QSTR_config), MP_ROM_PTR(&esp8285_config_obj)}, 
    {MP_ROM_QSTR(MP_QSTR_active), MP_ROM_PTR(&esp8285_active_obj)},
    {MP_ROM_QSTR(MP_QSTR_status), MP_ROM_PTR(&esp8285_status_obj)},
};

STATIC MP_DEFINE_CONST_DICT(esp8285_locals_dict, esp8285_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_esp8285 = {
    .base = {
        {&mp_type_type},
        .name = MP_QSTR_esp8285,
        .make_new = esp8285_make_new,
        .locals_dict = (mp_obj_dict_t *)&esp8285_locals_dict,
    },

    .gethostbyname = esp8285_socket_gethostbyname,
    .connect = esp8285_socket_connect,
    .socket = esp8285_socket_socket,
    .send = esp8285_socket_send,
    .recv = esp8285_socket_recv,
    .close = esp8285_socket_close,
	.mqtt = esp8285_mqtt,
	.mqtt_setcfg = esp8285_mqtt_setcfg,
	.mqtt_set_last_will = esp8285_mqtt_set_last_will,
	.mqtt_connect = esp8285_mqtt_connect,
	.mqtt_disconnect = esp8285_mqtt_disconnect,
	.mqtt_ping = esp8285_mqtt_ping,
	.mqtt_publish = esp8285_mqtt_publish,
	.mqtt_subscribe = esp8285_mqtt_subscribe,
	.mqtt_wait_msg = esp8285_mqtt_wait_msg,
	.mqtt_check_msg = esp8285_mqtt_check_msg,
/*  
    .bind = cc3k_socket_bind,
    .listen = cc3k_socket_listen,
    .accept = cc3k_socket_accept,
    .sendto = cc3k_socket_sendto,
    .recvfrom = cc3k_socket_recvfrom,
    .setsockopt = cc3k_socket_setsockopt,
    .settimeout = cc3k_socket_settimeout,
    .ioctl = cc3k_socket_ioctl,
*/
};
