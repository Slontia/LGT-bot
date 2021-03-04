#pragma once
#include "util.h"
#include "bot_core.h"
#include "utility/msg_checker.h"
#include "utility/msg_sender.h"
#include <type_traits>

using MetaUserFuncType = ErrCode(BotCtx* const, const UserID, const std::optional<GroupID>, const replier_t);
using MetaCommand = MsgCommand<MetaUserFuncType>;

extern const std::vector<std::shared_ptr<MetaCommand>> meta_cmds;
extern const std::vector<std::shared_ptr<MetaCommand>> admin_cmds;

ErrCode HandleRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid, MsgReader& reader,
   const replier_t reply, const std::vector<std::shared_ptr<MetaCommand>>& cmds);

template <UniRef<MsgReader> ReaderRef, typename Reply>
ErrCode HandleMetaRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
    ReaderRef&& reader, Reply& reply)
{
	const auto ret = HandleRequest(bot, uid, gid, reader, std::forward<Reply>(reply), meta_cmds);
  if (ret == EC_REQUEST_NOT_FOUND)
  {
    reply() << "[错误] 未预料的元指令，您可以通过\"#帮助\"查看所有支持的元指令";
  }
  return ret;
}

template <UniRef<MsgReader> ReaderRef, typename Reply>
ErrCode HandleAdminRequest(BotCtx& bot, const UserID uid, const std::optional<GroupID> gid,
    ReaderRef&& reader, Reply& reply)
{
	const auto ret = HandleRequest(bot, uid, gid, reader, std::forward<Reply>(reply), admin_cmds);
  if (ret == EC_REQUEST_NOT_FOUND)
  {
    reply() << "[错误] 未预料的管理指令，您可以通过\"%帮助\"查看所有支持的管理指令";
  }
  return ret;
}
