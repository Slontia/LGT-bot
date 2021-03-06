cmake_minimum_required(VERSION 3.0)
project(BotCore LANGUAGES CXX C)

set(CMAKE_SYSTEM_VERSION 1)

# Default build with Debug
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -latomic")

list(APPEND SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/bot_core.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/db_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/match.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/message_handlers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/load_game_modules.cpp
)
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
  list(APPEND SOURCE_FLIES ${CMAKE_CURRENT_SOURCE_DIR}/dllmain.cpp)
endif()

find_package(Threads REQUIRED)
list(APPEND THIRD_PARTIES Threads::Threads)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  list(APPEND THIRD_PARTIES
    dl      # load shared libraries dynamicly on linux
    atomic
  )
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)

find_package(gflags REQUIRED)
list(APPEND THIRD_PARTIES gflags)

option(WITH_GLOG "build with glog" TRUE)
if (WITH_GLOG)
  find_package(glog REQUIRED)
  list(APPEND THIRD_PARTIES glog)
endif()

# find mysql-connector-c++
option(WITH_MYSQL "build with mysql connection" TRUE)
if (WITH_MYSQL)
  option(MYSQL_CONNECTOR_PATH "path of mysql-connector-c++ headers")
  option(MYSQL_CONNECTOR_LIBRARIES "path of mysql-connector-c++ libraries")
  if (NOT MYSQL_CONNECTOR_PATH OR NOT MYSQL_CONNECTOR_LIBRARIES)
    message(FATAL_ERROR "MYSQL_CONNECTOR_PATH and MYSQL_CONNECTOR_LIBRARIES is required")
  else()
    include_directories(${MYSQL_CONNECTOR_PATH})
    list(APPEND THIRD_PARTIES ${MYSQL_CONNECTOR_LIBRARIES})
  endif()
endif()

add_library(bot_core_static STATIC ${SOURCE_FILES})
add_library(bot_core SHARED ${SOURCE_FILES})
target_link_libraries(bot_core_static ${THIRD_PARTIES})

option(WITH_TEST "build with unittest" TRUE)
if (WITH_TEST)
  enable_testing()
  find_package(GTest REQUIRED)
  list(APPEND THIRD_PARTIES GTest::GTest GTest::Main)
  add_executable(test_bot test_bot.cc)
  target_link_libraries(test_bot ${THIRD_PARTIES} bot_core_static)
endif()

