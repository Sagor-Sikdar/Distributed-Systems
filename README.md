
#  Distributed Systems Labs in C++

MIT 6.824 style distributed systems lab rebuilt in C++.
It includes a series of labs in which you will build a transactional, sharded, fault-tolerant key/value storage system (like Google Spanner, MongoDB, etc). 

## Lab assignments

# Lab 1: Replicated State Machine

This project implements a **Replicated State Machine** using the Raft consensus algorithm. The project focuses on leader election, log replication, and fault tolerance for a distributed key-value storage system.

## Key Features

### 1. Leader Election and Heartbeats
- Implemented **leader election** using `RequestVote` RPCs.
- Periodically initiated elections when no leader was detected.
- Implemented **heartbeats** (empty `AppendEntries` RPCs) to maintain leadership.
- Managed election timeouts to prevent split votes.

### 2. Log Replication
- Added log entries to the leader using the `Start()` function and replicated them across nodes with `AppendEntries` RPC.
- Advanced the commit index when a majority of nodes replicated the logs.

### 3. Failure Handling
- Handled network failures and ensured followers rejoin and synchronize with the leader.
- Maintained log consistency across nodes during disconnections.

### 4. Testing
All tests were successfully passed, including:
- Leader election and re-election after failure.
- Log replication consistency across nodes.
- Handling follower disconnections and leader recovery.

# Lab 2: Fault-tolerant Key/Value Service

This project extends the Raft-based replicated state machine to implement a **fault-tolerant key-value storage service**. The service supports `Put`, `Append`, and `Get` operations, ensuring strong consistency across distributed servers.

## Key Features

### 1. Key-Value Operations
- Implemented `Put(key, value)`, `Append(key, arg)`, and `Get(key)` functions for the key-value store.
- Each operation is replicated via Raft to ensure consistency across servers.
- Clients retry requests if sent to the wrong server or if the leader changes.

### 2. Fault Tolerance and Consistency
- Achieved **strong consistency** using **linearizability**, ensuring all clients see the same and latest state.
- Clients retry failed operations due to server changes or network issues.
- Operations proceed as long as a majority of servers are alive, even during leader re-elections.

### 3. Concurrency and Coordination
- Handled concurrent client operations with Raft ensuring all servers execute commands in the same order.
- Integrated key-value operations into Raft’s log, ensuring all servers apply operations consistently.

### 4. Testing and Validation
Successfully passed all tests, including:
- Basic key-value operations.
- Concurrent operations with multiple clients.
- Leader re-election and continued operation with a majority of servers alive.
- Ensured progress and consistency under unreliable network conditions.
# Lab 3: Sharded Key/Value Service

This project extends the Raft-based key-value system by implementing a **sharded key-value storage system**. The system partitions keys across multiple replica groups to improve performance and handles shard reconfiguration when groups join or leave.

## Key Features

### 1. Shard Master and Replica Groups
- Implemented a **shard master** to manage configurations for replica groups.
- The shard master assigns shards to replica groups and supports reconfiguration via `Join`, `Leave`, `Move`, and `Query` RPCs.
- Replica groups use Raft to replicate the key-value data for their assigned shards.

### 2. Sharded Key-Value Store
- Each replica group is responsible for a subset of shards and handles operations (`Get`, `Put`, and `Append`) for its assigned shards.
- Clients query the shard master to determine which group is responsible for a particular key.

### 3. Reconfiguration and Shard Migration
- Handled shard reconfiguration when replica groups join or leave, ensuring smooth **shard migration**.
- Shards are transferred between replica groups while ensuring that clients always interact with the correct group.
- Implemented polling of the shard master to detect configuration changes and trigger shard migrations.

### 4. Fault Tolerance and Linearizability
- Ensured fault tolerance by replicating data across replica groups, allowing continued operation even if some servers fail.
- Implemented **linearizability**, ensuring that operations across shards are consistent and appear in a global order.

### 5. Testing and Validation
Passed all tests:
- Basic and concurrent shard operations.
- Minimal shard transfers after groups join or leave.
- Correct handling of configuration changes, shard transfers, and client requests.

## Lab environment

A modern linux environment (e.g., Ubuntu 22.04) is recommended for the labs. If you do not have access to this, consider using a virtual machine. 

## Getting started (Ubuntu 22.04)

Get source code:
```
git clone --recursive [repo_address]
```

Install dependencies:
```
sudo apt-get update
sudo apt-get install -y \
    git \
    pkg-config \
    build-essential \
    clang \
    libapr1-dev libaprutil1-dev \
    libboost-all-dev \
    libyaml-cpp-dev \
    libjemalloc-dev \
    python3-dev \
    python3-pip \
    python3-wheel \
    python3-setuptools \
    libgoogle-perftools-dev
sudo pip3 install -r requirements.txt
```

For next steps, checkout the guidelines in the [course web page](http://mpaxos.com/teaching/ds/22fa/labs.html).

## Previous Commit Author Information
Please note that some commits in this repository are authored by Sagor054, which is an alternate GitHub account previously used by the project owner, Sagor-Sikdar. This account is no longer maintained, and all future contributions will be made from the current account. The ownership and authorship of the code remain with Sagor-Sikdar.

## Acknowledgements
Author: Shuai Mu, Julie Lee (lab 1)

Many of the lab structure and the guideline text are taken from MIT 6.824. The code is based on the acedemic prototypes of previous research works including Rococo/Janus/Snow/DRP/Rolis/DepFast.
