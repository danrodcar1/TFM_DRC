#include "AUTOpairing.h"
#include "ADConeshot.h"
#include "AnomalyDetection.h"
#include "cJSON.h"

using namespace std;

// https://github.com/topics/pearson-correlation-coefficient?l=c%2B%2B&o=asc&s=updated
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/memory-types.html
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/performance/size.html
extern "C"
{
	void app_main(void);
}

#define LOG_LEVEL_LOCAL ESP_LOG_VERBOSE

#define LOG_TAG "main"

static AUTOpairing_t clienteAP;
static ADConeshot_t ADConeshot;
static AnomalyDetection_t AnomalyDetect;

const char *ESP_WIFI_SSID = "IoTLab";
const char *ESP_WIFI_PASS = "4cc3s0IoT@";
const char *FIRMWARE_UPGRADE_URL = "https://huertociencias.uma.es/ESP32OTA/espnow_project.bin";

UpdateStatus updateStatus = NO_UPDATE_FOUND;

// int adc_channel[1] = {ADC_CHANNEL_0};
static adc_channel_t adc_channel[4] = {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_2, ADC_CHANNEL_4};
// static adc_channel_t adc_channel[1] = {ADC_CHANNEL_0};
uint8_t lengthADC1_CHAN = sizeof(adc_channel) / sizeof(adc_channel_t);

typedef struct
{
	uint8_t pan;
	uint16_t timeout;
	uint16_t tsleep;
	uint16_t time;
} struct_config;
struct_config strConfig;

// typedef struct
// { // new structure for DATA SAVING
// 	uint16_t own_raw_data;
// 	uint16_t peer_raw_data;
// } struct_readings;

// RTC_DATA_ATTR static struct_readings data_read[ESPNOW_MAXIMUM_READINGS]; // Datos propios
// https://github.com/G6EJD/ESP32_RTC_RAM/blob/master/ESP32_BME280_RTCRAM_Datalogger.ino
RTC_DATA_ATTR static uint16_t own_raw_data[ESPNOW_MAXIMUM_READINGS];  // Datos propios
RTC_DATA_ATTR static uint16_t peer_raw_data[ESPNOW_MAXIMUM_READINGS]; // Datos peer
RTC_DATA_ATTR uint8_t personal_data_count = 0;
RTC_DATA_ATTR uint8_t incoming_data_count = 0;

// https://github.com/DaveGamble/cJSON#building

void pan_process_msg(struct_espnow_rcv_msg *my_msg)
{
	ESP_LOGI("* PAN process", "Mensaje PAN recibido");
	ESP_LOGI("* PAN process", "Antiguedad mensaje (ms): %lu", my_msg->ms_old);
	ESP_LOGI("* PAN process", "MAC: %02X:%02X:%02X:%02X:%02X:%02X", my_msg->macAddr[0], my_msg->macAddr[1], my_msg->macAddr[2], my_msg->macAddr[3], my_msg->macAddr[4], my_msg->macAddr[5]);
	ESP_LOGI("* PAN process", "Payload: %s", my_msg->payload);
	ESP_LOGI("* PAN process", "My PAN: %d", clienteAP.get_pan());
	ESP_LOGI("* PAN process", "Peer PAN: %d", clienteAP.get_pan());
	// Parse the JSON data
	cJSON *json = cJSON_Parse(my_msg->payload);
	//  Check if parsing was successful
	if (json == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		cJSON_Delete(json);
		return;
	}
	cJSON *data = cJSON_GetObjectItem(json, "Sensor0");
	if (data)
	{
		cJSON *volt_data = cJSON_GetObjectItem(data, "adc_voltage");
		cJSON *raw_data = cJSON_GetObjectItem(data, "raw_voltage");
		peer_raw_data[incoming_data_count] = raw_data->valueint;
		ESP_LOGI("Peer_reading", "ADC_FILTERED = %d", volt_data->valueint);
		ESP_LOGI("Peer_reading", "ADC_VOLTAGE = %d", raw_data->valueint);
		ESP_LOGI(TAG, "peer_data_raw:%d", peer_raw_data[incoming_data_count]);
		incoming_data_count++;
		if (incoming_data_count >= ESPNOW_MAXIMUM_READINGS)
		{
			// We can do something interesting here
			incoming_data_count = 0; // Reset counter
		}
	}
	cJSON_Delete(json);
}

