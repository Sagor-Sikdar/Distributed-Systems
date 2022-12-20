
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "raft_rpc.h"
#include "macros.h"

namespace janus {

RaftCommo::RaftCommo(PollMgr* poll) : Communicator(poll) {
}


shared_ptr<IntEvent> 
RaftCommo::SendRequestVote(parid_t par_id,
                          siteid_t site_id,
                          uint64_t candidateId,
                          uint64_t candidateTerm, 
                          uint64_t candidateLogTerm,
                          uint64_t candidateLogLength,  
                          uint64_t *ret, 
                          bool_t *vote_granted) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto& p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;

      fuattr.callback = [ret, vote_granted, ev](Future* fu) {
        fu->get_reply() >> *ret;
        fu->get_reply() >> *vote_granted;
        if(ev->status_ != Event::TIMEOUT){
          ev->Set(1);
        }
      };
      /* Always use Call_Async(proxy, RPC name, RPC args..., fuattr)
      * to asynchronously invoke RPCs */
      Call_Async(proxy, RequestVote, candidateId, candidateTerm, candidateLogTerm, candidateLogLength, fuattr);
    }
  }
  return ev;
  
}

shared_ptr<IntEvent> 
RaftCommo::SendAppendEntries(parid_t par_id,
                        siteid_t site_id,
                        uint64_t leaderId,
                        uint64_t leaderTerm, 
                        uint64_t prefixLogLength,
                        uint64_t prefixLogTerm,
                        std::vector<shared_ptr<Marshallable>> commands,
                        std::vector<uint64_t> terms,
                        uint64_t leaderCommitIndex,
                        uint64_t *ret, 
                        uint64_t *matchedIndex, 
                        bool_t *success) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto& p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;
      fuattr.callback = [ret, matchedIndex, success, ev](Future* fu) {
        fu->get_reply() >> *ret;
        fu->get_reply() >> *matchedIndex;
        fu->get_reply() >> *success;
        if(ev->status_ != Event::TIMEOUT){
          ev->Set(1);
        }
      };
      
      std::vector<MarshallDeputy> md;
      for (int i = 0; i < commands.size(); i++) md.push_back(MarshallDeputy(commands[i]));
      // Log_info("Terms size: %d", terms.size());
      Call_Async(proxy, AppendEntries, leaderId, leaderTerm, prefixLogLength, prefixLogTerm, md, terms, leaderCommitIndex, fuattr);
    }
  }
  return ev;
}


shared_ptr<IntEvent> 
RaftCommo::SendHeartBeat(parid_t par_id,
                              siteid_t site_id,
                              uint64_t candidateId,
                              uint64_t candidateTerm,
                              uint64_t *ret, 
                              bool_t *isSuccess) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto& p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;
      fuattr.callback = [ret, isSuccess, ev](Future* fu) {
        // bool_t followerAppendOK;
        fu->get_reply() >> *ret;
        fu->get_reply() >> *isSuccess;
        if(ev->status_ != Event::TIMEOUT){
          ev->Set(1);
        }
      };
      /* wrap Marshallable in a MarshallDeputy to send over RPC */
      Call_Async(proxy, HeartBeat, candidateId, candidateTerm, fuattr);
    }
  }
  return ev;
}

shared_ptr<IntEvent> 
RaftCommo::SendString(parid_t par_id, siteid_t site_id, const string& msg, string* res) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto& p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;
      fuattr.callback = [res,ev](Future* fu) {
        fu->get_reply() >> *res;
        ev->Set(1);
      };
      /* wrap Marshallable in a MarshallDeputy to send over RPC */
      Call_Async(proxy, HelloRpc, msg, fuattr);
    }
  }
  return ev;
}
} // namespace janus
