#include "../deptran/__dep__.h"
#include "server.h"
#include "../deptran/raft/server.h"
#include "client.h"

namespace janus {

int64_t ShardKvServer::GetNextOpId() {
  verify(sp_log_svr_);
  int64_t ret = sp_log_svr_->site_id_;
  ret = ret << 32;
  ret = ret + op_id_cnt_++; 
  return ret;
}

int ShardKvServer::Put(const uint64_t& oid, 
                  const string& k,
                  const string& v) {
    // lab_shard: fill in your code
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

int ShardKvServer::Append(const uint64_t& oid, 
                     const string& k,
                     const string& v) {
    // lab_shard: fill in your code
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
      return KV_TIMEOUT;
    } 
    return KV_SUCCESS;
}

int ShardKvServer::Get(const uint64_t& oid, 
                  const string& k,
                  string* v) {
    // lab_shard: fill in your code
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

void ShardKvServer::OnNextCommand(Marshallable& m) {
    // lab_shard: fill in your code
    auto v = (MultiStringMarshallable*)(&m);
    std::string temp = v->data_[0];

    if (v->data_[1][0] == 'p') {
      database[v->data_[2]] = v->data_[3];
    }
    else if (v->data_[1][0] == 'a') {
      database[v->data_[2]].append(v->data_[3]);
    }

    if(eventStorage.find(temp) != eventStorage.end() && eventStorage[temp]->status_ != Event::TIMEOUT){
      eventStorage[temp]->Set(1);
    }
}

shared_ptr<ShardKvClient> ShardKvServer::CreateClient(Communicator* comm) {
  auto cli = make_shared<ShardKvClient>();
  cli->commo_ = comm;
  verify(cli->commo_ != nullptr);
  static uint32_t id = 0;
  id++;
  cli->cli_id_ = id; 
  return cli;
}
} // namespace janus;