#include "AUTOpairing.h"
#include "ADConeshot.h"
#include "cJSON.h"

extern "C"
{
	void app_main(void);
}

#define LOG_LEVEL_LOCAL ESP_LOG_VERBOSE

#define LOG_TAG "main"

static AUTOpairing_t clienteAP;
static ADConeshot_t ADConeshot;

const char *ESP_WIFI_SSID = "MOVISTAR_13E8";
const char *ESP_WIFI_PASS = "zKH7MNJTE3FHKV7477FJ";
/* const char* ESP_WIFI_SSID = "HortSost";
const char* ESP_WIFI_PASS = "9b11c2671e5b"; */
const char *FIRMWARE_UPGRADE_URL = "https://huertociencias.uma.es/ESP32OTA/espnow_project.bin";

UpdateStatus updateStatus = NO_UPDATE_FOUND;

int adc_channel[1] = {ADC_CHANNEL_0};
// static adc_channel_t adc_channel[4] = {ADC_CHANNEL_0,ADC_CHANNEL_1,ADC_CHANNEL_2,ADC_CHANNEL_4};
uint8_t lengthADC1_CHAN = sizeof(adc_channel) / sizeof(adc_channel_t);

typedef struct
{
	uint8_t tsleep;
	uint8_t pan;
	uint16_t timeout;
} struct_config;

struct_config strConfig;

void pan_process_msg(struct_espnow_rcv_msg *my_msg){
	ESP_LOGI("* PAN process", "Mensaje PAN recibido");
	ESP_LOGI("* PAN process", "Antiguedad mensaje (ms): %lu",my_msg->ms_old);
	ESP_LOGI("* PAN process", "MAC: %02X:%02X:%02X:%02X:%02X:%02X", my_msg->macAddr[0], my_msg->macAddr[1], my_msg->macAddr[2], my_msg->macAddr[3], my_msg->macAddr[4], my_msg->macAddr[5]);
	ESP_LOGI("* PAN process", "Payload: %s",my_msg->payload);
	ESP_LOGI("* PAN process", "Mi PAN: %d", clienteAP.get_pan());
	
	free(my_msg->payload);
}

void mqtt_process_msg(struct_espnow_rcv_msg *my_msg)
{
	ESP_LOGI("* mqtt process", "topic: %s", my_msg->topic);
	cJSON *root2 = cJSON_Parse(my_msg->payload);
	if (strcmp(my_msg->topic, "config") == 0)
	{
		ESP_LOGI("* mqtt process", "payload: %s", my_msg->payload);
		ESP_LOGI("* mqtt process", "Deserialize payload.....");
		cJSON *sleep = cJSON_GetObjectItem(root2, "sleep");
		cJSON *timeout = cJSON_GetObjectItem(root2, "timeout");
		cJSON *pan = cJSON_GetObjectItem(root2, "pan");
		uint16_t config[MAX_CONFIG_SIZE]; // max config size
		if (timeout)
			config[1] = timeout->valueint;
		if (sleep)
			config[2] = sleep->valueint;
		if (pan)
			config[3] = pan->valueint;
		ESP_LOGI("* mqtt process", "tsleep = %d", config[1]);
		ESP_LOGI("* mqtt process", "timeout = %d", config[2]);
		ESP_LOGI("* mqtt process", "PAN_ID = %d", config[3]);
		clienteAP.set_config(config);
	}
	if (strcmp(my_msg->topic, "update") == 0)
	{
		updateStatus = THERE_IS_AN_UPDATE_AVAILABLE;
		// clienteAP.init_update(); ¿Porque funcione en app_main y aqui no? :(
	}
	free(my_msg->topic);
	free(my_msg->payload);
	cJSON_Delete(root2);
}

void app_main(void)
{
	clienteAP.init_config_size(sizeof(strConfig));
	if (!clienteAP.get_config((uint16_t *)&strConfig))
	{
		strConfig.timeout = 3000;
		strConfig.tsleep = 10;
	}

	struct_adclist *my_reads = (struct_adclist *)malloc(lengthADC1_CHAN * sizeof(struct_adclist));
	my_reads->length = lengthADC1_CHAN;
	ESP_LOGI(TAG, "ADC Length = %d", lengthADC1_CHAN);
	my_reads->adc_read = (struct_adcread *)malloc(my_reads->length * sizeof(struct_adcread));
	my_reads->chn = (int *)malloc(lengthADC1_CHAN);
	for (int i = 0; i < my_reads->length; i++)
	{
		my_reads->chn[i] = adc_channel[i];
		my_reads->adc_read[i].AN_i = 0;
		my_reads->adc_read[i].adc_raw = 0;
		my_reads->adc_read[i].voltage = 0;
		my_reads->adc_read[i].sum = 0;
		my_reads->adc_read[i].adc_filtered = 0;
		for (int j = 0; j < FILTER_LEN; j++)
			my_reads->adc_read[i].adc_buff[j] = 0;
	}
	
	clienteAP.esp_set_https_update(FIRMWARE_UPGRADE_URL, ESP_WIFI_SSID, ESP_WIFI_PASS);
	clienteAP.set_timeOut(strConfig.timeout, true); // tiempo máximo
	clienteAP.set_deepSleep(strConfig.tsleep);		// tiempo dormido en segundos
	ESP_LOGI("Config debug","Timeout = %d",strConfig.timeout);
	ESP_LOGI("Config debug","Timeout = %d",strConfig.tsleep);
	clienteAP.set_channel(6);						// canal donde empieza el scaneo
	clienteAP.set_pan(2);
	clienteAP.set_mqtt_msg_callback(mqtt_process_msg);		// por defecto a NULL -> no se llama a ninguna función
	clienteAP.set_pan_msg_callback(pan_process_msg);
	clienteAP.begin();

	if (clienteAP.envio_disponible() == true)
	{
		cJSON *root, *fmt;
		const esp_partition_t *running = esp_ota_get_running_partition();
		esp_app_desc_t running_app_info;
		char tag[25];
		ADConeshot.adc_init(my_reads);
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
			cJSON_AddNumberToObject(fmt, "adc_filtered", ADConeshot.get_adc_filtered_read(my_reads, i));
			cJSON_AddNumberToObject(fmt, "adc_voltage", ADConeshot.get_adc_voltage_read(my_reads, i));
		}
		char *my_json_string = cJSON_Print(root);
		ESP_LOGI(TAG, "my_json_string\n%s", my_json_string);
		clienteAP.espnow_send_check(my_json_string); // hará deepsleep por defecto

		free(my_reads);
		cJSON_Delete(root);
		cJSON_free(my_json_string);

		switch (updateStatus)
		{
		case THERE_IS_AN_UPDATE_AVAILABLE:
			clienteAP.init_update();
			break;
		case NO_UPDATE_FOUND:
			// get deep sleep enter time
			//						gotoSleep();
			break;
		}
	}
}
