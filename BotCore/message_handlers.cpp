#include "stdafx.h"
#include "message_handlers.h"
#include "match.h"
#include "log.h"
#include "msg_checker.h"
#include "db_manager.h"

using MetaUserFuncType = std::string(const UserID, const std::optional<GroupID>);
using MetaCommand = MsgCommand<MetaUserFuncType>;

extern const std::vector<std::shared_ptr<MetaCommand>> meta_cmds;

static const auto make_command = [](std::string&& description, const auto& cb, auto&&... checkers) -> std::shared_ptr<MetaCommand>
{
  return MakeCommand<std::string(const UserID, const std::optional<GroupID>)>(std::move(description), cb, std::move(checkers)...);
};

static std::string help(const UserID uid, const std::optional<GroupID> gid, const std::vector<std::shared_ptr<MetaCommand>>& cmds, const std::string& type)
{
  std::stringstream ss;
  ss << "[��ʹ�õ�" << type << "ָ��]";
  int i = 0;
  for (const std::shared_ptr<MetaCommand>& cmd : cmds)
  {
    ss << std::endl << "[" << (++i) << "] " << cmd->Info();
  }
  return ss.str();
}

static std::string HandleRequest(const UserID uid, const std::optional<GroupID> gid, MsgReader& reader, const std::vector<std::shared_ptr<MetaCommand>>& cmds, const std::string& type)
{
  reader.Reset();
  for (const std::shared_ptr<MetaCommand>& cmd : cmds)
  {
    if (std::optional<std::string> reply_msg = cmd->CallIfValid(reader, std::tuple{ uid, gid })) { return *reply_msg; }
  }
  return "[����] δԤ�ϵ�" + type + "ָ��";
}

static std::string show_gamelist(const UserID uid, const std::optional<GroupID> gid)
{
  std::stringstream ss;
  int i = 0;
  ss << "��Ϸ�б�";
  for (const auto& [name, _] : g_game_handles)
  {
    ss << std::endl << (++i) << ". " << name;
  }
  return ss.str();
}

template <bool skip_config>
static std::string new_game(const UserID uid, const std::optional<GroupID> gid, const std::string& gamename)
{
  const auto it = g_game_handles.find(gamename);
  std::string err_msg = (it != g_game_handles.end()) ? MatchManager::NewMatch(*it->second, uid, gid, skip_config) : "������Ϸʧ�ܣ�δ֪����Ϸ��";
  return err_msg.empty() ? "������Ϸ�ɹ�" : "[����] " + err_msg;
}

static std::string config_over(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = MatchManager::ConfigOver(uid, gid);
  return err_msg.empty() ? "" : "[����] " + err_msg;
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
  std::string err_msg = !gid.has_value() ? MatchManager::AddPlayerToPrivateGame(match_id, uid) : "������Ϸʧ�ܣ���˽�Ų��м���˽����Ϸ����ȥ������ID�Լ��뵱ǰ������Ϸ";
  return err_msg.empty() ? "" : "[����] " + err_msg;
}

static std::string join_public(const UserID uid, const std::optional<GroupID> gid)
{
  std::string err_msg = gid.has_value() ? MatchManager::AddPlayerToPublicGame(*gid, uid) : "����˽����Ϸ����ָ������ID";
  return err_msg.empty() ? "����ɹ�" : "[����] " + err_msg;
}

static std::string show_private_matches(const UserID uid, const std::optional<GroupID> gid)
{
  std::stringstream ss;
  uint64_t count = 0;
  MatchManager::ForEachMatch([&ss, &count](const std::shared_ptr<Match> match)
  {
    if (match->IsPrivate() && match->state() == Match::State::NOT_STARTED)
    {
      ++count;
      ss << std::endl << match->game_handle().name_ << " - [����ID] " << match->host_uid() << " - [����ID] " << match->mid();
    }
  });
  if (ss.rdbuf()->in_avail() == 0) { return "��ǰ��δ��ʼ��˽�ܱ���"; }
  else { return "��" + std::to_string(count) + "����" + ss.str(); }
}

static std::string show_match_status(const UserID uid, const std::optional<GroupID> gid)
{
  // TODO: make it thread safe
  std::stringstream ss;
  std::shared_ptr<Match> match = MatchManager::GetMatch(uid, gid);
  if (!match && gid.has_value()) { match = MatchManager::GetMatchWithGroupID(*gid); }
  if (!match) { return "[����] ��δ������Ϸ����÷���δ������Ϸ"; }
  ss << "��Ϸ���ƣ�" << match->game_handle().name_ << std::endl;
  ss << "������Ϣ��" << match->OptionInfo() << std::endl;
  ss << "��Ϸ״̬��" << (match->state() == Match::State::IN_CONFIGURING ? "������" :
                       match->state() == Match::State::NOT_STARTED ? "δ��ʼ" : "�ѿ�ʼ") << std::endl;
  ss << "����ţ�";
  if (match->gid().has_value()) { ss << *gid << std::endl; }
  else { ss << "˽����Ϸ" << std::endl; }
  ss << "�ɲμ�������" << match->game_handle().min_player_;
  if (const uint64_t max_player = match->game_handle().max_player_; max_player > match->game_handle().min_player_) { ss << "~" << max_player; }
  else if (max_player == 0) { ss << "+"; }
  ss << "��" << std::endl;
  ss << "������" << match->host_uid() << std::endl;
  const std::set<uint64_t>& ready_uid_set = match->ready_uid_set();
  ss << "�Ѳμ���ң�" << ready_uid_set.size() << "��";
  for (const uint64_t uid : ready_uid_set) { ss << std::endl << uid; }
  return ss.str();
}

