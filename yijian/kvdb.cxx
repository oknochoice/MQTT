#include "kvdb.h"
#include <system_error>
#include <leveldb/write_batch.h>
#include "protofiles/chat_message.pb.h"

using yijian::buffer;

enum Session_ID : int32_t {
  regist_login_connect = -1,
  logout_disconnect = -2
};

kvdb::kvdb(std::string & path) {
  YILOG_TRACE ("func: {}", __func__);
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, path, &db_);

  if (!status.ok()) {
    throw std::system_error(std::error_code(50000, 
          std::generic_category()),
       "open db failure");
  }

  create_client([&](Buffer_SP sp){
    YILOG_TRACE ("net callback");
    dispatch(sp->datatype(), sp);
  });
}

kvdb::~kvdb() {
  YILOG_TRACE ("func: {}", __func__);
  delete db_;
}

// common
leveldb::Status kvdb::put(const leveldb::Slice & key, 
    const leveldb::Slice & value) {
  YILOG_TRACE ("func: {}", __func__);
  return db_->Put(leveldb::WriteOptions(), key, value);
}
leveldb::Status kvdb::get(const leveldb::Slice & key, 
    std::string & value) {
  YILOG_TRACE ("func: {}", __func__);
  return db_->Get(leveldb::ReadOptions(), key, &value);
}

std::string kvdb::userKey(const std::string & userid) {
  YILOG_TRACE ("func: {}", __func__);
  return "u_" + userid;
}
std::string kvdb::nodeKey(const std::string & tonodeid) {
  YILOG_TRACE ("func: {}", __func__);
  return "n_" + tonodeid;
}
std::string kvdb::msgKey(const std::string & tonodeid,
    const std::string & incrementid) {
  YILOG_TRACE ("func: {}", __func__);
  return "m_" + tonodeid + "_" + incrementid;
}
std::string kvdb::msgKey(const std::string & tonodeid,
    const int32_t incrementid) {
  return msgKey(tonodeid, std::to_string(incrementid));
}
std::string kvdb::userPhoneKey(const std::string & countrycode,
    const std::string phoneno) {
  YILOG_TRACE ("func: {}", __func__);
  return "p_" + countrycode + "_" + phoneno;
}
std::string kvdb::errorKey(const std::string & userid,
    const std::string & nth) {
  YILOG_TRACE ("func: {}", __func__);
  return "e_" + userid + "_" + nth;
}
std::string kvdb::talklistKey(const std::string & userid) {
  YILOG_TRACE ("func: {}", __func__);
  return "t_" + userid;
}


std::string kvdb::signupKey() {
  YILOG_TRACE ("func: {}", __func__);
  return "signup_kvdb";
}
std::string kvdb::loginKey() {
  YILOG_TRACE ("func: {}", __func__);
  return "login_kvdb";
}
std::string kvdb::logoutKey() {
  YILOG_TRACE ("func: {}", __func__);
  return "logout_kvdb";
}
std::string kvdb::connectKey() {
  YILOG_TRACE ("func: {}", __func__);
  return "connect_kvdb";
}
std::string kvdb::disconnectKey() {
  YILOG_TRACE ("func: {}", __func__);
  return "disconnect_kvdb";
}


/*
 * current 
 *
 * */
