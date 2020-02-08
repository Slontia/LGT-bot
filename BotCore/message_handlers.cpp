#include "stdafx.h"

#include <sstream>
#include <tuple>
#include <optional>
#include <memory>
#include <optional>

#include "message_handlers.h"
#include "match.h"
#include "lgtbot.h"
#include "log.h"
#include "msg_checker.h"

using MetaUserFuncType = std::string(const UserID, const std::optional<GroupID>);
using MetaCommand = MsgCommand<MetaUserFuncType>;

extern const std::vector<std::shared_ptr<MetaCommand>> meta_cmds;

static const auto make_meta_command = [](std::string&& description, const auto& cb, auto&&... checkers) -> std::shared_ptr<MetaCommand>
{
  return MakeCommand<std::string(const UserID, const std::optional<GroupID>)>(std::move(description), cb, std::move(checkers)...);
};

static std::string help(const UserID uid, const std::optional<GroupID> gid)
{
  std::stringstream ss;
  ss << "[��ʹ�õ�Ԫָ��]";
  int i = 0;
  for (const std::shared_ptr<MetaCommand>& cmd : meta_cmds)
  {
    ss << std::endl << std::endl << "[" << (++i) << "] " << cmd->Info();
  }
  return ss.str();
}

static std::string show_gamelist(const UserID uid, const std::optional<GroupID> gid)
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

static std::string new_game(const UserID uid, const std::optional<GroupID> gid, const std::string& gamename)
{
  std::string err_msg;
  const auto it = g_game_handles.find(gamename);
  if (it == g_game_handles.end()) { err_msg = "������Ϸʧ�ܣ�δ֪����Ϸ��"; }
  else if (gid.has_value()) { err_msg = MatchManager::NewMatch(*it->second, uid, gid); }
  else { err_msg = MatchManager::NewMatch(*it->second, uid, gid); }
  return err_msg.empty() ? "������Ϸ�ɹ�" : "[����] " + err_msg;
}

static std::string start_game(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = MatchManager::StartGame(uid, gid);
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

static std::string leave(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = MatchManager::DeletePlayer(uid, gid);
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

static std::string join_private(const UserID uid, const std::optional<GroupID> gid, const MatchId match_id)
{
  std::string err_msg;
  if (gid.has_value()) { err_msg = "������Ϸʧ�ܣ���˽�Ų��м���˽����Ϸ����ȥ������ID�Լ��뵱ǰ������Ϸ"; }
  else { err_msg = MatchManager::AddPlayerToPrivateGame(match_id, uid); }
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

static std::string join_public(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg;
  if (!gid.has_value()) { err_msg = "����˽����Ϸ����ָ������ID"; }
  else { err_msg = MatchManager::AddPlayerToPublicGame(*gid, uid); }
  return err_msg.empty() ? "����ɹ�" : "[����] " + err_msg;
}

static std::string show_private_matches(const UserID uid, const std::optional<GroupID> gid)
{
  std::stringstream ss;
  uint64_t count = 0;
  MatchManager::ForEachMatch([&ss, &count](const std::shared_ptr<Match> match)
  {
    if (match->IsPrivate() && !match->is_started())
    {
      ++count;
      ss << std::endl << match->Name() << " - [����ID] " << match->host_uid() << " - [����ID] " << match->mid();
    }
  });
  if (ss.rdbuf()->in_avail() == 0) { return "��ǰ��δ��ʼ��˽�ܱ���"; }
  else { return "��" + std::to_string(count) + "����" + ss.str(); }
}

static const std::vector<std::shared_ptr<MetaCommand>> meta_cmds =
{
  make_meta_command("�鿴����", help, std::make_unique<VoidChecker>("#����")),
  make_meta_command("��ʾ��Ϸ�б�", show_gamelist, std::make_unique<VoidChecker>("#��Ϸ�б�")),
  make_meta_command("�ڵ�ǰ���佨��������Ϸ����˽��bot�Խ���˽����Ϸ", new_game, std::make_unique<VoidChecker>("#����Ϸ"), std::make_unique<AnyArg>("��Ϸ����", "ĳ��Ϸ��")),
  make_meta_command("������ʼ��Ϸ", start_game, std::make_unique<VoidChecker>("#��ʼ��Ϸ")),
  make_meta_command("����Ϸ��ʼǰ�˳���Ϸ", leave, std::make_unique<VoidChecker>("#�˳���Ϸ")),
  make_meta_command("���뵱ǰ����Ĺ�����Ϸ", join_public, std::make_unique<VoidChecker>("#������Ϸ")),
  make_meta_command("˽��bot�Լ���˽����Ϸ", join_private, std::make_unique<VoidChecker>("#������Ϸ"), std::make_unique<BasicChecker<MatchId>>("˽�ܱ������")),
  make_meta_command("�鿴��ǰ����δ��ʼ��˽�ܱ���", show_private_matches, std::make_unique<VoidChecker>("#˽����Ϸ�б�")),
};

std::string HandleMetaRequest(const UserID uid, const std::optional<GroupID> gid, MsgReader& reader)
{

  reader.Reset();
  for (const std::shared_ptr<MetaCommand>& cmd : meta_cmds)
  {
    if (std::optional<std::string> reply_msg = cmd->CallIfValid(reader, std::tuple{ uid, gid })) { return *reply_msg; }
  }
  return "[����] δԤ�ϵ�Ԫ����";
}