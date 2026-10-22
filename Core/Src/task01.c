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

#include "stm32_log.h"
#include "usbd_cdc_if.h"

log_port_ret_t log_port_write(const char *data, uint32_t len)
{
//    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
//        return LOG_PORT_DONE;                       // chưa cắm / chưa enumerate: bỏ dòng này
//    }
    if (CDC_Transmit_FS((uint8_t *)data, (uint16_t)len) != USBD_OK) {
        return LOG_PORT_BUSY;                       // chưa nhận, lần sau thử lại, không mất log
    }
    return LOG_PORT_ASYNC;                          // slot được giữ tới khi USB gửi xong
}

const unsigned char* TAG = "MQTT";
#define MQTT_RX_BUFFER_SIZE    (128U)

static uint8_t mqtt_rx_buffer[MQTT_RX_BUFFER_SIZE];

/* Được callback đặt = 1 khi nhận hoàn chỉnh một payload */
static volatile uint8_t mqtt_frame_ready = 0U;
static volatile uint32_t mqtt_frame_len = 0U;

/* Các biến này chỉ được sử dụng trong MQTT callbacks */
static uint32_t mqtt_expected_len = 0U;
static uint32_t mqtt_received_len = 0U;
static uint8_t mqtt_discard_current = 0U;


mqtt_connection_status_t con_status = MQTT_CONNECT_DISCONNECTED;

static mqtt_client_t *mqtt_client;

static struct mqtt_connect_client_info_t client_info = {
		.client_id 		= "stm32_client_001",
		.client_user	= NULL,
		.client_pass 	= NULL,
		.keep_alive		= 60,
		.will_topic		= "stm32/cmd"
};

static ip_addr_t broker_ip;


static void mqtt_request_cb(void *arg, err_t err)
{
    (void)arg;
    LOGI(TAG, "Request result: %d", err);
}

void my_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status){
	(void)arg;
	if(status == MQTT_CONNECT_ACCEPTED){
		LOGI(TAG, "Connected");

		err_t err = mqtt_subscribe(
			client, "stm32/cmd", 0,
			mqtt_request_cb, NULL);
		con_status = MQTT_CONNECT_ACCEPTED;
		LOGI(TAG, "Subscribe call: %d", err);
	}
	else{
		LOGI(TAG, "Connection status: %d",
		               (int)status);
		con_status = MQTT_CONNECT_DISCONNECTED;
	}
}

void my_mqtt_incoming_data_cb(
    void *arg,
    const u8_t *data,
    u16_t len,
    u8_t flags)
{
    (void)arg;

    /* Một message đang bị loại bỏ hoặc buffer đang bận */
    if (mqtt_discard_current != 0U) {
        if ((flags & MQTT_DATA_FLAG_LAST) != 0U) {
            mqtt_discard_current = 0U;
        }
        return;
    }

    /* Kiểm tra con trỏ và giới hạn độ dài trước khi copy */
    if (((len > 0U) && (data == NULL)) ||
        (mqtt_received_len > mqtt_expected_len) ||
        ((uint32_t)len > mqtt_expected_len - mqtt_received_len) ||
        ((uint32_t)len >
         (sizeof(mqtt_rx_buffer) - mqtt_received_len))) {

        LOGI(TAG, "Invalid fragment: received=%lu, len=%u",
             (unsigned long)mqtt_received_len,
             (unsigned)len);

        mqtt_discard_current = 1U;
        return;
    }

    if (len > 0U) {
        memcpy(&mqtt_rx_buffer[mqtt_received_len], data, len);
        mqtt_received_len += len;
    }

    /* Chỉ hoàn tất message ở fragment cuối */
    if ((flags & MQTT_DATA_FLAG_LAST) != 0U) {
        if (mqtt_received_len == mqtt_expected_len) {
            mqtt_frame_len = mqtt_received_len;

            /* Báo cho task ứng dụng biết dữ liệu đã sẵn sàng */
            __DMB();
            mqtt_frame_ready = 1U;

            LOGI(TAG, "RX complete: %lu bytes",
                 (unsigned long)mqtt_frame_len);
        } else {
            LOGI(TAG, "RX incomplete: got=%lu, expected=%lu",
                 (unsigned long)mqtt_received_len,
                 (unsigned long)mqtt_expected_len);
        }
    }
}

