cmake_minimum_required(VERSION 3.0)
project(BotCore LANGUAGES CXX C)

set(CMAKE_SYSTEM_VERSION 1)

# Default build with Debug
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(EXECUTABLE_OUTPUT_PATH  ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR})

option(WITH_GCOV "bulid with gcov" FALSE)
if (WITH_GCOV)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -fprofile-arcs -ftest-coverage")
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov")
endif()

option(WITH_ASAN "build with address sanitizier" FALSE)
if (WITH_ASAN)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
endif()

option(WITH_CORE "build with bot core" TRUE)
if (WITH_CORE)
  add_subdirectory(bot_core)
endif()

option(WITH_SIMULATOR "build with bot simulator" TRUE)
if (WITH_SIMULATOR)
  add_subdirectory(bot_simulator)
endif()

if (WITH_TEST)
  add_subdirectory(game_util) # only build for unittest
  add_subdirectory(utility) # only build for unittest
endif()

option(WITH_GAMES "build with games" TRUE)
if (WITH_GAMES)
  add_subdirectory(games)
endif()
