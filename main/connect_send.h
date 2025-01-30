#ifndef CONNECT_SEND_H
#define CONNECT_SEND_H

void connect_and_send(float temp);
void wifi_init_sta(void);
void connect_and_send_array(float *temp_data);
#endif 