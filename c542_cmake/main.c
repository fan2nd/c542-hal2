#include "main.h"
#include "app_board.h"
#include "app_console.h"

int main(void)
{
  if ((mx_system_init() != SYSTEM_OK) ||
      (app_board_init() != 0) || (app_console_init() != 0))
  {
    return -1;
  }

  while (1)
  {
    app_board_poll();
    app_console_poll();
  }
}
