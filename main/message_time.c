#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include "message_time.h"

static const char* build_time = __DATE__ " " __TIME__;

void set_esp_time_from_build() {
    struct tm tm_time = {0};
    strptime(build_time, "%b %d %Y %H:%M:%S", &tm_time);
    time_t t = mktime(&tm_time);
    struct timeval now = {.tv_sec = t};
    settimeofday(&now, NULL);
    printf("Time initialized from build: %s\n", asctime(&tm_time));
}

void write_message(char *message, float temp){
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S+01:00", &timeinfo); //+01:00 because of timezone
    sprintf(message, "%s,08,%.4f,123\n", timestamp, temp);
}
