/*----------------------------------------------------------------------------
 * Copyright (c) <2018>, <Huawei Technologies Co., Ltd>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Notice of Export Control Law
 * ===============================================
 * Huawei LiteOS may be subject to applicable export control laws and regulations, which might
 * include those applicable to Huawei LiteOS of U.S. and the country in which you are located.
 * Import, export and usage of Huawei LiteOS in any manner by you shall be in compliance with such
 * applicable export control laws and regulations.
 *---------------------------------------------------------------------------*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <link_log.h>
#include <queue.h>
#include <oc_mqtt_al.h>
#include <oc_mqtt_profile.h>
#include <iot_config.h>

#include <app_x-cube-mems1.h>

#define CN_SERVER_IPV4                 "121.36.42.100"
#define CN_SERVER_PORT                 "1883"
#define CN_SECURITY_TYPE               EN_DTLS_AL_SECURITY_TYPE_NONE

#define CN_EP_DEVICEID                 CONFIG_MQTT_DEVID
#define CN_EP_PASSWD                   CONFIG_MQTT_PASSWD

#define CN_BOOT_MODE                   0
#define CN_LIFE_TIME                   60
#define CN_OTA_FOTA_VERSION            "FOTAV1"
//if your command is very fast,please use a queue here--TODO
static queue_t                   *s_queue_rcvmsg = NULL;   ///< this is used to cached the message

static oc_mqtt_profile_service_t  s_device_service;
static oc_mqtt_profile_kv_t       property_temp;
static oc_mqtt_profile_kv_t       property_hum;
static oc_mqtt_profile_kv_t       property_press;
static oc_mqtt_profile_kv_t       property_x;
static oc_mqtt_profile_kv_t       property_y;
static oc_mqtt_profile_kv_t       property_z;

static volatile bool stop_report = false;

//use this function to push all the message to the buffer
static int app_msg_deal(oc_mqtt_profile_msgrcv_t *msg)
{
    int    ret = -1;
    char  *buf;
    int    buflen;
    int    datalen = 0;
    oc_mqtt_profile_msgrcv_t *demo_msg;

    ///< clone the message
    buflen = sizeof(oc_mqtt_profile_msgrcv_t) + msg->msg_len + 1;///< we copy with '\0' endings
    if(NULL != msg->request_id)
    {
        buflen += strlen(msg->request_id) +1; ///< ending with '\0'
    }

    buf = osal_malloc(buflen);
    if(NULL != buf)
    {
        demo_msg = (oc_mqtt_profile_msgrcv_t *)(uintptr_t)buf;
        buf += sizeof(oc_mqtt_profile_msgrcv_t);
        ///< copy the message and push it to the queue
        demo_msg->type = msg->type;

        if(NULL != msg->request_id)
        {
            demo_msg->request_id = buf;
            datalen = strlen(msg->request_id);
            (void) memcpy(buf,msg->request_id,datalen);
            buf[datalen] = '\0';
            buf += (datalen+1);
        }
        else
        {
            demo_msg->request_id = NULL;
        }

        demo_msg->msg = buf;
        demo_msg->msg_len = msg->msg_len;
        datalen = msg->msg_len;
        (void) memcpy(buf,msg->msg,datalen);
        buf[datalen] = '\0';

        LINK_LOG_DEBUG("RCVMSG:type:%d reuqestID:%s payloadlen:%d payload:%s\n\r",\
                (int) demo_msg->type,demo_msg->request_id==NULL?"NULL":demo_msg->request_id,\
                demo_msg->msg_len,(char *)demo_msg->msg);

        ret = queue_push(s_queue_rcvmsg,demo_msg,10);
        if(ret != 0)
        {
            osal_free(demo_msg);
        }
    }

    return ret;
}

#define CN_EVENT_SERVICES_ID         "services"
#define CN_EVENT_TYPE_INDEX          "event_type"
#define CN_EVENT_TYPE_VERSIONQUERY   "version_query"
#define CN_EVENT_TYPE_FIRMUPDATE     "firmware_upgrade"

#include <cJSON.h>

static int oc_cmd_event_versionquery(cJSON *event)
{
    int ret;
    char *topic = "$oc/devices/"CN_EP_DEVICEID"/sys/events/up";
    const char *fmt = "{ \"services\": [{ \"service_id\": \"$ota\", \"event_type\": \"version_report\", \"paras\": { \"sw_version\":\"v1.0\",\"fw_version\":\"%s\" } }]}";
    char *data;
    int   len;

    len = strlen(fmt) + strlen(CN_OTA_FOTA_VERSION) + 1;
    data = osal_malloc(len);

    if (data == NULL)
    {
        return -1;
    }

    snprintf(data, len, fmt, CN_OTA_FOTA_VERSION);
    LINK_LOG_DEBUG("REPLY:CURVERSION:%s",data);
    ret = oc_mqtt_publish(topic,(uint8_t *)data, strlen(data),0);
    osal_free(data);

    return ret;
}

#ifdef CONFIG_OTA_ENABLE
#include <ota_https.h>
#include <ota_flag.h>

//Topic: $oc/devices/{device_id}/sys/events/up
//数据格式：
//{
//    "object_device_id": "{object_device_id}",
//    "services": [{
//        "service_id": "$ota",
//        "event_type": "upgrade_progress_report",
//        "event_time": "20151212T121212Z",
//        "paras": {
//            "result_code": 0,
//            "progress": 80,
//            "version": "V2.0",
//            "description": "upgrade processing"
//        }
//    }]
//}

static int oc_report_upgraderet(int upgraderet, const char *version)
{
    int ret;
    char *topic = "$oc/devices/"CN_EP_DEVICEID"/sys/events/up";
    char *fmt = "{ \"services\": [{ \"service_id\": \"$ota\", \"event_type\": \"upgrade_progress_report\", \"paras\": { \"result_code\":%d,\"version\":\"%s\" } }]}";
    char *data;
    int   len;

    len = strlen(fmt) + strlen(version) + sizeof(upgraderet);
    data = osal_malloc(len);
    
    if (data == NULL)
    {
        return -1;
    }

    snprintf(data, len, fmt,upgraderet, version);
    LINK_LOG_DEBUG("REPORT:UPGRADERET:%s",data);
    ret = oc_mqtt_publish(topic,(uint8_t *)data, strlen(data),0);
    osal_free(data);

    return ret;
}


static int oc_cmd_event_firmupdate(cJSON *event)
{
    int ret = -1;
    cJSON *obj_paras;
    cJSON *obj_version;
    cJSON *obj_url;
    cJSON *obj_filesize;
    cJSON *obj_accesstoken;
    cJSON *obj_sign;

    ota_https_para_t *otapara;

    stop_report = true;

    otapara = osal_malloc(sizeof(ota_https_para_t));
    memset(otapara, 0, sizeof(ota_https_para_t));
    if(NULL == otapara)
    {
        return ret;
    }

    obj_paras = cJSON_GetObjectItem(event, "paras");
    if(NULL != obj_paras)
    {
        obj_version = cJSON_GetObjectItem(obj_paras, "version");
        obj_url = cJSON_GetObjectItem(obj_paras, "url");
        obj_filesize = cJSON_GetObjectItem(obj_paras, "file_size");
        obj_accesstoken = cJSON_GetObjectItem(obj_paras, "access_token");
        obj_sign = cJSON_GetObjectItem(obj_paras, "sign");

        if((NULL != obj_version) && (NULL != obj_url) && (NULL != obj_filesize) && \
                (NULL != obj_accesstoken) && (NULL != obj_sign))
        {
            otapara->authorize = cJSON_GetStringValue(obj_accesstoken);
            otapara->url = cJSON_GetStringValue(obj_url);
            otapara->signature = cJSON_GetStringValue(obj_sign);
            otapara->file_size = obj_filesize->valueint;
            otapara->version = cJSON_GetStringValue(obj_version);
            ///< here we do the firmware download
            if(0 != ota_https_download(otapara))
            {
                oc_report_upgraderet(6,otapara->version);
                LINK_LOG_ERROR("DOWNLOADING ERR");
            }
            else
            {
//                oc_report_upgraderet(0,otapara->version);
                LINK_LOG_DEBUG("DOWNLOADING SUCCESS");
                osal_task_sleep(500);
                osal_reboot();
            }
            ret = 0;
        }
    }

    osal_free(otapara);

    return ret;
}


static int oc_cmd_event(oc_mqtt_profile_msgrcv_t *msg)
{
    int ret = -1;
    cJSON *obj_root;
    cJSON *obj_servicearry;
    cJSON *obj_service;
    cJSON *obj_eventtype;

    obj_root = cJSON_Parse(msg->msg);
    if(NULL == obj_root)
    {
        goto EXIT_JSONFMT;
    }

    obj_servicearry = cJSON_GetObjectItem(obj_root,CN_EVENT_SERVICES_ID);
    if((NULL == obj_servicearry) || (!cJSON_IsArray(obj_servicearry)))
    {
        goto EXIT_JSONSERVICEARRY;
    }

    cJSON_ArrayForEach(obj_service,obj_servicearry)
    {
        obj_eventtype = cJSON_GetObjectItem(obj_service,CN_EVENT_TYPE_INDEX);
        if(NULL == obj_eventtype)
        {
            continue;
        }
        if(0 == strcmp(cJSON_GetStringValue(obj_eventtype),CN_EVENT_TYPE_VERSIONQUERY))
        {
            oc_cmd_event_versionquery(obj_service);
        }
        else if(0 == strcmp(cJSON_GetStringValue(obj_eventtype),CN_EVENT_TYPE_FIRMUPDATE))
        {
            oc_cmd_event_firmupdate(obj_service);
        }
    }
    cJSON_Delete(obj_root);
    return 0;

EXIT_JSONSERVICEARRY:
    cJSON_Delete(obj_root);
EXIT_JSONFMT:
    return ret;
}

///< now we deal the message here
static int  oc_cmd_normal(oc_mqtt_profile_msgrcv_t *demo_msg)
{
    static int value = 0;
    oc_mqtt_profile_cmdresp_t  cmdresp;
    oc_mqtt_profile_propertysetresp_t propertysetresp;
    oc_mqtt_profile_propertygetresp_t propertygetresp;

    switch(demo_msg->type)
    {
        case EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_MSGDOWN:
            ///< add your own deal here
            break;
        case EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_COMMANDS:
            ///< add your own deal here

            ///< do the response
            cmdresp.paras = NULL;
            cmdresp.request_id = demo_msg->request_id;
            cmdresp.ret_code = 0;
            cmdresp.ret_name = NULL;
            (void)oc_mqtt_profile_cmdresp(NULL,&cmdresp);
            break;

        case EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_PROPERTYSET:
            ///< add your own deal here

            ///< do the response
            propertysetresp.request_id = demo_msg->request_id;
            propertysetresp.ret_code = 0;
            propertysetresp.ret_description = NULL;
            (void)oc_mqtt_profile_propertysetresp(NULL,&propertysetresp);
            break;

        case  EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_PROPERTYGET:
            ///< add your own deal here

            ///< do the response
            value  = (value+1)%100;
            s_device_service.service_property->key = "radioValue";
            s_device_service.service_property->value = &value;
            s_device_service.service_property->type = EN_OC_MQTT_PROFILE_VALUE_INT;

            propertygetresp.request_id = demo_msg->request_id;
            propertygetresp.services = &s_device_service;
            (void)oc_mqtt_profile_propertygetresp(NULL,&propertygetresp);
            break;
        case EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_EVENT:
            oc_cmd_event(demo_msg);
            break;

        default:
            break;

    }
    return 0;
}

static int task_rcvmsg_entry( void *args)
{
    oc_mqtt_profile_msgrcv_t *demo_msg;
    while(1)
    {
        demo_msg = NULL;
        (void)queue_pop(s_queue_rcvmsg,(void **)&demo_msg,(int)cn_osal_timeout_forever);
        if(NULL != demo_msg)
        {
            (void) oc_cmd_normal(demo_msg);
            osal_free(demo_msg);
        }
    }
}
#endif

static int  oc_report_normal(void)
{
    int ret = en_oc_mqtt_err_ok;
    float temp, hum, press;
    static int s_asex_x, s_asex_y, s_asex_z;
    static char s_temp [8];
    static char s_hum [8];
    static char s_press [8];

    MX_MEMS_Getinfo(&temp, &hum, &press, &s_asex_x, &s_asex_y, &s_asex_z);

    snprintf(s_temp,  6, "%f", (double)temp);
    snprintf(s_hum,   6, "%f", (double)hum);
    snprintf(s_press, 8, "%f", (double)press);

    s_temp  [5] = '\0';
    s_hum   [5] = '\0';
    s_press [7] = '\0';

    property_temp.key = "temperature";
    property_temp.value = s_temp;
    property_temp.type = EN_OC_MQTT_PROFILE_VALUE_STRING;

    property_hum.key = "humidity";
    property_hum.value = s_hum;
    property_hum.type = EN_OC_MQTT_PROFILE_VALUE_STRING;

    property_press.key = "pressure";
    property_press.value = s_press;
    property_press.type = EN_OC_MQTT_PROFILE_VALUE_STRING;

    property_x.key = "accelerometer_x";
    property_x.value = &s_asex_x;
    property_x.type = EN_OC_MQTT_PROFILE_VALUE_INT;

    property_y.key = "accelerometer_y";
    property_y.value = &s_asex_y;
    property_y.type = EN_OC_MQTT_PROFILE_VALUE_INT;

    property_z.key = "accelerometer_z";
    property_z.value = &s_asex_z;
    property_z.type = EN_OC_MQTT_PROFILE_VALUE_INT;

    ret = oc_mqtt_profile_propertyreport(NULL,&s_device_service);

    printf("reported temperature = %s, humidity = %s, pressure = %s, accelerometer = (%d, %d, %d) %s\n\r",
           s_temp, s_hum, s_press, s_asex_x, s_asex_y, s_asex_z, ret == 0 ? "successful :-)" : "fail :-(");

    return ret;
}

static int task_reportmsg_entry(void *args)
{
    int ret;
    oc_mqtt_profile_connect_t  connect_para;

    memset( &connect_para, 0, sizeof(connect_para));

    connect_para.boostrap =      CN_BOOT_MODE;
    connect_para.device_id =     CN_EP_DEVICEID;
    connect_para.device_passwd = CN_EP_PASSWD;
    connect_para.server_addr =   CN_SERVER_IPV4;
    connect_para.sevver_port =   CN_SERVER_PORT;
    connect_para.life_time =     CN_LIFE_TIME;
    connect_para.rcvfunc =       app_msg_deal;

    connect_para.security.type = EN_DTLS_AL_SECURITY_TYPE_NONE;

    ret = oc_mqtt_profile_connect(&connect_para);
    if((ret != en_oc_mqtt_err_ok))
    {
        printf("config:err :code:%d\r\n",ret);
        return -1;
    }

#ifdef CONFIG_OTA_ENABLE
    static ota_flag_t otaflag = {0};

    if(0 == ota_flag_get(EN_OTA_TYPE_FOTA,&otaflag))
    {
        oc_report_upgraderet(otaflag.info.curstatus == EN_OTA_STATUS_UPGRADED_SUCCESS ? 0 : 1,
                             CN_OTA_FOTA_VERSION);
    }

    otaflag.info.curstatus = EN_OTA_STATUS_IDLE;
    ota_flag_save(EN_OTA_TYPE_FOTA,&otaflag);
#endif

    oc_cmd_event_versionquery((cJSON *)NULL);

    char *topic;
    topic = "user/demo_topic";
    ret = oc_mqtt_subscribe(topic, 0);
    printf("subscribe:topic:%d ret:%d",topic,ret);

    ret = oc_mqtt_unsubscribe(topic);
    printf("unsubscribe:topic:%d ret:%d",topic,ret);

    MX_MEMS_Init();

    while(1)  //do the loop here
    {
        if (!stop_report)
        {
            oc_report_normal();
        }

        osal_task_sleep(10*1000);
    }
}

int standard_app_demo_main(void)
{
    s_queue_rcvmsg = queue_create("queue_rcvmsg",2,1);

    property_temp.nxt  = &property_hum;
    property_hum.nxt   = &property_press;
    property_press.nxt = &property_x;
    property_x.nxt     = &property_y;
    property_y.nxt     = &property_z;
    property_z.nxt     = NULL;

    printf ("CN_OTA_FOTA_VERSION is %s\n\r", CN_OTA_FOTA_VERSION);

    ///< initialize the service
    s_device_service.event_time = NULL;
    s_device_service.service_id = "SensorService";
    s_device_service.service_property = &property_temp;
    s_device_service.nxt = NULL;

    (void) osal_task_create("demo_reportmsg",task_reportmsg_entry,NULL,0x800,NULL,8);
#ifdef CONFIG_OTA_ENABLE
    (void) osal_task_create("demo_rcvmsg",task_rcvmsg_entry,NULL,0x1800,NULL,8);
#endif

    return 0;
}

int liteos_reboot(void)
{
    extern void HAL_NVIC_SystemReset(void);
    HAL_NVIC_SystemReset();
    return 0;
}
