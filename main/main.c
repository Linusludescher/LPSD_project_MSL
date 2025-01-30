// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_sleep.h"

// #include "ADC_read.h"
// #include "connect_send.h"
// #include "message_time.h"


// // persistent variable durch sleep periode, if /= 0 kein init();
// static RTC_DATA_ATTR uint8_t sleep_cycle_count = 0; // counter
// static RTC_DATA_ATTR time_t rtc_time_at_sleep = 0; // Store last known time


// /*
// Was verbessert werden muss: 

// nach deepsleep startet main von neu (das ist so), darum: 
// - MIT RTC and NVS memory daten durch deepsleep speichern +
// - adc_init und de_init möglichst nur einmal durchführen +
// - set_esp_time_from_build() nur einmal durchführen +
// - pro iteration: 
//     - message updaten
//     - read_temperature und connect_and_send() machen was gut funktioniert

// - nach 10 iterationen in undendlich deepsleep gehen
// */

// void app_main(void)
// {
//     adc_init();
//     printf("starting main\n");
//     wifi_init_sta();         // Ensure Wi-Fi is connected

//     if (sleep_cycle_count == 0) {
//         // nur beim ersten boot
//         //init(); #notwendige init funktionen??
//         initialize_sntp(); // Fetch time from NTP server
//     }
	

//     // Load the time from memory for the write_message function
//     time_t current_time = rtc_time_at_sleep;

// 	float temperature = read_temperature();

//     char message[128];
//     write_message(message, temperature);
//     printf("Message: %s\n", message);
	
	
// 	sleep_cycle_count++;
	
// 	//deep sleep für immer nach 10 iterationen/messungen
// 	if (sleep_cycle_count >= 10) {
//         // Reset the count and enter indefinite deep sleep
//         //printf("Reached 10 cycles, entering indefinite deep sleep\n");
//         sleep_cycle_count = 0;
//         esp_deep_sleep_start();
    
//     // Add 30 seconds to the saved time before entering deep sleep
//     rtc_time_at_sleep += 30; // Add 30 seconds
//     time(&rtc_time_at_sleep); // save time before deep sleep

//     uint64_t deepsleep_us = 30*1000000; //30sec
// 	esp_deep_sleep(deepsleep_us);
//     adc_deinit();
// }
// }

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"

#include "ADC_read.h"
#include "connect_send.h"
#include "message_time.h"
#include "time.h"


#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static RTC_DATA_ATTR uint8_t sleep_cycle_count = 0; // counter
RTC_DATA_ATTR time_t rtc_time_at_sleep = 0; // Store last known time

static RTC_DATA_ATTR float temp_data[10]; // 10 messsungen
static RTC_DATA_ATTR int temp_data_size = 0;
#define NUM_SAMPLES 10

void append_temp_data(float new_value) {
    if (temp_data_size < NUM_SAMPLES) {
        temp_data[temp_data_size++] = new_value;  // Add value and increase size
        //printf("Appended: %f, New Size: %d\n", new_value, temp_data_size);
    } else {
        //printf("Array is full! Cannot append more data.\n");
    }
}

const char *ntpServer = "pool.ntp.org";

void initialize_sntp(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    while (timeinfo.tm_year < (2020 - 1900)) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

void app_main(void)
{
    //printf("starting main\n");
    adc_init();
    if(sleep_cycle_count == 0){
        //set_esp_time_from_build();
        nvs_flash_init();  // Required for Wi-Fi
        wifi_init_sta();    // Connect to Wi-Fi

        // Wait until Wi-Fi is connected before calling SNTP
        vTaskDelay(50 / portTICK_PERIOD_MS);

        initialize_sntp();  // Fetch time from server

    }


    sleep_cycle_count++;
    //printf("Sleepcycle: %d\n", sleep_cycle_count);

    float temperature = read_temperature();
    append_temp_data(temperature);
    //printf("temp: %f \n", temperature);
    time_t now;
    time(&now);
    //printf("Current time: %s", ctime(&now));
    


    // char message[128];
    //write_message(message, temperature);

    //connect_and_send(temperature); //this works

    if(sleep_cycle_count == NUM_SAMPLES){
        connect_and_send_array(temp_data);
        //printf("sent data");
        esp_deep_sleep(300*1000000);
    }
    adc_deinit();
    uint64_t deepsleep_us = 29*1000000;
    esp_deep_sleep(deepsleep_us);
}