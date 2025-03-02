#ifndef UDP_CAST_TASK_H
#define UDP_CAST_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t udp_cast_task_handle; // Declare o handle da tarefa

void udp_cast_task(void *pvParameter);
void end_udp_cast_task();
void start_udp_cast_task();
extern char COM_IP[16];
extern int selected;

#endif // UDP_CAST_TASK_H
