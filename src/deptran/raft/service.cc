
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
  // Log_info("[HandleRequestVote] received rpc: (cId, rId, cterm, svrTerm, votedFor) --> (%d, %d, %d, %d, %d)", candidateId, svr_->site_id_, candidateTerm, svr_->currentTerm, svr_->votedFor);
  bool validTerm = (candidateTerm > svr_->currentTerm) || (candidateTerm == svr_->currentTerm && (svr_->votedFor == -1 || svr_->votedFor == candidateId));
  
  bool isUpdatedLog = (svr_->logs.size() == 0);
  if (isUpdatedLog == false) {
    int logSize = svr_->logs.size();
    int lastTerm = svr_->logs[logSize - 1].term;

    isUpdatedLog = (candidateLogTerm > lastTerm) || (candidateLogTerm == lastTerm && candidateLogLength >= logSize); 
  }  
  
  if (validTerm && isUpdatedLog) {
    svr_->currentTerm = candidateTerm;
    svr_->currentRole = FOLLOWER;
    svr_->votedFor = candidateId;
    // Log_info("[Service.cc] %d is now a follower", svr_->site_id_);
  }   
  *retTerm = svr_->currentTerm;
  *vote_granted = validTerm;
  svr_->t_start = std::chrono::steady_clock::now();
  // Log_info("[(Done)HandleRequestVote: (cID, rID, rTerm, vote_granted, votedFor) --> (%d, %d, %d, %d, %d --> %d)\n", candidateId, svr_->site_id_,  *retTerm, *vote_granted, svr_->site_id_, svr_->votedFor);
  svr_->m.unlock();
  defer->reply();
}

void RaftServiceImpl::HandleAppendEntries(const uint64_t& leaderId,
                                          const uint64_t& leaderTerm,
                                          const MarshallDeputy& md_cmd,
                                          uint64_t *retTerm,
                                          bool_t *isAlive,
                                          rrr::DeferredReply* defer) {
  /* Your code here */
  std::shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy&>(md_cmd).sp_data_;
  svr_->m.lock();
  if (leaderTerm >= svr_->currentTerm) {
    if (leaderTerm > svr_->currentTerm) svr_->votedFor = -1;
    *isAlive = true;
    svr_->currentTerm = leaderTerm;
    svr_->currentLeader = leaderId;
    svr_->t_start = std::chrono::steady_clock::now();
    // Log_info("Heart Beat Received at %d from %d",svr_->site_id_, leaderId);
  }
  else {
    *isAlive = false;
  }
  *retTerm = svr_->currentTerm;
  svr_->m.unlock();
  defer->reply();
}

void RaftServiceImpl::HandleHeartBeat(const uint64_t& leaderId,
                                          const uint64_t& leaderTerm,
                                          const MarshallDeputy& md_cmd,
                                          uint64_t *retTerm,
                                          bool_t *isAlive,
                                          rrr::DeferredReply* defer) {
  /* Your code here */
  std::shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy&>(md_cmd).sp_data_;
  svr_->m.lock();
  if (leaderTerm >= svr_->currentTerm) {
    if (leaderTerm > svr_->currentTerm) svr_->votedFor = -1;
    *isAlive = true;
    svr_->currentTerm = leaderTerm;
    svr_->currentLeader = leaderId;
    svr_->t_start = std::chrono::steady_clock::now();
    // Log_info("Heart Beat Received at %d from %d",svr_->site_id_, leaderId);
  }
  else {
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