void kvdb::set_current_userid(const std::string & userid) {
  YILOG_TRACE ("func: {}", __func__);
  if (likely(!put("current_user", userid).ok())) {
    throw std::system_error(std::error_code(50003,
          std::generic_category()),
        "set current user id failure");
  }
}
std::string kvdb::get_current_userid() {
  YILOG_TRACE ("func: {}", __func__);
  std::string userid;
  if (likely(!get("current_user", userid).ok())) {
    throw std::system_error(std::error_code(50004,
          std::generic_category()),
        "get current user id failure");
  }
  return userid;
}
int32_t kvdb::get_current_error_maxth() {
  YILOG_TRACE ("func: {}", __func__);
  auto key = errorKey(get_current_userid(), 0);
  std::string error_data;
  if (get(key, error_data).ok()) {
    chat::ErrorNth nth;
    nth.ParseFromString(error_data);
    return nth.maxnth();
  }else {
    return 1;
  }
}
void kvdb::set_current_error(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  chat::ErrorNth nth;
  nth.set_maxnth(get_current_error_maxth() + 1);
  std::string key = errorKey(get_current_userid(),
      std::to_string(get_current_error_maxth()));
  leveldb::WriteBatch batch;
  leveldb::Slice error(sp->data(), sp->data_size());
  batch.Put(key, error);
  batch.Put(errorKey(get_current_userid(), 0), 
      nth.SerializeAsString());
  auto status = db_->Write(leveldb::WriteOptions(), &batch);
  if (!status.ok()) {
    throw std::system_error(std::error_code(50005,
          std::generic_category()),
        "set error msg failure");
  }
}

/*
 * user
 *
 * */
void kvdb::putUser(const std::string & userid, 
      const std::string & countrycode,
      const std::string & phoneno,
    const leveldb::Slice & user) {
  YILOG_TRACE ("func: {}", __func__);
  if (userid.empty() || countrycode.empty() || phoneno.empty()) {
    throw std::system_error(std::error_code(50001,
          std::generic_category()),
        "parameter is empty");
  }
  auto userkey = userKey(userid);
  auto phonekey = userPhoneKey(countrycode, phoneno);
  leveldb::WriteBatch batch;
  batch.Put(phonekey, userid);
  batch.Put(userkey, user);
  auto status = db_->Write(leveldb::WriteOptions(), &batch);
  if (unlikely(!status.ok())) {
    throw std::system_error(std::error_code(50006,
          std::generic_category()),
        "put user failure");
  }
}

void kvdb::getUser(const std::string & id, 
    std::string & user) {
  YILOG_TRACE ("func: {}", __func__);
  auto key = userKey(id);
  auto status = get(key, user);
  if (unlikely(!status.ok())) {
    throw std::system_error(std::error_code(50007,
          std::generic_category()),
        "get user failure");
  }
}

void kvdb::getUser(const std::string & countrycode,
    const std::string & phoneno,
    std::string & user) {
  YILOG_TRACE ("func: {}", __func__);
  auto key = userPhoneKey(countrycode, phoneno);
  std::string userid;
  get(key, userid);
  getUser(userid, user);
}

// message
void kvdb::putTalklist(const std::string & userid, 
    const leveldb::Slice & talklist) {
  YILOG_TRACE ("func: {}", __func__);
  auto key = talklistKey(userid);
  auto status = put(key, talklist);
  if (unlikely(!status.ok())) {
    throw std::system_error(std::error_code(50008,
          std::generic_category()),
        "put talklist failure");
  }
}
void kvdb::getTalklist(const std::string & userid, 
    std::string & talklist) {
  YILOG_TRACE ("func: {}", __func__);
  auto key = talklistKey(userid);
  auto status = get(key, talklist);
  if (unlikely(!status.ok())) {
    throw std::system_error(std::error_code(50009,
          std::generic_category()),
        "get talklist failure");
  }
}
void kvdb::putMsg(const std::string & msgNode,
    const int32_t & incrementid,
    const leveldb::Slice & msg) {
  YILOG_TRACE ("func: {}", __func__);
  if (msgNode.empty() || 0 == incrementid) {
    throw std::system_error(std::error_code(50002,
          std::generic_category()),
        "msg miss tonodeid or incrementid");
  }
  auto key = msgKey(msgNode, incrementid);
  auto status = put(key, msg);
  if (unlikely(!status.ok())) {
    throw std::system_error(std::error_code(50040,
          std::generic_category()),
        "put message failure");
  }
}
void kvdb::getMsg(const std::string & msgNode,
    const int32_t & incrementid,
    std::string & msg) {
  YILOG_TRACE ("func: {}", __func__);

  if (msgNode.empty() || 0 >= incrementid) {
    throw std::system_error(std::error_code(50003,
          std::generic_category()),
        "msg miss tonodeid or incrementid");
  }

  std::string value;
  auto key = msgKey(msgNode, incrementid);
  auto status = get(key, msg);
  if (unlikely(!status.ok())) {
    throw std::system_error(std::error_code(50041,
          std::generic_category()),
        "get message failure");
  }

}


