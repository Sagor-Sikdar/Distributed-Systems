

#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"
#include "frame.h"
#include "coordinator.h"
#include "../classic/tpc_command.h"
#include <ctime>


namespace janus {

RaftServer::RaftServer(Frame * frame) {
  frame_ = frame ;
  /* Your code here for server initialization. Note that this function is 
     called in a different OS thread. Be careful about thread safety if 
     you want to initialize variables here. */

}

RaftServer::~RaftServer() {
  /* Your code here for server teardown */

}

void RaftServer::Setup() {
  /* Your code here for server setup. Due to the asynchronous nature of the 
     framework, this function could be called after a RPC handler is triggered. 
     Your code should be aware of that. This function is always called in the 
     same OS thread as the RPC handlers. */
    Log_info("ServerId: %d has started", site_id_);
    Simulation();
}

bool RaftServer::Start(shared_ptr<Marshallable> &cmd,
                       uint64_t *index,
                       uint64_t *term) {
  /* Your code here. This function can be called from another OS thread. */
  Log_info("Start For Server %d is called", site_id_);
  *index = 0;
  *term = 0;
  return false;
}

void RaftServer::GetState(bool *is_leader, uint64_t *term) {
  /* Your code here. This function can be called from another OS thread. */
  m.lock();
  *is_leader = (currentRole == LEADER && !IsDisconnected());
  *term = currentTerm;
  m.unlock();
}

void RaftServer::SyncRpcExample() {
  /* This is an example of synchronous RPC using coroutine; feel free to 
     modify this function to dispatch/receive your own messages. 
     You can refer to the other function examples in commo.h/cc on how 
     to send/recv a Marshallable object over RPC. */
  Coroutine::CreateRun([this](){
    string res;
    uint64_t ret;
    bool_t visited;
    // auto event = commo()->SendString(0, /* partition id is always 0 for lab1 */
    //                                  2, "example_msg", &res);

    auto event = commo()->SendRequestVote(0, 2, 10, 20, &ret, &visited);

    event->Wait(1000000); //timeout after 1000000us=1s
    if (event->status_ == Event::TIMEOUT) {
      // Log_info("timeout happens");
    } else {
      // Log_info("[SyncRpcExample] rpc response is: %d", ret); 
    }
  });
}

/* Do not modify any code below here */

void RaftServer::Disconnect(const bool disconnect) {
  Log_info("Disconnecting %d", site_id_);
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  verify(disconnected_ != disconnect);
  // global map of rpc_par_proxies_ values accessed by partition then by site
  static map<parid_t, map<siteid_t, map<siteid_t, vector<SiteProxyPair>>>> _proxies{};
  if (_proxies.find(partition_id_) == _proxies.end()) {
    _proxies[partition_id_] = {};
  }
  RaftCommo *c = (RaftCommo*) commo();
  if (disconnect) {
    verify(_proxies[partition_id_][loc_id_].size() == 0);
    verify(c->rpc_par_proxies_.size() > 0);
    auto sz = c->rpc_par_proxies_.size();
    _proxies[partition_id_][loc_id_].insert(c->rpc_par_proxies_.begin(), c->rpc_par_proxies_.end());
    c->rpc_par_proxies_ = {};
    verify(_proxies[partition_id_][loc_id_].size() == sz);
    verify(c->rpc_par_proxies_.size() == 0);
  } else {
    verify(_proxies[partition_id_][loc_id_].size() > 0);
    auto sz = _proxies[partition_id_][loc_id_].size();
    c->rpc_par_proxies_ = {};
    c->rpc_par_proxies_.insert(_proxies[partition_id_][loc_id_].begin(), _proxies[partition_id_][loc_id_].end());
    _proxies[partition_id_][loc_id_] = {};
    verify(_proxies[partition_id_][loc_id_].size() == 0);
    verify(c->rpc_par_proxies_.size() == sz);
  }
  Log_info("disconnected");
  disconnected_ = disconnect;
}

bool RaftServer::IsDisconnected() {
  return disconnected_;
}

int getRandom(int minV, int maxV) {
  return rand() % (maxV - minV + 1) + minV;
}

void RaftServer::SendHeartBeat() {
  while (currentRole == LEADER){
    for (int serverId = 0; serverId < 5; serverId++) {
        m.lock();
        if (currentRole != LEADER) { m.unlock(); break; }
        if (serverId == site_id_) { m.unlock(); continue; }
        m.unlock();

        auto callback = [&] () {
          int svrId = serverId;
          bool_t isAlive;
          uint64_t retTerm;

          auto cmdptr = std::make_shared<TpcCommitCommand>();
          auto vpd_p = std::make_shared<VecPieceData>();
          vpd_p->sp_vec_piece_data_ = std::make_shared<vector<shared_ptr<SimpleCommand>>>();
          cmdptr->tx_id_ = -1;
          cmdptr->cmd_ = vpd_p;
          auto cmdptr_m = dynamic_pointer_cast<Marshallable>(cmdptr);

          // auto event = commo()->SendAppendEntries(0, svrId, site_id_, currentTerm, cmdptr_m, &retTerm, &isAlive);
          auto event = commo()->SendHeartBeat(0, svrId, site_id_, currentTerm, cmdptr_m, &retTerm, &isAlive);

          event->Wait(80000);

          if (event->status_ != Event::TIMEOUT) {
            m.lock();
            if (!isAlive && retTerm > currentTerm) {
              currentTerm = retTerm;
              currentRole = FOLLOWER;
              votedFor = -1;
              // Log_info("Leader %d has now become a follower", site_id_);
            }
            m.unlock();
          }
        };
        // Coroutine::Sleep(100000);
        Coroutine::CreateRun(callback);
    }
    Coroutine::Sleep(25000);
  }
}


void RaftServer:: ReceiveHeartBeat() {
  auto callback = [&] () {
    auto random_timeout = getRandom(1000, 1500);
    auto timeout = std::chrono::milliseconds(random_timeout);
    t_start = std::chrono::steady_clock::now();

    while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_start).count() < timeout.count()) {
      Coroutine::Sleep(random_timeout * 100);
    }

