GAME_OPTION("设置每件商品投标最大轮数（包括首轮投标），若达到该投标轮数仍有复数玩家投标额最高，则流标", 投标轮数, (ArithChecker<uint32_t>(1, 9, "轮数")), 2)
GAME_OPTION("每件商品投标时间（秒）", 投标时间, (ArithChecker<uint32_t>(10, 300, "投标时间（秒）")), 60)
GAME_OPTION("弃牌时间（秒）", 弃牌时间, (ArithChecker<uint32_t>(10, 3600, "弃牌时间（秒）")), 900)
GAME_OPTION("玩家初始金币数", 初始金币数, (ArithChecker<uint32_t>(100, 100000, "金币数")), 300)
GAME_OPTION("回合数", 回合数, (ArithChecker<uint32_t>(1, 10, "回合数")), 5)
GAME_OPTION("随机种子", 种子, (AnyArg("种子", "我是随便输入的一个字符串")), "")