/*
 * network regist login connect
 *
 * */ 
void kvdb::registUser(const std::string & phoneno,
                  const std::string & countrycode,
                  const std::string & password,
                  const std::string & verifycode,
                  const std::string & nickname,
                  CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  // check
#warning phone num verify
  if (unlikely(phoneno.empty())) {
    throw std::system_error(std::error_code(50010,
          std::generic_category()),
        "phoneno is not valid.");
  }
#warning country code verify
  if (unlikely(countrycode.empty())) {
    throw std::system_error(std::error_code(50011,
          std::generic_category()),
        "countrycode is not valid.");
  }
  if (unlikely(password.size() < 6)) {
    throw std::system_error(std::error_code(50012,
          std::generic_category()),
        "password length at least 6");
  }
  if (unlikely(verifycode.empty())) {
    throw std::system_error(std::error_code(50013,
          std::generic_category()),
        "verifycode is no valid");
  }
  if (unlikely(nickname.empty())) {
    throw std::system_error(std::error_code(50014,
          std::generic_category()),
        "nickname is empty");
  }
  // add func to map
  put_map(Session_ID::regist_login_connect, 
      std::forward<CB_Func>(func));
  // send regist
  chat::Register enroll;
  enroll.set_phoneno(phoneno);
  enroll.set_countrycode(countrycode);
#warning need encrypt
  enroll.set_password(password);
  enroll.set_nickname(nickname);
  enroll.set_verifycode(verifycode);
  client_send(buffer::Buffer(enroll), nullptr);

}

void kvdb::login(const std::string & phoneno,
             const std::string & countrycode,
             const std::string & password,
             const int os,
             const std::string & devicemodel,
             const std::string & uuid,
             CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  // add func to map
  put_map(Session_ID::regist_login_connect, 
      std::forward<CB_Func>(func));
  // send login
  chat::Login login;
  login.set_phoneno(phoneno);
  login.set_countrycode(countrycode);
#warning need encrypt
  login.set_password(password);
  auto device = login.mutable_device();
  device->set_os(static_cast<chat::Device::OperatingSystem>(os));
  device->set_devicemodel(devicemodel);
  device->set_uuid(uuid);
  client_send(buffer::Buffer(login), nullptr);
}
void kvdb::logout(const std::string & userid,
              const std::string & uuid,
              CB_Func && func) {
  // add func to map
  put_map(Session_ID::logout_disconnect, 
      std::forward<CB_Func>(func));
  // send logout
  chat::Logout logout;
  logout.set_userid(userid);
  logout.set_uuid(uuid);
  client_send(buffer::Buffer(logout), nullptr);
}
void kvdb::connect(const std::string & userid,
               const std::string & uuid,
               const bool isrecivenoti,
               const std::string & OSversion,
               const std::string & appversion,
               CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  // add func to map
  put_map(Session_ID::regist_login_connect, 
      std::forward<CB_Func>(func));
  // send connect
  chat::ClientConnect connect;
  connect.set_userid(userid);
  connect.set_uuid(uuid);
  connect.set_isrecivenoti(isrecivenoti);
  connect.set_osversion(OSversion);
  connect.set_clientversion(KVDBVersion);
  connect.set_appversion(appversion);
  client_send(buffer::Buffer(connect), nullptr);
}

void kvdb::disconnect(const std::string & userid,
               const std::string & uuid,
               CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  // add func to map
  put_map(Session_ID::logout_disconnect, 
      std::forward<CB_Func>(func));
  // send connect
  chat::ClientDisConnect disconnect;
  disconnect.set_userid(userid);
  disconnect.set_uuid(uuid);
  client_send(buffer::Buffer(disconnect), nullptr);
}

