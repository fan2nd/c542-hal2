#ifndef APP_BOARD_H
#define APP_BOARD_H

#include <stdbool.h>

int app_board_init(void);
void app_board_poll(void);
/* Require both the sampled and debounced button states to be released. */
bool app_board_can_sleep(void);
/* Caller must quiesce UART/DMA first. Returns after a debounced USER press. */
void app_board_sleep(void);

#endif
