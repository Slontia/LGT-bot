#include "stdafx.h"

#include <sstream>
#include <tuple>
#include <optional>

#include "message_iterator.h"
#include "message_handlers.h"
#include "match.h"
#include "lgtbot.h"

std::string show_gamelist(const UserID uid, const std::optional<GroupID> gid)
{
  std::stringstream ss;
  int i = 0;
  ss << "��Ϸ�б�";
  for (const auto&[name, _] : g_game_handles) { ss << std::endl << (++i) << '.' << name; }
  return ss.str();
}

std::string new_game(const UserID uid, const std::optional<GroupID> gid, const std::string& gamename, const bool for_public_match)
{
  std::string err_msg;
  if (g_game_handles.find(gamename) == g_game_handles.end()) { err_msg = "������Ϸʧ�ܣ�δ֪����Ϸ��"; }
  else if (gid.has_value()) { err_msg = match_manager.new_match(GROUP_MATCH, gamename, uid, gid); }
  else if (for_public_match) { err_msg = "������Ϸʧ�ܣ�����������Ϸ��֧��˽�Ž���������Ⱥ���������й�������"; }
  else { err_msg = match_manager.new_match(PRIVATE_MATCH, gamename, uid, uid); }
  return err_msg.empty() ? "������Ϸ�ɹ�" : "[����] " + err_msg;
}

std::string start_game(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = match_manager.StartGame(uid);
  return err_msg.empty() ? "��Ϸ��ʼ" : "[����] " + err_msg;
}

std::string leave(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = match_manager.DeletePlayer(uid);
  return err_msg.empty() ? "�˳��ɹ�" : "[����] " + err_msg;
}

std::string join(const UserID uid, const std::optional<GroupID> gid, std::optional<MatchId> match_id)
{
  std::string err_msg;
  if (!match_id.has_value())
  {
    if (!gid.has_value()) { err_msg = "��ָ������ID����������"; }
    else { match_id = match_manager.get_group_match_id(*gid); }
  }
  if (match_id.has_value()) { err_msg = match_manager.AddPlayer(*match_id, uid); }
  return err_msg.empty() ? "����ɹ�" : "[����] " + err_msg;
}