/*
 * network friend
 *
 * */
void kvdb::addfriend(const std::string & inviteeid,
                 const std::string & msg,
                 CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::AddFriend add;
  add.set_inviterid(get_current_userid());
  add.set_inviteeid(inviteeid);
  add.set_msg(msg);
  put_map_send(std::forward<CB_Func>(func), buffer::Buffer(add));
}

void kvdb::addfriendAuthorize(const std::string & inviterid,
                        int isAgree,
                        CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::AddFriendAuthorize authorize;
  authorize.set_inviterid(inviterid);
  authorize.set_inviteeid(get_current_userid());
  authorize.set_isagree(static_cast<chat::IsAgree>(isAgree));
  put_map_send(std::forward<CB_Func>(func), 
      buffer::Buffer(authorize));
}

/*
 * network group
 *
 * */
void kvdb::creategroup(const std::string & groupname,
    const std::vector<std::string> membersid,
    CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::CreateGroup group;
  group.set_userid(get_current_userid());
  group.set_nickname(groupname);
  for (auto & memberid: membersid) {
    group.add_membersid(memberid);
  }
  put_map_send(std::forward<CB_Func>(func), 
      buffer::Buffer(group));
}
void kvdb::addmembers2group(const std::string & groupid,
    const std::vector<std::string> membersid,
    CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::GroupAddMember add;
  add.set_tonodeid(groupid);
  for (auto & memberid: membersid) {
    add.add_membersid(memberid);
  }
  put_map_send(std::forward<CB_Func>(func), 
      buffer::Buffer(add));
}

/*
 * network user
 *
 * */
void kvdb::queryuserVersion(const std::string & userid,
    CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::QueryUserVersion query;
  query.set_userid(userid);
  put_map_send(std::forward<CB_Func>(func), 
      buffer::Buffer(query));
}
void kvdb::queryuserVersion(CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  queryuserVersion(get_current_userid(), 
      std::forward<CB_Func>(func));
}
void kvdb::queryuser(const std::string && userid, 
    CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::QueryUser query;
  query.set_userid(userid);
  put_map_send(std::forward<CB_Func>(func), 
      buffer::Buffer(query));
}
void kvdb::queryuser(CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  queryuser(get_current_userid(), 
      std::forward<CB_Func>(func));
}

/*
 * network node and message
 *
 * func
 * failure error key
 *
 * */
void kvdb::querynode(const std::string & nodeid, 
    CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::QueryNode query;
  query.set_tonodeid(nodeid);
  put_map_send(std::forward<CB_Func>(func), 
      buffer::Buffer(query));
}
void kvdb::sendmessage2user(const std::string & userid,
                        const std::string & tonodeid,
                        const int type,
                        const std::string & contenct,
                        CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::NodeMessage msg;
  msg.set_touserid_outer(userid);
  msg.set_tonodeid(tonodeid);
  msg.set_type(static_cast<chat::MediaType>(type));
  msg.set_content(contenct);
  put_map_send_cache(std::forward<CB_Func>(func), 
      buffer::Buffer(msg));

}
void kvdb::sendmessage2group(const std::string & tonodeid,
                      const int type,
                      const std::string & contenct,
                      CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::NodeMessage msg;
  msg.set_tonodeid(tonodeid);
  msg.set_type(static_cast<chat::MediaType>(type));
  msg.set_content(contenct);
  put_map_send_cache(std::forward<CB_Func>(func), 
      buffer::Buffer(msg));
}
void kvdb::querymsg(const std::string & tonodeid,
              const int32_t fromincrementid,
              const int32_t toincrementid,
              CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::QueryMessage query;
  query.set_tonodeid(tonodeid);
  query.set_fromincrementid(fromincrementid);
  query.set_toincrementid(toincrementid);
  query.set_isreset(true);
  put_map_send(std::forward<CB_Func>(func),
      buffer::Buffer(query));
}
void kvdb::querymsgContine(CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::QueryMessage query;
  query.set_isreset(false);
  put_map_send(std::forward<CB_Func>(func),
      buffer::Buffer(query));
}
void kvdb::queryonemsg(const std::string & tonodeid,
                   const int32_t incrementid,
                   CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  chat::QueryOneMessage query;
  query.set_tonodeid(tonodeid);
  query.set_incrementid(incrementid);
  put_map_send(std::forward<CB_Func>(func),
      buffer::Buffer(query));
}

