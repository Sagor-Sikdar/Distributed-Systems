

#include "client.h"
#include "server.h"

namespace janus {

int ShardKvClient::Op(uint64_t gid, function<int(uint32_t*, uint64_t)> func) {
  uint64_t t1 = Time::now();
  while (true) {
    uint64_t t2 = Time::now();
    if (t2 - t1 > 10000000) {
      return KV_TIMEOUT;
    }
    uint32_t ret = 0;
    int r1; 
    r1 = func(&ret,gid);
    if (r1 == ETIMEDOUT || ret == KV_TIMEOUT) {
      leader_idx_ = (leader_idx_+1) % 5;
      return KV_TIMEOUT;
    }
    if (ret == KV_SUCCESS) {
      return KV_SUCCESS;
    }
    if (ret == KV_NOTLEADER) {
      leader_idx_ = (leader_idx_+1) % 5;
    }
  }

}

ShardMasterClient ShardKvClient::getSMClient(){
  ShardMasterClient sm;
  sm.commo_ = this->commo_;
  return sm;
}

int ShardKvClient::Put(const string& k, const string& v) {
  shardid_t shardid = Key2Shard(k);
  ShardConfig sc;
  getSMClient().Query(-1,&sc);
  uint64_t gid =  sc.shard_group_map_[shardid];
  return Op(gid,[&](uint32_t* r, uint64_t gid)->int{
    return Proxy(gid * 5 + leader_idx_).Put(GetNextOpId(), k, v, r);
  });
}

ShardKvProxy& ShardKvClient::Proxy(siteid_t site_id) {
  verify(commo_);
  auto p = (ShardKvProxy*)commo_->rpc_proxies_.at(site_id);
  return *p; 
}

int ShardKvClient::Append(const string& k, const string& v) {
  shardid_t shardid = Key2Shard(k);
  ShardConfig sc;
  getSMClient().Query(-1,&sc);
  uint64_t gid =  sc.shard_group_map_[shardid];
  return Op(gid,[&](uint32_t* r, uint64_t gid)->int{
    return Proxy(gid * 5 + leader_idx_).Append(GetNextOpId(), k, v, r);
  });
}

int ShardKvClient::Get(const string& k, string* v) {
  shardid_t shardid = Key2Shard(k);
  ShardConfig sc;
  getSMClient().Query(-1,&sc);
  uint64_t gid =  sc.shard_group_map_[shardid];
  return Op(gid,[&](uint32_t* r, uint64_t gid)->int{
    return Proxy(gid * 5 + leader_idx_).Get(GetNextOpId(), k, r, v);
  });
}
} // namesapce janus;