#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json_parser.h"

#include "iot_import.h"
#include "iot_export.h"

#define PRODUCT_KEY             "a19kxqwXWu7"
#define DEVICE_NAME             "s7zMqOjD2yA1GcqckXXv"
#define DEVICE_SECRET           "07G44K9Xia01rqNmZh3lVx4UbgX2EKVz"

#define TOPIC_PROPERTY_SET            "/sys/"PRODUCT_KEY"/"DEVICE_NAME"/thing/service/property/set"
#define TOPIC_PROPERTY_POST           "/sys/"PRODUCT_KEY"/"DEVICE_NAME"/thing/event/property/post"
#define TOPIC_PROPERTY_POST_REPLY     "/sys/"PRODUCT_KEY"/"DEVICE_NAME"/thing/event/property/post_reply"
#define TOPIC_DATA                    "/sys/"PRODUCT_KEY"/"DEVICE_NAME"/data"

#define MQTT_MSGLEN             (1024)

#define DBG_TRACE(fmt, args...)  \
    do { \
        HAL_Printf("%s|%03d :: ", __func__, __LINE__); \
        HAL_Printf(fmt, ##args); \
        HAL_Printf("%s", "\r\n"); \
    } while(0)

static int switchValue;

void property_set_handler(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;
    DBG_TRACE("topic message arrived: topic=%.*s, topic_msg=%.*s",
        topic_info->topic_len,
        topic_info->ptopic,
        topic_info->payload_len,
        topic_info->payload);

    char * pValue = LITE_json_value_of("params.LightSwitch", (char *)topic_info->payload);
    switchValue = atoi(pValue);
    DBG_TRACE("get value from property set: %d", switchValue);
}

void default_event_handler(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;
    DBG_TRACE("event type: %d", msg->event_type);

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_UNDEF:
            DBG_TRACE("undefined event occur.");
            break;

        case IOTX_MQTT_EVENT_DISCONNECT:
            DBG_TRACE("MQTT disconnect.");
            break;

        case IOTX_MQTT_EVENT_RECONNECT:
            DBG_TRACE("MQTT reconnect.");
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
            DBG_TRACE("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
            DBG_TRACE("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
            DBG_TRACE("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            DBG_TRACE("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            DBG_TRACE("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
            DBG_TRACE("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
            DBG_TRACE("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
            DBG_TRACE("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_NACK:
            DBG_TRACE("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_RECVEIVED:
            DBG_TRACE("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
            break;

        default:
            DBG_TRACE("Should NOT arrive here.");
            break;
    }
}

int mqtt_client_loop(iotx_mqtt_param_t* mqtt_params)
{
    void *pclient;
    int rc = 0, msg_len;
    char msg_pub[128];

    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(mqtt_params);
    if (NULL == pclient) {
        DBG_TRACE("MQTT construct failed");
        return -1;
    }

    /* Subscribe the specific topic */
    rc = IOT_MQTT_Subscribe(pclient, TOPIC_PROPERTY_SET, IOTX_MQTT_QOS1, property_set_handler, NULL);
    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        DBG_TRACE("IOT_MQTT_Subscribe() failed, rc = %d", rc);
        return -1;
    }

    HAL_SleepMs(1000);

    iotx_mqtt_topic_info_t topic_msg;
    /* Initialize topic information */
    memset(&topic_msg, 0x0, sizeof(iotx_mqtt_topic_info_t));
    strcpy(msg_pub, "message: hello! start!");

    topic_msg.qos = IOTX_MQTT_QOS1;
    topic_msg.retain = 0;
    topic_msg.dup = 0;
    topic_msg.payload = (void *)msg_pub;
    topic_msg.payload_len = strlen(msg_pub);

    rc = IOT_MQTT_Publish(pclient, TOPIC_PROPERTY_POST, &topic_msg);
    DBG_TRACE("rc = IOT_MQTT_Publish() = %d", rc);

    do {
        /* Generate topic message */
        msg_len = snprintf(msg_pub,
            sizeof(msg_pub),
            "{\"method\":\"thing.event.property.post\", \"params\":{\"LightSwitch\":%d}}",
            switchValue);

        if (msg_len < 0) {
            DBG_TRACE("Error occur! Exit program");
            rc = -1;
            break;
        }

        topic_msg.payload = (void *)msg_pub;
        topic_msg.payload_len = msg_len;

        rc = IOT_MQTT_Publish(pclient, TOPIC_PROPERTY_POST, &topic_msg);
        if (rc < 0) {
            DBG_TRACE("error occur when publish");
            rc = -1;
            break;
        }
        DBG_TRACE("packet-id=%u, publish topic msg=%s", (uint32_t)rc, msg_pub);

        /* handle the MQTT packet received from TCP or SSL connection */
        IOT_MQTT_Yield(pclient, 200);
        HAL_SleepMs(2000);

    } while (1);

    IOT_MQTT_Unsubscribe(pclient, TOPIC_PROPERTY_POST);
    HAL_SleepMs(200);
    IOT_MQTT_Destroy(&pclient);

    return rc;
}

int mqtt_connect(void)
{
    int rc = 0;
    iotx_conn_info_pt pconn_info;
    char *msg_buf = NULL, *msg_readbuf = NULL;

    if (NULL == (msg_buf = (char *)HAL_Malloc(MQTT_MSGLEN))) {
        DBG_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(MQTT_MSGLEN))) {
        DBG_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, (void **)&pconn_info)) {
        DBG_TRACE("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }

    iotx_mqtt_param_t mqtt_params;
    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 2000;
    mqtt_params.clean_session = 0;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MQTT_MSGLEN;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MQTT_MSGLEN;

    mqtt_params.handle_event.h_fp = default_event_handler;
    mqtt_params.handle_event.pcontext = NULL;

    if(mqtt_client_loop(&mqtt_params) < 0)
        goto do_exit;
 
do_exit:
    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }

    return rc;
}

int main(int argc, char **argv)
{
    IOT_OpenLog("mqtt");
    IOT_SetLogLevel(IOT_LOG_DEBUG);

    mqtt_connect();

    IOT_DumpMemoryStats(IOT_LOG_DEBUG);
    IOT_CloseLog();

    return 0;
}