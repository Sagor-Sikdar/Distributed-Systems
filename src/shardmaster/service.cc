
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "service.h"
#include "client.h"
#include "../kv/server.h"

namespace janus {

void ShardMasterServiceImpl::Join(const map<uint32_t, std::vector<uint32_t>>& gid_server_map, uint32_t* ret, rrr::DeferredReply* defer) {
  // your code here
  uint64_t oid = GetNextOpId();
  
  shared_ptr<MultiStringMarshallable> data = make_shared<MultiStringMarshallable>();
  data->data_.push_back(to_string(oid));
  data->data_.push_back("Join");
  for (auto it: gid_server_map){
    data->data_.push_back(to_string(it.first));    
    for (auto val: it.second) data->data_.push_back(to_string(val));
  }

  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;

  uint64_t index, term;
  auto cmd = std::static_pointer_cast<Marshallable>(data);
  
  if (!GetRaftServer().Start(cmd, &index, &term)) {
    *ret = KV_NOTLEADER;
  }

  else {
    event->Wait(30000000);
    *ret = (event->status_ == Event::TIMEOUT) ? KV_TIMEOUT : KV_SUCCESS;
  }  
  defer->reply();
}

void ShardMasterServiceImpl::Leave(const std::vector<uint32_t>& gids, uint32_t* ret, rrr::DeferredReply* defer) {
  // your code here
  uint64_t oid = GetNextOpId();
  shared_ptr<MultiStringMarshallable> data = make_shared<MultiStringMarshallable>();
  
  data->data_.push_back(to_string(oid));
  data->data_.push_back("Leave");
  for (auto val: gids){
    data->data_.push_back(to_string(val));
  }

  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;

  uint64_t index, term;
  auto cmd = std::static_pointer_cast<Marshallable>(data);

  if (!GetRaftServer().Start(cmd, &index, &term)) {
    *ret = KV_NOTLEADER;
  }
  else {
    event->Wait(30000000);
    *ret = event->status_ == Event::TIMEOUT ? KV_TIMEOUT : KV_SUCCESS;
  }

  defer->reply();
}
void ShardMasterServiceImpl::Move(const int32_t& shard, const uint32_t& gid, uint32_t* ret, rrr::DeferredReply* defer) {
  // your code here
  uint64_t oid = GetNextOpId();
  shared_ptr<MultiStringMarshallable> data = make_shared<MultiStringMarshallable>();
  
  data->data_.push_back(to_string(oid));
  data->data_.push_back("Move");
  data->data_.push_back(to_string(shard));
  data->data_.push_back(to_string(gid));

  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;

  uint64_t index, term;
  auto cmd = std::static_pointer_cast<Marshallable>(data);

  if (!GetRaftServer().Start(cmd, &index, &term)) {
    *ret = KV_NOTLEADER;
  }
  else {
    event->Wait(30000000);
    *ret = event->status_ == Event::TIMEOUT ? KV_TIMEOUT : KV_SUCCESS;
  }
  defer->reply();
}

void ShardMasterServiceImpl::Query(const int32_t& config_no, uint32_t* ret, ShardConfig* config, rrr::DeferredReply* defer) {
  // your code here
  lck.lock();
  int cindex = config_no;
  if (config_no == -1 || config_no > configNum) {
    cindex = configNum; 
  }
  lck.unlock();

  uint64_t oid = GetNextOpId();
  auto data = make_shared<MultiStringMarshallable>();
  data->data_.push_back(to_string(oid));
  data->data_.push_back(to_string(cindex));
  data->data_.push_back("Query");
  auto event = Reactor::CreateSpEvent<IntEvent>();
  eventStorage[to_string(oid)] = event;

  uint64_t index, term;
  auto cmd = std::dynamic_pointer_cast<Marshallable>(data);
  if (!GetRaftServer().Start(cmd, &index, &term)) {
    *ret = KV_NOTLEADER;
  } else {
    event->Wait(30000000);
    if (event->status_ == Event::TIMEOUT) {
      *ret = KV_TIMEOUT;
    } else { 
      *config = configs_[cindex];
      *ret = KV_SUCCESS;
    }
  }
  defer->reply();
}

void ShardMasterServiceImpl::OnNextCommand(Marshallable& m) {
  // your code here  
  auto v = (MultiStringMarshallable*)(&m);
  std::string temp = v->data_[0];

  
  if (v->data_[1][0] == 'J') {
    configNum++;
    ShardConfig config(configNum, configs_[configNum - 1]);

    uint32_t gid = stoi(v->data_[2]);

    if (configs_[configNum - 1].group_servers_map_.find(gid) == configs_[configNum - 1].group_servers_map_.end()) {
      int n_group = configs_[configNum - 1].group_servers_map_.size() + 2;
      int minLen = configs_[configNum - 1].shard_group_map_.size() / n_group;

      uint32_t gid = stoi(v->data_[2]);

      for (int pos = 3; pos < v->data_.size(); pos++) config.group_servers_map_[gid].push_back(stoi(v->data_[pos]));

      map<uint32_t, uint32_t> shard_count;
      for (auto it: config.shard_group_map_) {
        if (shard_count[it.second] == minLen) {
          config.shard_group_map_[it.first] = gid;
        } else {
          shard_count[it.second]++;
        }
      }
    }
  
    configs_[configNum] = config;
  }

  else if (v->data_[1][0] == 'L') {
    configNum++;
    ShardConfig config(configNum, configs_[configNum - 1]);

    for (int pos = 2; pos < v->data_.size(); pos++) {
      uint32_t gid = stoi(v->data_[pos]);

      if (configs_[configNum - 1].group_servers_map_.find(gid) != configs_[configNum - 1].group_servers_map_.end()) {
        int n_group = configs_[configNum - 1].group_servers_map_.size();

        config.group_servers_map_.erase(gid);
        vector<uint32_t> groups{0};
        for (auto it: config.group_servers_map_) groups.push_back(it.first);


        int idx = 0;
        for (auto it: configs_[configNum - 1].shard_group_map_) {
          if (it.second == gid) {
            config.shard_group_map_[it.first] = groups[idx];
            idx = (idx + 1) % n_group;
          }
        }
      }
    }

    configs_[configNum] = config;
  }

  else if (v->data_[1][0] == 'M') {
    configNum++;
    ShardConfig config(configNum, configs_[configNum - 1]);

    uint32_t shard = stoi(v->data_[2]);
    uint32_t gid = stoi(v->data_[3]);
    config.shard_group_map_[shard] = gid;

    configs_[configNum] = config;
  }

  if(eventStorage.find(temp) != eventStorage.end() && eventStorage[temp]->status_ != Event::TIMEOUT){
    eventStorage[temp]->Set(1);
  }
}

// do not change anything below
shared_ptr<ShardMasterClient> ShardMasterServiceImpl::CreateClient() {
  auto cli = make_shared<ShardMasterClient>();
  cli->commo_ = sp_log_svr_->commo_;
  uint32_t id = sp_log_svr_->site_id_;
  return cli;
}

} // namespace janus