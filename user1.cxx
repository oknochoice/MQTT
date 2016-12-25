#include "macro.h"
#include "spdlog/spdlog.h"
#include "yijian/lib_client.h"
#include "yijian/mongo.h"
#include "yijian/protofiles/chat_message.pb.h"
#include <google/protobuf/util/json_util.h>
#include "buffer.h"
#include <iostream>
#include <unistd.h>
#include <thread>
#include <condition_variable>
#include "kvdb.h"
#include <google/protobuf/util/json_util.h>

#include <boost/coroutine2/all.hpp>
std::mutex mutex_;
std::condition_variable cvar_;
bool iswait_ = true;
using google::protobuf::util::MessageToJsonString;;
void mainwait() {
  YILOG_TRACE("func: {}, iswait_: {}", __func__, iswait_);
  std::unique_lock<std::mutex> ul(mutex_);
  cvar_.wait(ul, [](){
        YILOG_TRACE("iswait_: {}", iswait_);
        return !iswait_;
      });
  iswait_ = true;
}

void notimain() {
  YILOG_TRACE("func: {}, iswait_: {}", __func__, iswait_);
  std::unique_lock<std::mutex> ul(mutex_);
  iswait_ = false;
  cvar_.notify_one();
}

template <typename Proto>
std::string pro2string(Proto & any) {
  std::string value;
  google::protobuf::util::MessageToJsonString(any, &value);
  return value;
}

int main() {
  initConsoleLog();
  using yijian::Buffer_SP;

  auto thread_id = std::this_thread::get_id();
  std::cout << "main thread id " << thread_id << std::endl;

  typedef boost::coroutines2::coroutine<chat::NodeMessage> coro_t;
  coro_t::pull_type getOneMsg(
      [](coro_t::push_type & sink){
        auto msg = chat::NodeMessage();
        for (int i = 0; i < 1000; ++i) {
          msg.set_type(chat::MediaType::TEXT);
          msg.set_content(std::to_string(i));
          sink(msg);
        }
      });
  for (int i = 0; i < 10; ++i) {
    auto item = getOneMsg.get();
    getOneMsg();
    YILOG_TRACE("content:{}.", item.content());
  }

  std::string phoneno = "18514029918";
  std::string countrycode = "86";
  std::string password = "123456";
  std::string verifycode = "654321";
  std::string name = "user1_yijian";
  int os = 0;
  std::string osversion = "9.0.3";
  std::string appversion = "1.0.0";
  std::string devicemodel = "iphone 5s";
  std::string uuid = phoneno + "_uuid";

  auto db = new kvdb(name);

  /*
  db->registUser(phoneno, countrycode, password,
      verifycode, name, [&db](const std::string & key){
        std::string value;
        db->get(key, value);
        YILOG_INFO("key:{}, value:{}.", key, value);
        chat::RegisterRes res;
        res.ParseFromString(value);
        std::string json;
        MessageToJsonString(res, &json);
        std::cout << json << std::endl;
        assert(res.issuccess() == true);
        notimain();
      });
  mainwait();

  db->registUser(phoneno, countrycode, password,
      verifycode, name, [&db](const std::string & key){
        YILOG_INFO("key:{}.", key);
        std::string value;
        db->get(key, value);
        chat::RegisterRes res;
        res.ParseFromString(value);
        std::string json;
        MessageToJsonString(res, &json);
        std::cout << json << std::endl;
        assert(res.issuccess() == false);
        YILOG_INFO("errno:{}, msg:{}.", 
            res.e_no(), res.e_msg());
        notimain();
      });
  mainwait();
  */

  db->login(phoneno, countrycode, "000000", os, devicemodel, uuid,
      [&db](const std::string & key){
        YILOG_INFO("key:{}.", key);
        std::string value;
        db->get(key, value);
        chat::LoginRes res;
        res.ParseFromString(value);
        std::string json;
        MessageToJsonString(res, &json);
        std::cout << json << std::endl;
        assert(res.issuccess() == false);
        notimain();
      });
  mainwait();

  db->login(phoneno, countrycode, password, os, devicemodel, uuid,
      [&db](const std::string & key){
        YILOG_INFO("key:{}.", key);
        std::string value;
        db->get(key, value);
        chat::LoginRes res;
        res.ParseFromString(value);
        std::string json;
        MessageToJsonString(res, &json);
        std::cout << json << std::endl;
        assert(res.issuccess() == true);
        notimain();
      });
  mainwait();

  std::string userid = db->get_current_userid();
  db->connect(userid, uuid, true, osversion, appversion,
      [&db](const std::string & key){
        YILOG_INFO("key:{}.", key);
        std::string value;
        db->get(key, value);
        chat::ClientConnectRes res;
        res.ParseFromString(value);
        std::string json;
        MessageToJsonString(res, &json);
        std::cout << json << std::endl;
        assert(res.issuccess() == true);
        notimain();
      });
  mainwait();

  std::string user_data;
  auto isHasUser = db->getUser(db->get_current_userid(), user_data);
  auto queryuser = [&db, &user_data](){
    db->queryuser(db->get_current_userid(), 
        [&db, &user_data](const std::string & key){
          YILOG_INFO ("query user {}", key);
          assert(key == db->userKey(db->get_current_userid()));
          db->get(key, user_data);
          chat::User user;
          user.ParseFromString(user_data);
          YILOG_INFO ("{}", pro2string(user));
          notimain();
        });
  };
  if (isHasUser) {
    chat::User user;
    user.ParseFromString(user_data);
    YILOG_INFO ("{}", pro2string(user));
    db->queryuserVersion(db->get_current_userid(),
        [&user, &queryuser](const std::string & version){
          YILOG_INFO ("query user version {}", version);
          auto v = std::stoi(version);
          if (v > user.version()) {
            queryuser();
          }else{
            notimain();
          }
        });
  }else {
    queryuser();
  }

  mainwait();
  /*
  db->addfriend("585fcf1b4b99d9260b5e9186", "ni hao",
      [](const std::string & key){
      });
      */


  return 0;
}

