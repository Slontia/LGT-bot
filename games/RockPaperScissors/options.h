#include "msg_checker.h"
#ifdef GAME_OPTION
GAME_OPTION(win_count, (std::make_unique<ArithChecker<uint32_t, 1, 9>>("ʤ������")), 3)
#endif