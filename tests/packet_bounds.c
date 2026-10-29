/* Host-only boundaries: RTOS, hardware, MQTT transport and parsed provisioning input. */
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packet_constants.inc"
#ifdef TEST_SMALL_PACKET
#undef MAX_JSON_STRING_SIZE
#define MAX_JSON_STRING_SIZE 128
#endif
#define __TIME64__ 1
#define pdTRUE 1
#define pdFALSE 0
#define WLAN0_IFACE 0
static void test_printf(const char *format, ...) { (void)format; }
#define PRINTF test_printf
#define USER_PROVISIONING_KEY "test-key"
typedef int64_t __time64_t;
static int User_semaphore;
static int publishes;
static int transport_result;
static struct {
    char Device_ID[10];
    char MQTT_timesync_current_time_str[MAX_TIMESYNC_LENGTH];
    int ACCEL_transmit_threshold;
} user_data, *pUserData = &user_data;
static struct {
    int16_t Xvalue, Yvalue, Zvalue;
    int64_t accelTime;
} accelXmitData[MAX_SAMPLES_PER_PACKET];
static char mqttMessage[MAX_JSON_STRING_SIZE];
static int take_semaphore(void *semaphore) { (void)semaphore; return 0; }
static void xSemaphoreGive(int semaphore) { (void)semaphore; }
static int get_battery_voltage(void) { return 3000; }
static void user_time64_msec_since_poweron(int64_t *time) { *time = 432000000; }
static int get_current_rssi(int iface) { (void)iface; return -50; }
static int xPortGetFreeHeapSize(void) { return 50000; }
static void vTaskDelay(int ticks) { (void)ticks; }
static int user_mqtt_send_message(void) { publishes++; return transport_result; }
#define cJSON_Object 1
#define cJSON_String 2
typedef struct cJSON {
    int type;
    char *string;
    char *valuestring;
    struct cJSON *child;
    struct cJSON *next;
} cJSON;
static cJSON key = {cJSON_String, "KEY", USER_PROVISIONING_KEY, NULL, NULL};
static cJSON root = {cJSON_Object, NULL, NULL, &key, NULL};
static cJSON *cJSON_Parse(const char *input) { (void)input; return &root; }
static void cJSON_Delete(cJSON *node) { (void)node; }
static char MACaddr[] = "ABCDEF";
#include "packet_functions.inc"

int main(void)
{
    strcpy(user_data.Device_ID, "ABCDEF");
    memset(user_data.MQTT_timesync_current_time_str, 'T', MAX_TIMESYNC_LENGTH - 1);
    for (int i = 0; i < MAX_SAMPLES_PER_PACKET; i++) {
        accelXmitData[i].Xvalue = INT16_MIN;
        accelXmitData[i].Yvalue = INT16_MAX;
        accelXmitData[i].Zvalue = INT16_MIN;
        accelXmitData[i].accelTime = 432000000;
    }
#ifdef TEST_SMALL_PACKET
    assert(send_json_packet(1, 1, 1) != 0);
    assert(publishes == 0);
    puts("Oversized packet rejected before publish");
    return 0;
#endif
    assert(send_json_packet(-1, 1, 1) != 0);
    assert(send_json_packet(MAX_SAMPLES_PER_PACKET + 1, 1, 1) != 0);
    assert(publishes == 0);
    assert(send_json_packet(0, 1, 1) == 0);
    assert(send_json_packet(MAX_SAMPLES_PER_PACKET, 1, 1) == 0);
    assert(publishes == 2);
    char expected[MAX_TIMESYNC_LENGTH + 32];
    snprintf(expected, sizeof(expected), "\"timesync\": \"%s\",\r\n", user_data.MQTT_timesync_current_time_str);
    assert(strstr(mqttMessage, expected) != NULL);
    assert(strstr(mqttMessage, "\r\n\t\t}\r\n\t}\r\n}\r\n") != NULL);
    transport_result = 7;
    assert(send_json_packet(1, 1, 1) == 7);
    size_t used = sizeof(mqttMessage) - 2;
    mqttMessage[used] = '\0';
    assert(append_packet_field(&used, "%s", "x") == 0);
    assert(used == sizeof(mqttMessage) - 1);
    assert(append_packet_field(&used, "%s", "y") != 0);
    assert(used == sizeof(mqttMessage) - 1);

    char reply[101];
    uint32_t length = 123;
    assert(neuralert_provisioning_step1("{}", reply, sizeof(reply), &length) == 0);
    assert(length == strlen(reply));
    size_t exact_capacity = length + 1;
    assert(neuralert_provisioning_step1("{}", reply, exact_capacity, &length) == 0);
    assert(neuralert_provisioning_step1("{}", reply, exact_capacity - 1, &length) != 0);
    assert(length == 0);
    char tiny[1] = {'x'};
    assert(neuralert_provisioning_step1("{}", tiny, sizeof(tiny), &length) != 0);
    assert(tiny[0] == '\0' && length == 0);
    assert(neuralert_provisioning_step1("{}", tiny, 0, &length) != 0);
    assert(length == 0);
    puts("Packet and provisioning boundary checks passed");
    return 0;
}