static std::string show_rule(const UserID uid, const std::optional<GroupID> gid, const std::string& gamename)
{
  std::stringstream ss;
  const auto it = g_game_handles.find(gamename);
  if (it == g_game_handles.end()) { return "[����] δ֪����Ϸ��"; }
  ss << "�ɲμ�������" << it->second->min_player_;
  if (const uint64_t max_player = it->second->max_player_; max_player > it->second->min_player_) { ss << "~" << max_player; }
  else if (max_player == 0) { ss << "+"; }
  ss << "��" << std::endl;
  ss << "��ϸ����" << std::endl;
  ss << it->second->rule_;
  return ss.str();
}

static std::string show_profile(const UserID uid, const std::optional<GroupID> gid)
{
  if (const std::unique_ptr<DBManager>& db_manager = DBManager::GetDBManager(); db_manager == nullptr) { return "[����] δ�������ݿ�"; }
  else { return db_manager->GetUserProfit(uid); }
}

static const std::vector<std::shared_ptr<MetaCommand>> meta_cmds =
{
  make_command("�鿴����", [](const UserID uid, const std::optional<GroupID> gid) { return help(uid, gid, meta_cmds, "Ԫ"); }, std::make_unique<VoidChecker>("#����")),
  make_command("�鿴������Ϣ", show_profile, std::make_unique<VoidChecker>("#������Ϣ")),
  make_command("�鿴��Ϸ�б�", show_gamelist, std::make_unique<VoidChecker>("#��Ϸ�б�")),
  make_command("�鿴��Ϸ����", show_rule, std::make_unique<VoidChecker>("#����"), std::make_unique<AnyArg>("��Ϸ����", "ĳ��Ϸ��")),
  make_command("�鿴��ǰ����δ��ʼ��˽�ܱ���", show_private_matches, std::make_unique<VoidChecker>("#˽����Ϸ�б�")),
  make_command("�鿴�Ѽ��룬��÷������ڽ��еı�����Ϣ", show_match_status, std::make_unique<VoidChecker>("#��Ϸ��Ϣ")),

  make_command("�ڵ�ǰ���佨��������Ϸ����˽��bot�Խ���˽����Ϸ", new_game<true>, std::make_unique<VoidChecker>("#����Ϸ"), std::make_unique<AnyArg>("��Ϸ����", "ĳ��Ϸ��")),
  make_command("�ڵ�ǰ���佨��������Ϸ����˽��bot�Խ���˽����Ϸ����������Ϸ����������", new_game<false>, std::make_unique<VoidChecker>("#��������Ϸ"), std::make_unique<AnyArg>("��Ϸ����", "ĳ��Ϸ��")),
  make_command("�����Ϸ�������ú�������ҽ��뷿��", config_over, std::make_unique<VoidChecker>("#�������")),
  make_command("������ʼ��Ϸ", start_game, std::make_unique<VoidChecker>("#��ʼ��Ϸ")),
  make_command("���뵱ǰ����Ĺ�����Ϸ", join_public, std::make_unique<VoidChecker>("#������Ϸ")),
  make_command("˽��bot�Լ���˽����Ϸ", join_private, std::make_unique<VoidChecker>("#������Ϸ"), std::make_unique<BasicChecker<MatchId>>("˽�ܱ������")),
  make_command("����Ϸ��ʼǰ�˳���Ϸ", leave, std::make_unique<VoidChecker>("#�˳���Ϸ")),
};

static std::string release_game(const UserID uid, const std::optional<GroupID> gid, const std::string& gamename)
{
  if (const auto it = g_game_handles.find(gamename); it == g_game_handles.end()) { return "[����] δ֪����Ϸ��"; }
  else if (std::optional<uint64_t> game_id = it->second->game_id_.load(); game_id.has_value()) { return "[����] ��Ϸ�ѷ�����IDΪ" + std::to_string(*game_id); }
  else if (const std::unique_ptr<DBManager>& db_manager = DBManager::GetDBManager(); db_manager == nullptr) { return "[����] δ�������ݿ�"; }
  else if (game_id = db_manager->ReleaseGame(gamename); !game_id.has_value()) { return "[����] ����ʧ�ܣ���鿴������־"; }
  else
  {
    it->second->game_id_.store(game_id);
    return "�����ɹ�����Ϸ\'" + gamename + "\'��IDΪ" + std::to_string(*game_id);
  }
}

static const std::vector<std::shared_ptr<MetaCommand>> admin_cmds =
{
  make_command("�鿴����", [](const UserID uid, const std::optional<GroupID> gid) { return help(uid, gid, admin_cmds, "����"); }, std::make_unique<VoidChecker>("%����")),
  make_command("������Ϸ��д����Ϸ��Ϣ�����ݿ�", release_game, std::make_unique<VoidChecker>("%������Ϸ"), std::make_unique<AnyArg>("��Ϸ����", "ĳ��Ϸ��"))
};

std::string HandleMetaRequest(const UserID uid, const std::optional<GroupID> gid, MsgReader& reader)
{
  return HandleRequest(uid, gid, reader, meta_cmds, "Ԫ");
}

std::string HandleAdminRequest(const UserID uid, const std::optional<GroupID> gid, MsgReader& reader)
{
  return HandleRequest(uid, gid, reader, admin_cmds, "����");
}