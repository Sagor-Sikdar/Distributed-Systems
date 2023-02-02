#include "../deptran/__dep__.h"
#include "server.h"
#include "../deptran/raft/server.h"
#include "client.h"

namespace janus {

static int volatile x1 =
    MarshallDeputy::RegInitializer(MarshallDeputy::CMD_MULTI_STRING,
                                     [] () -> Marshallable* {
                                       return new MultiStringMarshallable;
                                     });

int64_t KvServer::GetNextOpId() {
  verify(sp_log_svr_);
  int64_t ret = sp_log_svr_->site_id_;
  ret = ret << 32;
  ret = ret + op_id_cnt_++; 
  return ret;
}

int KvServer::Put(const uint64_t& oid, 
                  const string& k,
                  const string& v) {
  /* 
  Your are recommended to use MultiStringMarshallable as the format 
  for the log entries. Here is an example how you can use it.
  auto s = make_shared<MultiStringMarshallable>();
  s->data_.push_back(to_string(op_id));
  s->data_.push_back("put");
  s->data_.push_back(k);
  s->data_.push_back(v);
  */
  /* your code here */
  shared_ptr<MultiStringMarshallable> data = make_shared<MultiStringMarshallable>();
  data->data_.push_back(to_string(oid));
  data->data_.push_back("put");
  data->data_.push_back(k);
  data->data_.push_back(v);

  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;

  uint64_t index, term;

  auto cmd = std::static_pointer_cast<Marshallable>(data);
  
  if (!GetRaftServer().Start(cmd, &index, &term)) {
    return KV_NOTLEADER;
  }
  
  event->Wait(30000000);
  if (event->status_ == Event::TIMEOUT) {
    return KV_TIMEOUT;
  } 

  return KV_SUCCESS;
}

int KvServer::Append(const uint64_t& oid, 
                     const string& k,
                     const string& v) {
  /* your code here */
  shared_ptr<MultiStringMarshallable> data = make_shared<MultiStringMarshallable>();
  data->data_.push_back(to_string(oid));
  data->data_.push_back("append");
  data->data_.push_back(k);
  data->data_.push_back(v);

  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;

  uint64_t index, term;

  auto cmd = std::dynamic_pointer_cast<Marshallable>(data);
  if (!GetRaftServer().Start(cmd, &index, &term)) {
    return KV_NOTLEADER;
  }

  event->Wait(30000000);
  if (event->status_ == Event::TIMEOUT) {
    std::cout << "[Append Timeout] Key: " << k << ", Value: " << v << ", database: " << database[k] << std::endl;
    return KV_TIMEOUT;
  } 
  
  return KV_SUCCESS;
}

int KvServer::Get(const uint64_t& oid, 
                  const string& k,
                  string* v) {
  /* your code here */
  auto data = make_shared<MultiStringMarshallable>();
  data->data_.push_back(to_string(oid));
  data->data_.push_back("get");
  data->data_.push_back(k);
  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;


  uint64_t index, term;

  auto cmd = std::dynamic_pointer_cast<Marshallable>(data);
  if (!GetRaftServer().Start(cmd, &index, &term)) {
    return KV_NOTLEADER;
  }

  event->Wait(30000000);
  if (event->status_ == Event::TIMEOUT) {
    return KV_TIMEOUT;
  } 
  
  *v=database[k];
  return KV_SUCCESS;
}

void KvServer::OnNextCommand(Marshallable& m) {
  auto v = (MultiStringMarshallable*)(&m);
  /* your code here */
  std::string temp = v->data_[0];
  if(eventStorage.find(temp) != eventStorage.end() && eventStorage[temp]->status_ != Event::TIMEOUT){
    eventStorage[temp]->Set(1);
  }

  if (v->data_[1][0] == 'p') {
    database[v->data_[2]] = v->data_[3];
  }
  else if (v->data_[1][0] == 'a') {
    database[v->data_[2]].append(v->data_[3]);
  }
}

shared_ptr<KvClient> KvServer::CreateClient() {
  /* don't change this function */
  auto cli = make_shared<KvClient>();
  verify(commo_ != nullptr);
  cli->commo_ = commo_;
  uint32_t id = sp_log_svr_->site_id_;
  id = id << 16;
  cli->cli_id_ = id+cli_cnt_++; 
  return cli;
}

} // namespace janus;