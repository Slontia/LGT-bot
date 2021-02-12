GAME_OPTION("每回合最长时间（秒）", 局时, (std::make_unique<ArithChecker<uint32_t, 10, 3600>>("局时（秒）")), 300)
GAME_OPTION("开始淘汰的回合数", 淘汰回合, (std::make_unique<ArithChecker<uint32_t, 1, 100>>("回合数")), 6)
GAME_OPTION("达到开始淘汰的回合数之后，每次淘汰间隔的回合数", 淘汰间隔, (std::make_unique<ArithChecker<uint32_t, 1, 5>>("回合数")), 2)
GAME_OPTION("可预测的最大数字（可预测的最小数字为1）", 最大数字, (std::make_unique<ArithChecker<uint32_t, 1, 9999>>("数字")), 99)
