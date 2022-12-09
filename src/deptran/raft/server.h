#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "commo.h"

namespace janus {

enum State { FOLLOWER, CANDIDATE, LEADER };

struct LogEntry {
  shared_ptr<Marshallable> cmd;
  uint64_t term;

  LogEntry(shared_ptr<Marshallable> c, int t) {
    cmd = c;
    term = t;
  }
};

class RaftServer : public TxLogServer {
 public:
  /* Your data here */
  uint64_t currentTerm = 0;
  uint64_t votedFor = -1;
  std::vector<LogEntry> logs;
  uint64_t commitIndex;
  std::chrono::time_point<std::chrono::steady_clock> t_start = std::chrono::steady_clock::now();
  std::chrono::time_point<std::chrono::steady_clock> election_start_time = std::chrono::steady_clock::now();
  int electionTimeout;

  State currentRole;
  uint64_t currentLeader;
  std::unordered_set<int> votesReceived;
  std::unordered_map<int, int> nextIndex;
  std::unordered_map<int, int> matchIndex;
  std::recursive_mutex m;

  /* Your functions here */
  void Init();
  void LeaderElection();
  void SendHeartBeat();
  void ReceiveHeartBeat();
  void ElectionTimer();
  void Simulation();


  /* do not modify this class below here */

 public:
  RaftServer(Frame *frame) ;
  ~RaftServer() ;

  bool Start(shared_ptr<Marshallable> &cmd, uint64_t *index, uint64_t *term);
  void GetState(bool *is_leader, uint64_t *term);

 private:
  bool disconnected_ = false;
	void Setup();

 public:
  void SyncRpcExample();
  void Disconnect(const bool disconnect = true);
  void Reconnect() {
    Log_info("ServerID: %d is reconnected.", site_id_);
    Disconnect(false);
  }
  bool IsDisconnected();

  virtual bool HandleConflicts(Tx& dtxn,
                               innid_t inn_id,
                               vector<string>& conflicts) {
    verify(0);
  };
  RaftCommo* commo() {
    return (RaftCommo*)commo_;
  }
};
} // namespace janus
