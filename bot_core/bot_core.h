#ifdef ERRCODE_DEF

ERRCODE_DEF_V(EC_OK, 0)
ERRCODE_DEF(EC_UNEXPECTED_ERROR)
ERRCODE_DEF(EC_NOT_INIT)

// system error
ERRCODE_DEF_V(EC_DB_CONNECT_FAILED, 101)
ERRCODE_DEF(EC_DB_CONNECT_DENIED)
ERRCODE_DEF(EC_DB_INIT_FAILED)
ERRCODE_DEF(EC_DB_NOT_EXIST)
ERRCODE_DEF(EC_DB_ALREADY_CONNECTED)
ERRCODE_DEF(EC_DB_NOT_CONNECTED)
ERRCODE_DEF(EC_DB_RELEASE_GAME_FAILED)

// user error
ERRCODE_DEF_V(EC_MATCH_NOT_EXIST, 201)
ERRCODE_DEF(EC_MATCH_USER_ALREADY_IN_MATCH)
ERRCODE_DEF(EC_MATCH_USER_NOT_IN_MATCH)
ERRCODE_DEF(EC_MATCH_USER_ALREADY_IN_OTHER_MATCH)
ERRCODE_DEF(EC_MATCH_NOT_THIS_GROUP)
ERRCODE_DEF(EC_MATCH_GROUP_NOT_IN_MATCH)
ERRCODE_DEF(EC_MATCH_NOT_HOST)
ERRCODE_DEF(EC_MATCH_NEED_REQUEST_PUBLIC)
ERRCODE_DEF(EC_MATCH_NEED_REQUEST_PRIVATE)
ERRCODE_DEF(EC_MATCH_NEED_ID)
ERRCODE_DEF(EC_MATCH_ALREADY_BEGIN)
ERRCODE_DEF(EC_MATCH_NOT_BEGIN)
ERRCODE_DEF(EC_MATCH_IN_CONFIG)
ERRCODE_DEF(EC_MATCH_NOT_IN_CONFIG)
ERRCODE_DEF(EC_MATCH_ACHIEVE_MAX_PLAYER)
ERRCODE_DEF(EC_MATCH_UNEXPECTED_CONFIG)
ERRCODE_DEF(EC_MATCH_ALREADY_OVER)
ERRCODE_DEF(EC_MATCH_FORBID_OPERATION)

ERRCODE_DEF_V(EC_REQUEST_EMPTY, 301)
ERRCODE_DEF(EC_REQUEST_NOT_ADMIN)
ERRCODE_DEF(EC_REQUEST_NOT_FOUND)
ERRCODE_DEF(EC_REQUEST_UNKNOWN_GAME)

ERRCODE_DEF_V(EC_GAME_ALREADY_RELEASE, 401)

ERRCODE_DEF_V(EC_GAME_REQUEST_OK, 10000)
ERRCODE_DEF(EC_GAME_REQUEST_CHECKOUT)
ERRCODE_DEF(EC_GAME_REQUEST_FAILED)
ERRCODE_DEF(EC_GAME_REQUEST_NOT_FOUND)
ERRCODE_DEF(EC_GAME_REQUEST_UNKNOWN)

#endif

#ifndef BOT_CORE_H
#define BOT_CORE_H
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

class GameHandle;
class MsgSenderForBot;

#define DEFINE_ID(idname) \
struct idname \
{ \
 public: \
  constexpr idname() : id_(UINT64_MAX) {} \
  constexpr idname(const uint64_t id) : id_(id) {} \
  constexpr idname(const idname&) = default; \
  constexpr idname& operator=(const uint64_t id) \
  { \
    id_ = id; \
    return *this; \
  } \
  constexpr idname& operator=(const idname&) = default; \
  operator uint64_t() const { return id_; } \
  auto operator<=>(const uint64_t id) const { return id_ <=> id; } \
  auto operator<=>(const idname&) const = default; \
  idname& operator++() \
  { \
    id_ += 1; \
    return *this; \
  } \
  template <typename Outputter> \
  friend auto& operator<<(Outputter& outputter, const idname id) { return outputter << id.id_; } \
  template <typename Inputter> \
  friend auto& operator>>(Inputter& inputter, idname& id) { return inputter >> id.id_; } \
  bool IsValid() const { return UINT64_MAX != id_; } \
\
 private: \
  uint64_t id_; \
}

DEFINE_ID(UserID);
DEFINE_ID(GroupID);
DEFINE_ID(MatchID);
DEFINE_ID(ComputerID);
DEFINE_ID(PlayerID);

extern "C" {

enum Target { TO_USER, TO_GROUP };
typedef MsgSenderForBot* (*NEW_MSG_SENDER_CALLBACK)(const Target, const uint64_t);
typedef void (*DELETE_MSG_SENDER_CALLBACK)(MsgSenderForBot* const msg_sender);

enum ErrCode {
#define ERRCODE_DEF_V(errcode, value) errcode = value,
#define ERRCODE_DEF(errcode) errcode,
#include "bot_core.h"
#undef ERRCODE_DEF_V
#undef ERRCODE_DEF
};

inline const char* errcode2str(const ErrCode errcode)
{
    switch (errcode) {
#define ERRCODE_DEF(errcode) \
    case errcode:            \
        return #errcode;
#define ERRCODE_DEF_V(errcode, value) ERRCODE_DEF(errcode)
#include "bot_core.h"
#undef ERRCODE_DEF_V
#undef ERRCODE_DEF
    default:
        return "Unknown Error Code";
    }
}

#ifdef _WIN32
#define DLLEXPORT(type) __declspec(dllexport) type __cdecl
#else
#define DLLEXPORT(type) type
#endif

class BOT_API
{
   public:
    static DLLEXPORT(void*) Init(const uint64_t this_uid, const NEW_MSG_SENDER_CALLBACK new_msg_sender_cb,
                                 const DELETE_MSG_SENDER_CALLBACK delete_msg_sender_cb, const char* const game_path,
                                 const uint64_t* const admins, const uint64_t admin_count);
    static DLLEXPORT(void) Release(void* bot);
    static DLLEXPORT(ErrCode) HandlePrivateRequest(void* bot, const uint64_t uid, const char* const msg);
    static DLLEXPORT(ErrCode)
            HandlePublicRequest(void* bot, const uint64_t gid, const uint64_t uid, const char* const msg);
#ifdef WITH_MYSQL
    static DLLEXPORT(ErrCode) ConnectDatabase(void* bot, const char* const addr, const char* const user,
                                              const char* const passwd, const char* const db_name, const char** errmsg);
#endif
};

#undef DLLEXPORT

}

#endif