/*
 *
 * private
 *
 * */
void kvdb::put_map(const int32_t sessionid, CB_Func && func) {
  YILOG_TRACE ("func: {}", __func__);
  std::unique_lock<std::mutex> ul(sessionid_map_mutex_);
  sessionid_cbfunc_map_[sessionid] = func;
}
void kvdb::put_map_send(CB_Func && func, Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  std::unique_lock<std::mutex> ul(sessionid_map_mutex_);
  client_send(sp, &temp_session_);
  sessionid_cbfunc_map_[temp_session_] = func;
}
void kvdb::call_erase_map(const int32_t sessionid, 
    const std::string & key) {
  YILOG_TRACE ("func: {}", __func__);
  std::unique_lock<std::mutex> ul(sessionid_map_mutex_);
  auto it = sessionid_cbfunc_map_.find(sessionid);
  if (likely(it != sessionid_cbfunc_map_.end())) {
    it->second(key);
    sessionid_cbfunc_map_.erase(sessionid);
  }else {
    throw std::system_error(std::error_code(50020, 
          std::generic_category()),
        "sessionid_cbfunc_map_ not find type"); 
  }
}

void kvdb::put_map_send_cache(CB_Func && func, Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  std::unique_lock<std::mutex> ul(sessionid_map_mutex_);
  client_send(sp, &temp_session_);
  sessionid_cbfunc_map_[temp_session_] = func;
  auto key = std::to_string(temp_session_);
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  put(key, value);
}

void kvdb::dispatch(int type, Buffer_SP sp) {
  YILOG_TRACE ("func: {}. ", __func__);
  auto static map_p = new std::map<int, std::function<void(void)>>();
  std::once_flag flag;
  std::call_once(flag, [=]() {
      // regist login logout connect disconnet
      (*map_p)[ChatType::registorres] = [=]() {
        registerRes(sp);
      };
      (*map_p)[ChatType::loginres] = [=]() {
        loginRes(sp);
      };
      (*map_p)[ChatType::logoutres] = [=]() {
        logoutRes(sp);
      };
      (*map_p)[ChatType::clientconnectres] = [=]() {
        connectRes(sp);
      };
      (*map_p)[ChatType::clientdisconnectres] = [=]() {
        disconnectRes(sp);
      };
      // error
      (*map_p)[ChatType::error] = [=]() {
        error(sp);
      };
      // friend group
      (*map_p)[ChatType::addfriendres] = [=]() {
        addfriendRes(sp);
      };
      (*map_p)[ChatType::addfriendauthorizeres] = [=]() {
        addfriendAuthorizeRes(sp);
      };
      (*map_p)[ChatType::creategroupres] = [=]() {
        creategroupRes(sp);
      };
      (*map_p)[ChatType::groupaddmemberres] = [=]() {
        groupaddmemberRes(sp);
      };
      // user
      (*map_p)[ChatType::queryuserres] = [=]() {
        queryuserversionRes(sp);
      };
      (*map_p)[ChatType::queryuserversionres] = [=]() {
        queryuserRes(sp);
      };
      // node and message
      (*map_p)[ChatType::querynoderes] = [=]() {
        querynodeRes(sp);
      };
      (*map_p)[ChatType::nodemessageres] = [=]() {
        messageRes(sp);
      };
      (*map_p)[ChatType::nodemessage] = [=]() {
        nodemessage(sp);
      };
  });
  auto it = map_p->find(type);
  if (it != map_p->end()) {
    it->second();
  }else {
    throw std::system_error(std::error_code(50021, 
          std::generic_category()),
        "map_p not find type"); 
  }
}

