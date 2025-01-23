 # LPSD Design Project Challenge
 Max, Stefan, Linus

 ## Based on: 
 - ESP wifi example
 - Stefan ADC code
 - ChatGPT
 
 This readme must be updated...

## Was verbessert werden muss: 

nach deepsleep startet main von neu (das ist so), darum: 
- MIT RTC and NVS memory daten durch deepsleep speichern
- adc_init und de_init möglichst nur einmal durchführen
- set_esp_time_from_build() nur einmal durchführen
- pro iteration: 
    - message updaten
    - read_temperature und connect_and_send() machen was gut funktioniert

- nach 10 iterationen in undendlich deepsleep gehen