    // Log_info("HeartBeat Timeout %d for server: %d", random_timeout, site_id_);

    m.lock();
    if (currentRole == FOLLOWER && !IsDisconnected()){
      // Log_info("Server %d is promoted to a candidate, term: %d", site_id_, currentTerm);
      currentTerm += 1;
      currentRole = CANDIDATE;
      votedFor = site_id_;
    }
    m.unlock();
  };
  Coroutine::CreateRun(callback);
}

void RaftServer::LeaderElection() {
  m.lock();
  votesReceived.clear();
  votesReceived.insert(site_id_);
  m.unlock();
  for (int serverId = 0; serverId < 5; serverId++){
    m.lock();
    if (currentRole == FOLLOWER) {
      m.unlock();
      break;
    }
    if (serverId == site_id_) {
      m.unlock();
      continue;
    }
    m.unlock();

    auto callback = [&] () {
      uint64_t retTerm, cTerm = currentTerm, candidateId = site_id_, svrId = serverId;
      bool_t vote_granted;

      // Log_info("[SendRequestVote] (cId, svrId, term) = (%d, %d, %d)\n", candidateId, serverId, currentTerm);
      
      auto event = commo()->SendRequestVote(0, svrId, candidateId, cTerm, &retTerm, &vote_granted);   
      event->Wait(1000000 + site_id_ * 300000); //timeout after 1000000us=1s
      
      if (event->status_ != Event::TIMEOUT) {          
        m.lock();
        // Log_info("[ReceiveRequestVote : (cId, sId, rTerm, vote_granted) -> (%d, %d, %d, %d)]", site_id_, svrId, retTerm, vote_granted); 
        if (currentRole == CANDIDATE && vote_granted && retTerm == currentTerm) {
          votesReceived.insert(svrId);
          // Log_info("ServerId: %d, votesReceived: %d", site_id_, votesReceived.size());

          if (votesReceived.size() == 3) {
            // Log_info ("%d won the election, term: %d", site_id_, currentTerm);
            currentLeader = site_id_;
            currentRole = LEADER;
            electionTimeout = 0;
          }
        } else if (retTerm > currentTerm) {
          currentTerm = retTerm;
          currentRole = FOLLOWER;
          electionTimeout = 0;
          votedFor = -1;
          // Log_info("[retTerm > currentTerm] for server: %d", site_id_);
        } else{
          // Log_info("[vote not granted] for server: %d", site_id_);
        }
        m.unlock();
      } else {
          // Log_info("SendRequestVote timeout happens for %d", site_id_);
      }  
    };
    Coroutine::CreateRun(callback);
  }
}



void RaftServer::ElectionTimer() {
    auto callback = [&] () {
  
    m.lock();
    //electionTimeout = getRandom(0, 1000) * site_id_ + 1000;
    electionTimeout = 1000 + site_id_ * 300;
    election_start_time = std::chrono::steady_clock::now();
    m.unlock();

    while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - election_start_time).count() < std::chrono::milliseconds(electionTimeout).count()) {
      Coroutine::Sleep(electionTimeout * 50);
    }

    // Log_info("Election Timeout %d for server: %d", electionTimeout, site_id_);

    m.lock();
    if (currentRole == CANDIDATE){
      // Log_info("Server %d is still the candidate", site_id_);
      currentTerm += 1;
    }
    m.unlock();
  };
  Coroutine::CreateRun(callback);
}



void RaftServer::Simulation() {
  Coroutine::CreateRun([this](){
    int prevFTerm = -1, prevCTerm = -1, prevLterm = -1;
    while (true) {
      if (currentRole == FOLLOWER && prevFTerm != currentTerm) {
        prevFTerm = currentTerm;
        ReceiveHeartBeat();
        continue;
      } else if (currentRole == CANDIDATE && prevCTerm != currentTerm) {
        prevCTerm = currentTerm;
        ElectionTimer();
        LeaderElection();
      } else if (currentRole == LEADER && prevLterm != currentTerm){
        prevLterm = currentTerm;
        SendHeartBeat();
      }      
      Coroutine::Sleep(30000);  
    }
  });
}
}// namespace janus