// regist login logout connect disconnet
void kvdb::registerRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  auto key = signupKey();
  put(key, value);
  call_erase_map(Session_ID::regist_login_connect, key);
}

void kvdb::loginRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  auto key = loginKey();
  put(key, value);
  call_erase_map(Session_ID::regist_login_connect, key);
}

void kvdb::logoutRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  auto key = logoutKey();
  put(key, value);
  call_erase_map(Session_ID::logout_disconnect, key);
}

void kvdb::connectRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  auto key = connectKey();
  put(key, value);
  call_erase_map(Session_ID::regist_login_connect, key);
}

void kvdb::disconnectRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  auto key = disconnectKey();
  put(key, value);
  call_erase_map(Session_ID::logout_disconnect, key);
}

void kvdb::error(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  set_current_error(sp);
  std::string key = errorKey(get_current_userid(),
      std::to_string(get_current_error_maxth() - 1));
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  put(key, value);
  call_erase_map(sp->session_id(), key);
}

void kvdb::addfriendRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto key = userKey(get_current_userid());
  call_erase_map(sp->session_id(), key);
}
void kvdb::addfriendAuthorizeRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto key = userKey(get_current_userid());
  call_erase_map(sp->session_id(), key);
}
void kvdb::creategroupRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto node = chat::CreateGroupRes();
  node.ParseFromArray(sp->data(), sp->data_size());
  auto key = nodeKey(node.tonodeid());
  call_erase_map(sp->session_id(), key);
}
void kvdb::groupaddmemberRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto node = chat::GroupAddMemberRes();
  node.ParseFromArray(sp->data(), sp->data_size());
  auto key = nodeKey(node.tonodeid());
  call_erase_map(sp->session_id(), key);
}
void kvdb::queryuserversionRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto res = chat::QueryUserVersionRes();
  res.ParseFromArray(sp->data(), sp->data_size());
  auto key = std::to_string(res.version());
  call_erase_map(sp->session_id(), key);
}
void kvdb::queryuserRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto res = chat::QueryUserRes();
  res.ParseFromArray(sp->data(), sp->data_size());
  auto key = userKey(res.user().id());
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  put(key, value);
  call_erase_map(sp->session_id(), key);
}
void kvdb::querynodeRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  auto res = chat::QueryNodeRes();
  res.ParseFromArray(sp->data(), sp->data_size());
  auto key = nodeKey(res.node().id());
  auto value = leveldb::Slice(sp->data(), sp->data_size());
  put(key, value);
  call_erase_map(sp->session_id(), key);
}
void kvdb::messageRes(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  std::string msg_string;
  if (get(std::to_string(sp->session_id()), msg_string).ok()) {
    // update msg 
    auto res = chat::NodeMessageRes();
    res.ParseFromArray(sp->data(), sp->data_size());
    auto msg = chat::NodeMessage();
    msg.ParseFromString(msg_string);
    msg.set_incrementid(res.incrementid());
    // compose ke value
    std::string key = msgKey(msg.tonodeid(), 
        msg.incrementid());
    leveldb::Slice value(msg.SerializeAsString());
    // update database
    leveldb::WriteBatch batch;
    batch.Delete(std::to_string(sp->session_id()));
    batch.Put(key, value);
    auto status = db_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
      throw std::system_error(std::error_code(50031, 
            std::generic_category()),
         "delete temporary key (sessionid) and insert msg failure");
    }
    call_erase_map(sp->session_id(), key);
  }else {
    throw std::system_error(std::error_code(50030, 
          std::generic_category()),
       "not find temporary key (sessionid)");
  }
}
void kvdb::nodemessage(Buffer_SP sp) {
  YILOG_TRACE ("func: {}", __func__);
  // put msg to db
  chat::NodeMessage msg;
  msg.ParseFromArray(sp->data(), sp->data_size());
  putMsg(msg.tonodeid(), msg.incrementid(), msg.SerializeAsString());
  auto key = msgKey(msg.tonodeid(), msg.incrementid());
  call_erase_map(sp->session_id(), key);
}

