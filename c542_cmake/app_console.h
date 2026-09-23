#ifndef APP_CONSOLE_H
#define APP_CONSOLE_H

/* Initialize after app_board_init; process at most one received byte per poll. */
int app_console_init(void);
void app_console_poll(void);

#endif
