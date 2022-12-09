#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "deptran/procedure.h"
#include "../command_marshaler.h"
#include "raft_rpc.h"
#include "server.h"
#include "macros.h"

class SimpleCommand;
namespace janus {

class TxLogServer;
class RaftServer;
class RaftServiceImpl : public RaftService {
 public:
  RaftServer* svr_;
  RaftServiceImpl(TxLogServer* sched);

  RpcHandler(RequestVote, 6,
             const uint64_t&, candidateId,
             const uint64_t&, candidateTerm,
             const uint64_t&, candidateLogTerm,
             const uint64_t&, candidateLogLength,
             uint64_t*, ret1,
             bool_t*, vote_granted) {
    *ret1 = 0;
    *vote_granted = false;
    svr_->currentRole = FOLLOWER;
  }

  RpcHandler(AppendEntries, 5,
            const uint64_t&, candidateId,
            const uint64_t&, candidateTerm,
            const MarshallDeputy&, cmd,
            uint64_t*, retTerm,
            bool_t*, isAlive) {
    *retTerm = 0;
    *isAlive = false;
    svr_->currentRole = FOLLOWER;
  }

  RpcHandler(HeartBeat, 5,
            const uint64_t&, candidateId,
            const uint64_t&, candidateTerm,
            const MarshallDeputy&, cmd,
            uint64_t*, retTerm,
            bool_t*, isAlive) {
    *retTerm = 0;
    *isAlive = false;
    svr_->currentRole = FOLLOWER;
  }

  RpcHandler(HelloRpc, 2, const string&, req, string*, res) {
    *res = "error"; 
  };
};

} // namespace janus
