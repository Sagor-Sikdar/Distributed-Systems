#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"

namespace janus {

class TxData;

class RaftCommo : public Communicator {

 public:
  RaftCommo() = delete;
  RaftCommo(PollMgr*);

  shared_ptr<IntEvent> 
  SendRequestVote(parid_t par_id,
                  siteid_t site_id,
                  uint64_t candidateId,
                  uint64_t candidateTerm, 
                  uint64_t candidateLogTerm,
                  uint64_t candidateLogLength,  
                  uint64_t *ret, 
                  bool_t *vote_granted);

  shared_ptr<IntEvent>
  SendAppendEntries(parid_t par_id,
                    siteid_t site_id,
                    uint64_t leaderId,
                    uint64_t leaderTerm, 
                    uint64_t prefixLogIndex,
                    uint64_t prevLogTerm,
                    std::vector<shared_ptr<Marshallable>> cmds,
                    std::vector<uint64_t> terms,
                    uint64_t leaderCommitIndex,
                    uint64_t *ret, 
                    uint64_t *matchedIndex, 
                    bool_t *success) ;

  shared_ptr<IntEvent>
  SendHeartBeat(parid_t par_id,
                siteid_t site_id,
                uint64_t candidateId,
                uint64_t candidateTerm, 
                uint64_t *ret, 
                bool_t *isAlive);

  shared_ptr<IntEvent> 
  SendString(parid_t par_id, siteid_t site_id, const string& msg, string* res);

  /* Do not modify this class below here */

  friend class FpgaRaftProxy;
 public:
#ifdef RAFT_TEST_CORO
  std::recursive_mutex rpc_mtx_ = {};
  uint64_t rpc_count_ = 0;
#endif
};

} // namespace janus

