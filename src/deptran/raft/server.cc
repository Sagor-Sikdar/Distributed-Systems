

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
    Simulation();
}

bool RaftServer::Start(shared_ptr<Marshallable> &cmd,
                       uint64_t *index,
                       uint64_t *term) {
  /* Your code here. This function can be called from another OS thread. */
  m.lock();
  if (currentRole == LEADER) {
    *term = currentTerm;
    commands.push_back(cmd);
    terms.push_back(currentTerm);
    *index = terms.size();
    startIndex++;
    // Log_info("Start For Server %d is called", site_id_);
    m.unlock();
    return true;
  }

  m.unlock();
  return false;
}

void RaftServer::GetState(bool *is_leader, uint64_t *term) {
  /* Your code here. This function can be called from another OS thread. */
  
  timeout_val = 0;
  *term = currentTerm;
  *is_leader = currentRole == LEADER && timeout_val < 4;
}

void RaftServer::SyncRpcExample() {
  /* This is an example of synchronous RPC using coroutine; feel free to 
     modify this function to dispatch/receive your own messages. 
     You can refer to the other function examples in commo.h/cc on how 
     to send/recv a Marshallable object over RPC. */
  Coroutine::CreateRun([this](){
    string res;
    auto event = commo()->SendString(0, /* partition id is always 0 for lab1 */
                                     2, "hello", &res);

    event->Wait(1000000); //timeout after 1000000us=1s
    if (event->status_ == Event::TIMEOUT) {
      Log_info("timeout happens");
    } else {
      Log_info("rpc response is: %s", res.c_str());
    }
  });
}

/* Do not modify any code below here */

void RaftServer::Disconnect(const bool disconnect) {
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
  disconnected_ = disconnect;
}

bool RaftServer::IsDisconnected() {
  return disconnected_;
}

int getRandom(int minV, int maxV) {
  return rand() % (maxV - minV + 1) + minV;
}

void RaftServer::LeaderElection() {
  m.lock();
  votesReceived.clear();
  votesReceived.insert(site_id_);
  m.unlock();
  for (int serverId = 0; serverId < 5 && currentRole == CANDIDATE; serverId++){
    if (serverId == site_id_) { continue;}

    auto callback = [&] () {
      uint64_t retTerm, cTerm = currentTerm, candidateId = site_id_, svrId = serverId;
      bool_t vote_granted;

      // Log_info("[SendRequestVote] (cId, svrId, term) = (%d, %d, %d)\n", candidateId, serverId, currentTerm);
      uint64_t logLength = terms.size();
      uint64_t logTerm = logLength > 0 ? terms[logLength - 1] : 0;  
      auto event = commo()->SendRequestVote(0, svrId, candidateId, cTerm, logTerm, logLength, &retTerm, &vote_granted);   
      
      event->Wait(40000); //timeout after 1000000us=1s
      if (event->status_ != Event::TIMEOUT) {          
        m.lock();
        // Log_info("[ReceiveRequestVote] : (cId, sId, rTerm, vote_granted) -> (%d, %d, %d, %d)]", site_id_, svrId, retTerm, vote_granted); 
        if (currentRole == CANDIDATE && vote_granted && retTerm == currentTerm) {
          votesReceived.insert(svrId);

          if (votesReceived.size() == 3){
            currentLeader = site_id_;
            currentRole = LEADER;
            timeout_val = 0;
            
            // Log_info("New Leader: %d, matchLength: %d, commitLength: %d", site_id_, matchLength[site_id_], commitLength);
            for (int fId = 0; fId < 5; fId++) {
              if (fId == site_id_) continue;
              nextIndex[fId] = commands.size();
              matchLength[fId] = 0;
              ReplicateLog(fId);
            }

            matchLength[site_id_] = terms.size();
          }
          
        } else if (retTerm > currentTerm) {
          currentTerm = retTerm;
          currentRole = FOLLOWER;
          votedFor = -1;
        } 
        m.unlock();
      }  
    };
    Coroutine::CreateRun(callback);
  }
}

void RaftServer::ElectionTimer() {
    auto callback = [&] () {
  
    m.lock();
    electionTimeout = 1000 + site_id_ * 500;
    election_start_time = std::chrono::steady_clock::now();
    m.unlock();

    while(currentRole == CANDIDATE && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - election_start_time).count() < std::chrono::milliseconds(electionTimeout).count()) {
      Coroutine::Sleep(electionTimeout * 100);
    }

    m.lock();
    if (currentRole == CANDIDATE) {
      currentTerm += 1;
      votedFor = site_id_;
    }
    m.unlock();
  };
  Coroutine::CreateRun(callback);
}