void my_mqtt_pub_cb(void *arg, const char *topic, u32_t tot_len)
{
    (void)arg;

    if (mqtt_frame_ready != 0U) {
        /* Frame trước chưa được ứng dụng xử lý */
        mqtt_discard_current = 1U;

        LOGI(TAG, "RX busy, drop topic=%s, len=%lu",
             topic, (unsigned long)tot_len);
        return;
    }

    mqtt_expected_len = tot_len;
    mqtt_received_len = 0U;
    mqtt_discard_current = 0U;

    if (tot_len > sizeof(mqtt_rx_buffer)) {
        mqtt_discard_current = 1U;

        LOGI(TAG, "Payload too large: %lu",
             (unsigned long)tot_len);
        return;
    }

    LOGI(TAG, "Start RX: topic=%s, len=%lu",
         topic, (unsigned long)tot_len);

    /* API quy định không gọi data callback nếu payload rỗng */
    if (tot_len == 0U) {
        mqtt_frame_len = 0U;
        mqtt_frame_ready = 1U;
    }
}

static void mqtt_start_core(void* arg){
	(void)arg;

	mqtt_client = mqtt_client_new();
	if(mqtt_client == NULL){
		LOGI(TAG, "Allocation failed");
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

	LOGI(TAG, "Connect call: %d", err);
}

static void mqtt_publish_done(void *arg, err_t err)
{
    (void)arg;

    if (err == ERR_OK) {
        LOGI(TAG, "Publish completed");
    } else {
        LOGI(TAG, "Publish failed: %d", err);
    }
}

void StartTask01(void *argument)
{
	/* init code for LWIP */
	osDelay(500);
	MX_LWIP_Init();

	/* init code for USB_DEVICE */
	MX_USB_DEVICE_Init();
	/* USER CODE BEGIN StartTask01 */

	IP_ADDR4(&broker_ip, 10, 70, 22, 105);
	err_t err = tcpip_callback(mqtt_start_core, NULL);
	LOGI(TAG, "Schedule start: %d", err);
	uint32_t last_check = osKernelGetTickCount();
	uint32_t last_pub = osKernelGetTickCount();

	const char payload[64];

	/* Infinite loop */
	for(;;)
	{
		osDelay(1);
		// auto reconnect
		if(mqtt_client_is_connected(mqtt_client) != 1 && (osKernelGetTickCount() - last_check >= 1000)){ // connected to the server
			last_check = osKernelGetTickCount();
			mqtt_disconnect(mqtt_client);
			err_t err = mqtt_client_connect(mqtt_client,
												&broker_ip,
												MQTT_PORT,
												my_mqtt_connection_cb,
												NULL,
												&client_info);
			LOGI(TAG, "Connect call: %d", err);
		}
		// publish tick count
		if(mqtt_client_is_connected(mqtt_client) == 1 && (osKernelGetTickCount() - last_pub >= 1000)){ // connected to the server
			last_pub = osKernelGetTickCount();

			int leng_pay = snprintf(payload, 64, "TickCount %d", HAL_GetTick());
			mqtt_publish(mqtt_client, "stm32/tick", payload, leng_pay, 0, 1, mqtt_publish_done, NULL);
		}
		if (mqtt_frame_ready != 0U) {
		    __DMB();
		    if(strncmp(mqtt_rx_buffer, "LED_ON", 6) == 0){
		    	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
		    }
		    else{
		    	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
		    }

		    LOGI(TAG, mqtt_rx_buffer);
		    memset(mqtt_rx_buffer, 0x00, MQTT_RX_BUFFER_SIZE);
		    mqtt_frame_ready = 0U;
		}
		log_handler();
	}
	/* USER CODE END StartTask01 */
}

