
#include "../marshallable.h"
#include "service.h"
#include "server.h"

namespace janus {

RaftServiceImpl::RaftServiceImpl(TxLogServer *sched)
    : svr_((RaftServer*)sched) {
	struct timespec curr_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
	srand(curr_time.tv_nsec);
}


void RaftServiceImpl::HandleRequestVote(const uint64_t& candidateId,
                                        const uint64_t& candidateTerm,
                                        const uint64_t& candidateLogTerm,
                                        const uint64_t& candidateLogLength,
                                        uint64_t *retTerm,
                                        bool_t *vote_granted,
                                        rrr::DeferredReply* defer) {
  svr_->m.lock();
  
  bool validTerm = (candidateTerm > svr_->currentTerm) || (candidateTerm == svr_->currentTerm && (svr_->votedFor == -1 || svr_->votedFor == candidateId));
  bool UpdatedLog = true;;
  if (svr_->terms.size() > 0) {
    int logSize = svr_->terms.size();
    int lastTerm = svr_->terms[logSize - 1];
    UpdatedLog = (candidateLogTerm > lastTerm) || (candidateLogTerm == lastTerm && candidateLogLength >= logSize); 
  }  
  
  if (validTerm && UpdatedLog) {
    *retTerm = svr_->currentTerm = candidateTerm;
    svr_->currentRole = FOLLOWER;
    svr_->votedFor = candidateId;
    *vote_granted = true;
    svr_->t_start = std::chrono::steady_clock::now();
  }  else {
    *retTerm = svr_->currentTerm;
    *vote_granted = false;
  }
  
  svr_->m.unlock();
  defer->reply();
}

void RaftServiceImpl::HandleAppendEntries(const uint64_t& leaderId,
                                          const uint64_t& leaderTerm,
                                          const uint64_t& prefixLength,
                                          const uint64_t& prefixLogTerm,
                                          const std::vector<MarshallDeputy>& cmds,
                                          const std::vector<uint64_t>& terms,
                                          const uint64_t& leaderCommitIndex,
                                          uint64_t *retTerm,
                                          uint64_t *matchedIndex,
                                          bool_t *success,
                                          rrr::DeferredReply* defer) {
  /* Your code here */
  svr_->m.lock();
  if (svr_->currentTerm > leaderTerm
      || prefixLength > svr_->commands.size()
      || (prefixLength > 0 && prefixLogTerm != svr_->terms[prefixLength - 1])) {
    *retTerm = svr_->currentTerm;
    *matchedIndex = 0;
    *success = false;
  } else {
      if (svr_->currentTerm < leaderTerm) {
        svr_->currentTerm = leaderTerm;
        svr_->currentRole = FOLLOWER;
        svr_->votedFor = -1;
      }

      *retTerm = svr_->currentTerm;
      *matchedIndex = prefixLength + cmds.size();
      *success = true;

      if (cmds.size() > 0 && prefixLength < svr_->commands.size() && svr_->terms[prefixLength] != terms[0]){
        while(svr_->commands.size() > prefixLength) {
          svr_->commands.pop_back();
          svr_->terms.pop_back();
        }
      }

      if (prefixLength + cmds.size() > svr_->commands.size()) {
        for (int i = svr_->commands.size() - prefixLength; i < terms.size(); i++) {
          std::shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy&>(cmds[i]).sp_data_;
          svr_->commands.push_back(cmd);
          svr_->terms.push_back(terms[i]);
        }
      }

      if (leaderCommitIndex > svr_->commitLength) {
        for (int i = svr_->commitLength; i < leaderCommitIndex; i++) {
          svr_->app_next_(*svr_->commands[i]);
        }
        svr_->commitLength = leaderCommitIndex;
        Log_info("[HandleAppendEntries_Log] CommitLength for Server: %d is now %d", svr_->site_id_, svr_->commitLength);
      }
  }
  Log_info("[HandleAppendEntries] server: %d, retTerm: %d, matchIndex: %d, success: %d", svr_->site_id_, *retTerm, *matchedIndex, *success);
  svr_->m.unlock();
  defer->reply();
}

void RaftServiceImpl::HandleHeartBeat(const uint64_t& leaderId,
                                          const uint64_t& leaderTerm,
                                          uint64_t *retTerm,
                                          bool_t *isAlive,
                                          rrr::DeferredReply* defer) {
  // std::shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy&>(md_cmd).sp_data_;
  svr_->m.lock();
  // Log_info("Handle HeartBeat.. Lid: %d, LeaderTerm: %d, sId: %d, sTerm: %d", leaderTerm, leaderId, svr_->site_id_, svr_->currentTerm);
  if (leaderTerm >= svr_->currentTerm) {
    if (leaderTerm > svr_->currentTerm) svr_->votedFor = -1;

    svr_->currentTerm = leaderTerm;
    svr_->currentLeader = leaderId;
    svr_->currentRole = FOLLOWER;
    *isAlive = true;
    svr_->t_start = std::chrono::steady_clock::now();
  } else {
    *isAlive = false;
  }
  *retTerm = svr_->currentTerm;
  svr_->m.unlock();
  defer->reply();
}


void RaftServiceImpl::HandleHelloRpc(const string& req,
                                     string* res,
                                     rrr::DeferredReply* defer) {
  /* Your code here */
  Log_info("receive an rpc: %s from %d", req.c_str(), svr_->site_id_);
  *res = "hello";
  defer->reply();
}

} // namespace janus;