void RaftServer:: HeartBeatTimer() {
  auto callback = [&] () {
    //auto random_timeout = getRandom(1000, 1500);
    auto random_timeout = 1000 + 125 * site_id_;
    auto timeout = std::chrono::milliseconds(random_timeout);
    t_start = std::chrono::steady_clock::now();

    while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_start).count() < timeout.count()) {
      Coroutine::Sleep(random_timeout * 75);
    }

    // Log_info("HeartBeat Timeout %d for server: %d", random_timeout, site_id_);

    m.lock();
    // Log_info("CurrentRole: %d, votedFor: %d", currentRole == FOLLOWER, votedFor);
    if (currentRole == FOLLOWER){
      currentTerm += 1;
      currentRole = CANDIDATE;
      votedFor = site_id_;
      // Log_info("Server %d is promoted to a candidate, term: %d", site_id_, currentTerm);
    }
    m.unlock();
  };
  Coroutine::CreateRun(callback);
}

void RaftServer:: ReplicateLog(int followerID) {
  auto callback = [followerID, this] () {
    while(currentRole == LEADER) {
      m.lock();
      uint64_t retTerm, ackLength, cTerm = currentTerm;
      bool_t success;
      
      uint64_t prefixLength = nextIndex[followerID];
      uint64_t prevLogTerm = prefixLength > 0 ? terms[prefixLength - 1] : 0;
      std::vector<shared_ptr<Marshallable>> suffix_commands(commands.begin() + prefixLength, commands.end());
      std::vector<uint64_t> suffix_terms(terms.begin() + prefixLength, terms.end());
      m.unlock();

      auto event = commo()->SendAppendEntries(0, followerID, site_id_, cTerm, prefixLength, prevLogTerm, suffix_commands, suffix_terms, commitLength, &retTerm, &ackLength, &success);
      event->Wait(40000);
      if (event->status_ != Event::TIMEOUT) {
        m.lock();
        // Log_info("[VoteResponse] fId: %d, cTerm: %d, retTerm: %d", followerID, cTerm, retTerm);
        timeout_val = 0;
        if (success == false && ackLength == -1) {
          currentTerm = retTerm;
          currentRole = FOLLOWER;
          votedFor = -1;
          // Log_info("[Leader-->Follower] retTerm > cTerm for %d", followerID);
          m.unlock();
          break;
        }

        else if (currentRole == LEADER){
          if (success) {
            // Log_info("%d responded successfully", followerID);
            nextIndex[followerID] = ackLength;

            matchLength[followerID] = ackLength;

            std::vector<uint64_t>acksReceived;
            // std::cout << "MatchLength: ";
            matchLength[site_id_] = terms.size();
            for (int fsz = 0; fsz < 5; fsz++) {
              // std::cout << matchLength[fsz] << " ";
              if (matchLength[fsz] > 0) acksReceived.push_back(matchLength[fsz]);
            }
            // std::cout << std::endl;
            // Log_info("Size of acksReceived arr of leader: %d is: %d", site_id_, acksReceived.size());
            if (acksReceived.size() >= 3) {
              sort(acksReceived.begin(), acksReceived.end());

              int pos = -1;
              int minSize = acksReceived[0], maxSize = acksReceived[acksReceived.size() - 3];
              
              // Log_info("Min_size: %d, max_size: %d, currentTerm: %d", minSize, maxSize, currentTerm);
              for (int len = minSize; len <= maxSize; len++) {
                if (len > commitLength && terms[len - 1] == currentTerm) {
                  pos = len;
                }              
              }

              // Log_info("Pos: %d", pos);
              if (pos != -1) {
                for (int i = commitLength; i < pos; i++) {
                  app_next_(*commands[i]);
                  // Log_info("[Leader: %d] app_next_ called for index: %d", site_id_, i);
                }
                commitLength = pos;
                // Log_info("Committing at Server: %d, commitSize: %d", site_id_, commitLength);
              }
            }
          } else if (nextIndex[followerID] > 0){
            nextIndex[followerID]--;
            Coroutine::Sleep(15000);
            m.unlock();
            continue;
          }
        }
      } 
      else {
        // Log_info("Timeout for %d, leader: %d", followerID, site_id_);
        matchLength[followerID] = 0;
        timeout_val = timeout_val + 1 < 4 ? timeout_val + 1 : 4;
      }
      m.unlock();
      Coroutine::Sleep(100000);
    }
  };
  Coroutine::CreateRun(callback);
}

void RaftServer::Simulation() {
  Coroutine::CreateRun([this](){
    uint64_t prevFTerm = -1, prevCTerm = -1, prevLterm = -1, logCall = -1;
    while (true) {
      if (currentRole == FOLLOWER && prevFTerm != currentTerm) {
        prevFTerm = currentTerm;
        HeartBeatTimer();
      }
      
      else if (currentRole == CANDIDATE && prevCTerm != currentTerm) {
        prevCTerm = currentTerm;
        ElectionTimer();
        LeaderElection();
      } 
      Coroutine::Sleep(30000);  
    }
  });
}
}// namespace janus