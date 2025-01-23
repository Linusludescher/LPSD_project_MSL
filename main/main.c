#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"

#include "ADC_read.h"
#include "connect_send.h"
#include "message_time.h"

/*
Was verbessert werden muss: 

nach deepsleep startet main von neu (das ist so), darum: 
- MIT RTC and NVS memory daten durch deepsleep speichern
- adc_init und de_init möglichst nur einmal durchführen
- set_esp_time_from_build() nur einmal durchführen
- pro iteration: 
    - message updaten
    - read_temperature und connect_and_send() machen was gut funktioniert

- nach 10 iterationen in undendlich deepsleep gehen
*/

void app_main(void)
{
    printf("starting main\n");
    init();
    set_esp_time_from_build();
    uint64_t deepsleep_us = 5*1000000;


    for(uint8_t i = 0; i < 10; i++) {
        float temperature = read_temperature();

        char message[128];
        write_message(message, temperature);
        printf("Message: %s\n", message);

        //connect_and_send(temperature); //this works

        esp_deep_sleep(deepsleep_us);
    }
    deinit();
}