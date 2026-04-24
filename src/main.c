#include "app_games.h"
#include "app_tilt_test.h"

#ifndef APP_GAMES
#define APP_GAMES 0
#endif

int main(void) {
#if APP_GAMES
  app_games_run();
#else
  app_tilt_test_run();
#endif

  return 0;
}
