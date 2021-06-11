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

#include <stdio.h>
#include <string.h>

#include "py/objtuple.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "lib/netutils/netutils.h"
#include "modnetwork.h"

#if MICROPY_PY_UMQTT && !MICROPY_PY_LWIP

/******************************************************************************/
// mqtt class

STATIC const mp_obj_type_t MQTTClient_type;


STATIC void socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind){
    mod_network_socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<usocket.socket fd=%d, family=%d, type=%d>", self->fd, self->u_param.domain, self->u_param.type);
}



STATIC mp_obj_t mqtt_set_callback(mp_obj_t self_in,mqtt_callback callback_fn) {
	mqtt_obj_t *self = MP_OBJ_TO_PTR(self_in);
	if (self->nic == MP_OBJ_NULL) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
	if (callback_fn != NULL) {
       self->mqtt_callback_fn = callback_fn;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mqtt_set_callback_obj, mqtt_set_callback);

STATIC mp_obj_t mqtt_set_last_will(mp_obj_t self_in, mp_obj_t addr_in) {

}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mqtt_set_last_will_obj, mqtt_set_last_will);

STATIC void mqtt_select_nic(mqtt_obj_t *self, const byte *ip) {
    if (self->nic == MP_OBJ_NULL) {
        // select NIC based on IP
        self->nic = mod_network_find_nic(ip);
        self->nic_type = (mod_network_nic_type_t*)mp_obj_get_type(self->nic);		
        // call the NIC to open the mqtt
        int _errno;
        if (self->nic_type->mqtt(self, &_errno) != 0) {
            mp_raise_OSError(_errno);
        }
    }
}

	
STATIC mp_obj_t mqtt_connect(size_t n_args, const mp_obj_t *pos_args) {
	mqtt_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
	mqtt_select_nic(self, self->server);
	int reconnect = 0;
	if(n_args > 1)
	{
		reconnect = pos_args[1];
	}
	if (self->nic == MP_OBJ_NULL) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
	if (self->nic_type->mqtt_setcfg(self, self->client_id, self->user, self->password, self->keepalive, self->ssl, self->ssl_params) != 0) {
        mp_raise_ValueError("mqtt set error");
    }
    if (self->nic_type->mqtt_connect(self, self->server, self->port, reconnect) != 0) {
        mp_raise_ValueError("mqtt connect error");
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mqtt_connect_obj, 1, 2, mqtt_connect);

STATIC mp_obj_t mqtt_disconnect(mp_obj_t self_in, mp_obj_t addr_in) {
	mqtt_obj_t *self = MP_OBJ_TO_PTR(self_in);
	if (self->nic == MP_OBJ_NULL) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
	if (self->nic_type->mqtt_disconnect(self) != 0) {
            mp_raise_OSError(-1);
        }
	return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mqtt_disconnect_obj, mqtt_disconnect);

STATIC mp_obj_t mqtt_ping(mp_obj_t self_in, mp_obj_t addr_in) {

}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mqtt_ping_obj, mqtt_ping);

STATIC mp_obj_t mqtt_publish(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
	enum { ARG_topic, ARG_data, ARG_qos, ARG_retain};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_mqtt_topic, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mqtt_data, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mqtt_qos, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_mqtt_retain, MP_ARG_INT , {.u_int = -1} },
    };	
	mqtt_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
	
    size_t topic_len = 0;
    const char *topic = NULL;
    if (args[ARG_topic].u_obj != mp_const_none)
    {
        topic = mp_obj_str_get_data(args[ARG_topic].u_obj, &topic_len);
    }

    size_t data_len = 0;
    const char *data = NULL;
    if (args[ARG_data].u_obj != mp_const_none)
    {
        data = mp_obj_str_get_data(args[ARG_data].u_obj, &data_len);
    }

	int qos = 0;
	if (args[ARG_qos].u_int > 0) {
        qos = args[ARG_qos].u_int;
    }
	int retain = 0;
	if (args[ARG_retain].u_int > 0) {
        retain = args[ARG_retain].u_int;
    }
	if (self->nic == MP_OBJ_NULL) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }	
    if (self->nic_type->mqtt_publish(self, topic, data, qos, retain) != 0) {
        mp_raise_OSError(-1);
    }
    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mqtt_publish_obj, 2, mqtt_publish);

STATIC mp_obj_t mqtt_subscribe(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
	enum { ARG_topic,  ARG_qos};
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_mqtt_topic, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mqtt_qos, MP_ARG_INT, {.u_int = -1} },
    };
	mqtt_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    size_t topic_len = 0;
    const char *topic = NULL;
    if (args[ARG_topic].u_obj != mp_const_none)
    {
        topic = mp_obj_str_get_data(args[ARG_topic].u_obj, &topic_len);
    }
	int qos = 0;
	if (args[ARG_qos].u_int > 0) {
        qos = args[ARG_qos].u_int;
    }
	if (self->nic == MP_OBJ_NULL) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
    if (self->nic_type->mqtt_subscribe(self, topic, qos) != 0) {
        mp_raise_OSError(-1);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mqtt_subscribe_obj, 1, mqtt_subscribe);

