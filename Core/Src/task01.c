#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usb_device.h"
#include "gpio.h"

#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/tcpip.h"
#include <stdio.h>
#include <string.h>

static mqtt_client_t *mqtt_client;

static struct mqtt_connect_client_info_t client_info = {
		.client_id 		= "stm32_client_001",
		.client_user	= NULL,
		.client_pass 	= NULL,
		.keep_alive		= 60,
		.will_topic		= "hello server!"
};

static ip_addr_t broker_ip;


static void mqtt_request_cb(void *arg, err_t err)
{
    (void)arg;
    printf("[MQTT] Request result: %d\r\n", err);
}

void my_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status){
	(void)arg;
	if(status == MQTT_CONNECT_ACCEPTED){
		printf("[MQTT] Connected\r\n");

		err_t err = mqtt_subscribe(
			client, "stm32/cmd", 0,
			mqtt_request_cb, NULL);

		printf("[MQTT] Subscribe call: %d\r\n", err);
	}
	else{
		printf("[MQTT] Connection status: %d\r\n",
		               (int)status);
	}
}

void my_mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags){
	(void)arg;

	printf("[MQTT] Data fragment, len=%u, last=%u\r\n",
		   (unsigned)len,
		   (unsigned)((flags & MQTT_DATA_FLAG_LAST) != 0));

	/* Chưa xử lý payload ở bài đầu tiên */
	(void)data;
}

void my_mqtt_pub_cb(void *arg, const char *topic, u32_t tot_len){
	(void)arg;
	printf("[MQTT] Topic: %s, length: %lu\r\n", topic, (unsigned long)tot_len);
}

static void mqtt_start_core(void* arg){
	(void)arg;

	mqtt_client = mqtt_client_new();
	if(mqtt_client == NULL){
		printf("[MQTT] Allocation failed\r\n");
		return;
	}

	mqtt_set_inpub_callback(mqtt_client,
							my_mqtt_pub_cb,
							my_mqtt_incoming_data_cb,
							NULL);

	err_t err = mqtt_client_connect(mqtt_client,
									&broker_ip,
									MQTT_PORT,
									my_mqtt_connection_cb,
									NULL,
									&client_info);

	printf("[MQTT] Connect call: %d\r\n", err);
}

void StartTask01(void *argument)
{
	/* init code for LWIP */
	osDelay(100);
	MX_LWIP_Init();

	/* init code for USB_DEVICE */
	MX_USB_DEVICE_Init();
	/* USER CODE BEGIN StartTask01 */

	osDelay(2000);
	IP_ADDR4(&broker_ip, 192, 168, 0, 102);
	err_t err = tcpip_callback(mqtt_start_core, NULL);
	printf("[MQTT] Schedule start: %d\r\n", err);


	/* Infinite loop */
	for(;;)
	{
		osDelay(1000);
		printf("[MQTT] Keep alive %d\r\n", err);
	}
	/* USER CODE END StartTask01 */
}