void mqtt_process_msg(struct_espnow_rcv_msg *my_msg)
{
	ESP_LOGI("* mqtt process", "topic: %s", my_msg->topic);
	cJSON *root = cJSON_Parse(my_msg->payload);
	if (root == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		return;
	}
	if (strcmp(my_msg->topic, "config") == 0)
	{
		ESP_LOGI("* mqtt process", "payload: %s", my_msg->payload);
		ESP_LOGI("* mqtt process", "Deserialize payload.....");
		cJSON *sleep = cJSON_GetObjectItem(root, "sleep");
		cJSON *timeout = cJSON_GetObjectItem(root, "timeout");
		cJSON *pan = cJSON_GetObjectItem(root, "pan");
		uint16_t config[MAX_CONFIG_SIZE]; // max config size
		if (timeout)
			config[1] = timeout->valueint;
		if (sleep)
			config[2] = sleep->valueint;
		if (pan)
			config[3] = pan->valueint;
		clienteAP.set_config(config);
	}
	if (strcmp(my_msg->topic, "update") == 0)
	{
		updateStatus = THERE_IS_AN_UPDATE_AVAILABLE;
		clienteAP.init_update(); // ¿Porque funciona en app_main y aqui no? :(

		// Añadir en main si se quiere funcionalidad

		// switch (updateStatus)
		// {
		// case THERE_IS_AN_UPDATE_AVAILABLE:
		// 	clienteAP.init_update();
		// 	break;
		// case NO_UPDATE_FOUND:
		// 	// get deep sleep enter time
		// 	//						gotoSleep();
		// 	break;
		// }
	}
	free(my_msg->topic);
	free(my_msg->payload);
	cJSON_Delete(root);
}

void app_main(void)
{
	clienteAP.init_config_size(sizeof(strConfig));
	if (clienteAP.get_config((uint16_t *)&strConfig) == false)
	{
		strConfig.timeout = 3000;
		strConfig.tsleep = 30;
	}

	struct_adclist *my_reads = ADConeshot.set_adc_channel(adc_channel, lengthADC1_CHAN);
	clienteAP.esp_set_https_update(FIRMWARE_UPGRADE_URL, ESP_WIFI_SSID, ESP_WIFI_PASS);
	clienteAP.set_timeOut(strConfig.timeout, true); // tiempo máximo
	clienteAP.set_deepSleep(strConfig.tsleep);		// tiempo dormido en segundos
	ESP_LOGI("Config debug", "Timeout = %d", strConfig.timeout);
	ESP_LOGI("Config debug", "Timesleep = %d", strConfig.tsleep);
	clienteAP.set_channel(1);						   // canal donde empieza el scaneo
	clienteAP.set_app_area(4, 5);					   // indico PAN y aplicación a la que pertenece
	clienteAP.set_mqtt_msg_callback(mqtt_process_msg); // por defecto a NULL -> no se llama a ninguna función
	clienteAP.set_pan_msg_callback(pan_process_msg);
	clienteAP.begin();

	if (clienteAP.envio_disponible() == true)
	{
		cJSON *root, *fmt;
		const esp_partition_t *running = esp_ota_get_running_partition();
		esp_app_desc_t running_app_info;
		char tag[25];
		// ADConeshot.adc_init(my_reads);

		root = cJSON_CreateObject();
		if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
		{
			cJSON_AddStringToObject(root, "fw_version", running_app_info.version);
		}

		for (int i = 0; i < my_reads->length; i++)
		{
			ESP_LOGI(TAG, "Serialize readings of channel %d", i);
			sprintf(tag, "Sensor%d", i);
			cJSON_AddItemToObject(root, tag, fmt = cJSON_CreateObject());

			cJSON_AddNumberToObject(fmt, "adc_voltage", 1234);
			//cJSON_AddNumberToObject(fmt, "raw_voltage", 1234);
			//cJSON_AddNumberToObject(fmt, "dummy_val", 1234);
		}
		// Build topic from incoming message and public
		char *send_topic = "unbuentopic/tienecache";
		char *my_json_string = cJSON_PrintUnformatted(root);
		size_t msg_size = strlen(my_json_string);
		ESP_LOGI(TAG, "my_json_string\n%s", my_json_string);
		ESP_LOGI("* Tamaño paquete", "El mensaje ocupa %i bytes", msg_size);

		clienteAP.espnow_send_check(send_topic, my_json_string); // hará deepsleep por defecto
		free(my_reads);
		cJSON_Delete(root);
		cJSON_free(my_json_string);
	}
}