STATIC mp_obj_t mqtt_wait_msg(mp_obj_t self_in) {
	mqtt_obj_t *self = MP_OBJ_TO_PTR(self_in);
	//mqtt_msg *mqttmsg = NULL;
	mqtt_msg mqttmsg;
	mqttmsg.topic = mp_const_none;
	mqttmsg.msg = mp_const_none;
	if (self->nic == MP_OBJ_NULL) {
        // not connected
        mp_raise_OSError(MP_ENOTCONN);
    }
	if (self->nic_type->mqtt_wait_msg(self, &mqttmsg) > 0) {
	/*********************************Test****************************************************
		mp_printf(MP_PYTHON_PRINTER, "test mqtt_wait_msg:1.%s 2.%s \n",
        mqttmsg.topic, mqttmsg.msg);
	*********************************End***************************************************/
        	mp_obj_t list = mp_obj_new_list(0, NULL);
			mp_obj_list_append(list, mqttmsg.topic);
			mp_obj_list_append(list, mqttmsg.msg);
			mp_sched_schedule(self->mqtt_callback_fn, list);
    }
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mqtt_wait_msg_obj, mqtt_wait_msg);

STATIC mp_obj_t mqtt_check_msg(mp_obj_t self_in, mp_obj_t addr_in) {

}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mqtt_check_msg_obj, mqtt_check_msg);

STATIC const mp_rom_map_elem_t mqtt_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },

    { MP_ROM_QSTR(MP_QSTR_set_callback), MP_ROM_PTR(&mqtt_set_callback_obj) },
    //{ MP_ROM_QSTR(MP_QSTR_set_last_will), MP_ROM_PTR(&mqtt_set_last_will_obj) },	
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&mqtt_connect_obj) },	
    { MP_ROM_QSTR(MP_QSTR_disconnect), MP_ROM_PTR(&mqtt_disconnect_obj) },	
    //{ MP_ROM_QSTR(MP_QSTR_ping), MP_ROM_PTR(&mqtt_ping_obj) },	
    { MP_ROM_QSTR(MP_QSTR_publish), MP_ROM_PTR(&mqtt_publish_obj) },	
    { MP_ROM_QSTR(MP_QSTR_subscribe), MP_ROM_PTR(&mqtt_subscribe_obj) },	
    { MP_ROM_QSTR(MP_QSTR_wait_msg), MP_ROM_PTR(&mqtt_wait_msg_obj) },	
    //{ MP_ROM_QSTR(MP_QSTR_check_msg), MP_ROM_PTR(&mqtt_check_msg_obj) },	
};
	
STATIC MP_DEFINE_CONST_DICT(mqtt_locals_dict, mqtt_locals_dict_table);

// constructor socket(family=AF_INET, type=SOCK_STREAM, proto=0, fileno=None)
STATIC mp_obj_t mqtt_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args){
	enum { ARG_client_id, ARG_server, ARG_port, ARG_user, ARG_password, ARG_keepalive, ARG_ssl, ARG_ssl_params };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_mqtt_client_id, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mqtt_server, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mqtt_port, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_mqtt_user, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_mqtt_password, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_mqtt_keepalive, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_mqtt_ssl, MP_ARG_INT , {.u_int = -1} },
		{ MP_QSTR_mqtt_ssl_params, MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };
    // Parse args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
	
	mqtt_obj_t *self = m_new_obj_with_finaliser(mqtt_obj_t);
	self->base.type = &MQTTClient_type;
    self->nic = MP_OBJ_NULL;
    self->nic_type = NULL;
		
	size_t client_id_len =0;
    if (args[ARG_client_id].u_obj != MP_OBJ_NULL) {
        self->client_id = mp_obj_str_get_data(args[ARG_client_id].u_obj, &client_id_len);
    }
	size_t server_len =0;
    if (args[ARG_server].u_obj != MP_OBJ_NULL) {
        self->server = mp_obj_str_get_data(args[ARG_server].u_obj, &server_len);
    }
	
	if (args[ARG_port].u_int > 0) {
        self->port = args[ARG_port].u_int;
    }
	else{
		self->port = 0;
	}
	size_t user_len =0;
    if (args[ARG_user].u_obj != mp_const_none) {
        self->user = mp_obj_str_get_data(args[ARG_user].u_obj, &user_len);
    }
	else{
		self->user = NULL;
	}
	size_t password_len =0;
    if (args[ARG_password].u_obj != mp_const_none) {
        self->password = mp_obj_str_get_data(args[ARG_password].u_obj, &password_len);
    }
	else{
		self->password = NULL;
	}
	if (args[ARG_keepalive].u_int > 0) {
        self->keepalive = args[ARG_keepalive].u_int;
    }
	else{
		self->keepalive = 0;
	}
	if (args[ARG_ssl].u_int > 0) {
        self->ssl = args[ARG_ssl].u_int;
    }
	else{
		self->ssl = 0;
	}
	size_t ssl_params_len =0;
    if (args[ARG_ssl_params].u_obj != mp_const_none) {
        self->ssl_params = mp_obj_str_get_data(args[ARG_ssl_params].u_obj, &ssl_params_len);
    }
	else{
		self->ssl_params = NULL;
	}
	return MP_OBJ_FROM_PTR(self);
}


STATIC const mp_obj_type_t MQTTClient_type = {
	{ &mp_type_type },
	.name = MP_QSTR_mqtt,
	.make_new = mqtt_make_new,
    .print = socket_print,
	.locals_dict = (mp_obj_dict_t*)&mqtt_locals_dict,
};


STATIC const mp_rom_map_elem_t mp_module_umqtt_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_umqtt) },
    { MP_ROM_QSTR(MP_QSTR_MQTTClient), MP_ROM_PTR(&MQTTClient_type) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_umqtt_globals, mp_module_umqtt_globals_table);

const mp_obj_module_t mp_module_umqtt = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_umqtt_globals,
};
	
#endif // MICROPY_PY_UMQTT && !MICROPY_PY_LWIP
