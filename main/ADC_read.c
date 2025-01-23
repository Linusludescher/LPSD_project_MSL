#include <stdio.h>
#include "ADC_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_sleep.h"
#include "esp_sntp.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

const static char *TAG = "ADC_READ";

#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_2

#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_6
#define NR_MEASUREMENTS             12
//#define OUTPUT 
#define IQR_ENABLED

//Wifi Constants
#define WIFI_SSID "lpsd"
#define WIFI_PASS "lpsd2024"
#define SERVER_HOST "pbl.permasense.uibk.ac.at"
#define SERVER_PORT 22504
#define GROUP_ID 8

adc_oneshot_unit_handle_t adc1_handle;
adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
};
adc_cali_handle_t adc1_cali_chan0_handle = NULL;
static int adc_raw;
static int voltage;
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);\

int compare(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

float medianFromIndices(int *arr, int size) {
    if (size % 2 == 0) {
        return (arr[size / 2 - 1] + arr[size / 2]) / 2.0;
    } else {
        return arr[size / 2];
    }
}

float findOutliers(int *arr, int size, double multiplier) {
    if (size % 2 != 0) {
        printf("This program is designed to work with even sizes.\n");
        return 1.0;
    }
    qsort(arr, size, sizeof(int), compare);

    float Q1 = medianFromIndices(arr, size / 2);
    float Q3 = medianFromIndices(arr + size / 2, size / 2);
    
    float IQR = Q3 - Q1;
    float lowerThreshold;
    float upperThreshold;
#ifdef OUTPUT
    printf("Q1: %.2f, Q3: %.2f, IQR: %.2f\n", Q1, Q3, IQR);
#endif

    int good_vals = 0;
    while (!good_vals)
    {
        lowerThreshold = Q1 - multiplier * IQR;
        upperThreshold = Q3 + multiplier * IQR;

        for (int i = 0; i < size; i++) {
            if (arr[i] >= lowerThreshold && arr[i] <= upperThreshold) {
                good_vals++;
            }
        }

#ifdef OUTPUT
        printf("multiplier: %.2f, lower_threshold: %.2f, upper_threshold: %.2f\n", multiplier, lowerThreshold, upperThreshold);
#endif
        multiplier += 0.2;
        if (multiplier > 5)
        {
            int sum = 0;
            for (int i = 0; i < 10; i++)
                sum += arr[i];
            return sum/10;
        }
        
    }// Step 3: Print results
    
    int sum = 0, nr_selected = 0;
#ifdef OUTPUT
    printf("Filtered values (without outliers): ");
#endif
    for (int i = 0; i < size; i++) {
        if (arr[i] >= lowerThreshold && arr[i] <= upperThreshold) {
#ifdef OUTPUT
            printf("%d ", arr[i]);
#endif
            sum += arr[i];
            nr_selected++;
        }
    }

#ifdef OUTPUT
    printf("\nOutliers: ");
    for (int i = 0; i < size; i++) {
        if (arr[i] < lowerThreshold || arr[i] > upperThreshold) {
            printf("%d ", arr[i]);
        }
    }
    printf("\n");
#endif

    return 1.0*sum/nr_selected;
}

float read_temperature()
{
    int read_voltages[NR_MEASUREMENTS];
    for (int i = 0; i < NR_MEASUREMENTS; i++)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw));
        
#ifdef OUTPUT
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw);
#endif
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw, &voltage));
        read_voltages[i%NR_MEASUREMENTS] = voltage;
    }

    float v = 0;
#ifdef IQR_ENABLED
    v = findOutliers(read_voltages, NR_MEASUREMENTS, 0.8);
#else
    for (int i = 0; i < NR_MEASUREMENTS; i++)
    {
        v += read_voltages[i];
    }
    v /= NR_MEASUREMENTS;
#endif
    int beta = 3976;
    double resistance = 27000.0*v/(3300 - v);
    double temp = beta / (log(resistance / 10000) + beta / 298.15) - 273.15;
    
#ifdef OUTPUT
    ESP_LOGW(TAG, "ADC%d Channel[%d] Median Voltage Read: %.2f mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, v);
    ESP_LOGW(TAG, "ADC%d Channel[%d] Resistance %.2f kΩ", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, resistance/1000);
    ESP_LOGW(TAG, "ADC%d Channel[%d] Temperature %.2f°C", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, temp);
#endif

    return temp;
}

int64_t xx_time_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000000L + (tv.tv_usec));
}

bool adc_init()
{
    //-------------ADC1 Init---------------//
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    //-------------ADC1 Calibration Init---------------//
    bool do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);
    if (!do_calibration1_chan0) {
        ESP_LOGW(TAG, "ADC1 Channel 0 calibration is false");
    }
    return do_calibration1_chan0;
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
}
void adc_deinit()
{
    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    example_adc_calibration_deinit(adc1_cali_chan0_handle);

}

/*
void connect_to_wifi() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    assert(netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "Connecting to Wi-Fi...");

    EventBits_t bits = xEventGroupWaitBits(
        esp_wifi_connect_event_group(),
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(10000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Connected to Wi-Fi!");
    } else {
        ESP_LOGE(WIFI_TAG, "Failed to connect to Wi-Fi.");
    }
}

*/

/**
 * Send temperature data to the server
 * 
 * param temperature is Current temperature value to send
 */

/*
void send_temperature(float temperature) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_HOST);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(SERVER_PORT);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(WIFI_TAG, "Failed to create socket.");
        return;
    }

    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(WIFI_TAG, "Failed to connect to server.");
        close(sock);
        return;
    }

    char payload[128];
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S%z", &timeinfo);

    snprintf(payload, sizeof(payload), "%s,%d,%.4f\n", time_str, GROUP_ID, temperature);
    send(sock, payload, strlen(payload), 0);

    ESP_LOGI(WIFI_TAG, "Sent data: %s", payload);
    close(sock);
}
*/