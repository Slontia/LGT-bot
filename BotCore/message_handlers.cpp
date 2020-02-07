#include "stdafx.h"

#include <sstream>
#include <tuple>
#include <optional>

#include "message_handlers.h"
#include "match.h"
#include "lgtbot.h"
#include "log.h"

std::string show_gamelist(const UserID uid, const std::optional<GroupID> gid)
{
  std::stringstream ss;
  int i = 0;
  ss << "��Ϸ�б�";
  for (const auto& [name, _] : g_game_handles)
  {
    ss << std::endl << (++i) << ".\t" << name;
  }
  return ss.str();
}

std::string new_game(const UserID uid, const std::optional<GroupID> gid, const std::string& gamename)
{
  std::string err_msg;
  const auto it = g_game_handles.find(gamename);
  if (it == g_game_handles.end()) { err_msg = "������Ϸʧ�ܣ�δ֪����Ϸ��"; }
  else if (gid.has_value()) { err_msg = MatchManager::NewMatch(*it->second, uid, gid); }
  else { err_msg = MatchManager::NewMatch(*it->second, uid, gid); }
  return err_msg.empty() ? "������Ϸ�ɹ�" : "[����] " + err_msg;
}

std::string start_game(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = MatchManager::StartGame(uid, gid);
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

std::string leave(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = MatchManager::DeletePlayer(uid, gid);
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

std::string join_private(const UserID uid, const std::optional<GroupID> gid, const MatchId match_id)
{
  std::string err_msg;
  if (gid.has_value()) { err_msg = "������Ϸʧ�ܣ���˽�Ų��м���˽����Ϸ����ȥ������ID�Լ��뵱ǰ������Ϸ"; }
  else { err_msg = MatchManager::AddPlayerToPrivateGame(match_id, uid); }
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

std::string join_public(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg;
  if (!gid.has_value()) { err_msg = "����˽����Ϸ����ָ������ID"; }
  else { err_msg = MatchManager::AddPlayerToPublicGame(*gid, uid); }
  return err_msg.empty() ? "����ɹ�" : "[����] " + err_msg;
}