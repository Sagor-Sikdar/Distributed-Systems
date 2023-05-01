#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "commo.h"

namespace janus {

enum State { FOLLOWER, CANDIDATE, LEADER };

class RaftServer : public TxLogServer {
 public:
  /* Your data here */
  uint64_t currentTerm = 0;
  uint64_t votedFor = -1;
  std::vector<shared_ptr<Marshallable>> commands;
  std::vector<uint64_t> terms;

  uint64_t commitLength;
  std::chrono::time_point<std::chrono::steady_clock> t_start = std::chrono::steady_clock::now();
  std::chrono::time_point<std::chrono::steady_clock> election_start_time = std::chrono::steady_clock::now();
  int electionTimeout;
  uint64_t startIndex = -1;

  State currentRole;
  uint64_t currentLeader;
  std::unordered_set<uint64_t> votesReceived;
  std::unordered_map<uint64_t, uint64_t> nextIndex;
  std::unordered_map<uint64_t, uint64_t> matchLength;
  std::recursive_mutex m;

  /* Your functions here */
  void Init();
  void LeaderElection();
  void SendHeartBeat();
  void HeartBeatTimer();
  void ElectionTimer();
  void Simulation();
  void ReplicateLog(int followerId);

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
