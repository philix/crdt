// Copyright (C) 2020 Felipe O. Carvalho

#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "crdt.h"
#include "lib.h"

template <typename CRDT>
class P2PNetwork {
 public:
  size_t add(CRDT *crdt) {
    _replicas.push_back(crdt);
    return _replicas.size() - 1;
  }

  void disconnect(size_t i) {
    CRDT *replica = _replicas[i];
    if (replica) {
      printf("Disconnect '%s' from the network.\n", replica->name().c_str());
      _replicas[i] = nullptr;
      _offline_set.emplace(i, replica);
    }
  }

  void reconnect(size_t i) {
    for (auto it = _offline_set.begin(); it != _offline_set.end(); ++it) {
      if (it->first == i) {
        assert(_replicas[i] == nullptr);
        printf("Reconnecting '%s' to the network.\n", it->second->name().c_str());
        _replicas[i] = it->second;
        _offline_set.erase(it);
        break;
      }
    }
  }

  void broadcast(size_t i) {
    const CRDT *replica = _replicas[i];
    if (!replica) {
      return;
    }
    printf("Broadcasting from '%s' to all connected replicas...\n", replica->name().c_str());
    for (size_t j = 0; j < _replicas.size(); j++) {
      if (j != i) {
        auto *other = _replicas[j];
        if (other) {
          other->merge(*replica);
        }
      }
    }
  }

  void broadcastAll() {
    for (size_t i = 0; i < _replicas.size(); i++) {
      broadcast(i);
    }
  }

  int countPartitions() const {
    std::unordered_set<typename CRDT::ValueType> distinct_values;
    for (auto *replica : _replicas) {
      if (replica) {
        const auto value = replica->query();
        distinct_values.insert(value);
      }
    }
    for (auto & [ _, replica ] : _offline_set) {
      const auto value = replica->query();
      distinct_values.insert(value);
    }
    return (int)distinct_values.size();
  }

  void dump() const {
    printf("P2P network state:\n");
    if (!_offline_set.empty()) {
      printf("- online:\n");
    }
    for (auto *replica : _replicas) {
      if (replica) {
        replica->dump();
      }
    }
    if (!_offline_set.empty()) {
      printf("- offline\n");
      for (auto[_, replica] : _offline_set) {
        replica->dump();
      }
    }
    if (countPartitions() == 1) {
      puts("ALL CONVERGED!");
    }
    printf("\n");
  }

 private:
  std::vector<CRDT *> _replicas;
  std::unordered_set<std::pair<size_t, CRDT *>> _offline_set;
};

template <typename CRDT>
class StarNetwork {
 public:
  size_t setServerReplica(CRDT *crdt) {
    if (_replicas.empty()) {
      _replicas.push_back(crdt);
    } else {
      _replicas[0] = crdt;
    }
    return 0;
  }

  size_t add(CRDT *crdt) {
    if (_replicas.empty()) {
      _replicas.push_back(nullptr);  // The 0-th replica is the server replica
    }
    _replicas.push_back(crdt);
    return _replicas.size() - 1;
  }

  void disconnect(size_t i) {
    CRDT *replica = _replicas[i];
    if (replica) {
      if (i == 0) {
        printf("Server is down.\n");
      } else {
        printf("Disconnect '%s' from the network.\n", replica->name().c_str());
      }
      _replicas[i] = nullptr;
      _offline_set.emplace(i, replica);
    }
  }

  void reconnect(size_t i) {
    for (auto it = _offline_set.begin(); it != _offline_set.end(); ++it) {
      if (it->first == i) {
        assert(_replicas[i] == nullptr);
        if (i == 0) {
          printf("Server is back up.\n");
        } else {
          printf("Reconnecting '%s' to the network.\n", it->second->name().c_str());
        }
        _replicas[i] = it->second;
        _offline_set.erase(it);
        break;
      }
    }
  }

  void syncWithServer(size_t i) {
    if (i == 0) {
      return;  // 0 is the server
    }
    auto *server = _replicas[0];
    auto *replica = _replicas[i];
    if (!replica) {
      return;
    }
    if (!server) {
      printf("Server is not reachable from replica '%s'.\n", replica->name().c_str());
      return;
    }
    printf("Replica '%s' is syncing with %s.\n", replica->name().c_str(), server->name().c_str());
    // This simulates a request/response transaction in which the server
    // immediatelly replies with what it has and performs the merge
    // asynchrnously (i.e. after replying) for low-latency. Due to merge's
    // commutativity, both replicas (client and server) will reach the same CRDT
    // state.
    auto replica_copy_to_send_to_server = *replica;
    auto response_from_server = *server;
    // Perform merges in two directions
    replica->merge(response_from_server);
    server->merge(replica_copy_to_send_to_server);
    assert(replica->query() == server->query());
  }

  void syncAllReplicasToServer() {
    // i=0 is skipped (0 is the server)
    for (size_t i = 1; i < _replicas.size(); i++) {
      syncWithServer(i);
    }
  }

  int countPartitions() const {
    std::unordered_set<unsigned int> distinct_values;
    for (auto *replica : _replicas) {
      if (replica) {
        const auto value = replica->query();
        distinct_values.insert(value);
      }
    }
    for (auto & [ _, replica ] : _offline_set) {
      const auto value = replica->query();
      distinct_values.insert(value);
    }
    return (int)distinct_values.size();
  }

  void dump() const {
    printf("Star-network state:\n");
    if (!_offline_set.empty()) {
      printf("- online:\n");
    }
    for (auto *replica : _replicas) {
      if (replica) {
        replica->dump();
      }
    }
    if (!_offline_set.empty()) {
      printf("- offline\n");
      for (auto[_, replica] : _offline_set) {
        replica->dump();
      }
    }
    if (countPartitions() == 1) {
      puts("ALL CONVERGED!");
    }
    printf("\n");
  }

 private:
  std::vector<CRDT *> _replicas;
  std::unordered_set<std::pair<size_t, CRDT *>> _offline_set;
};

void simulateGCountersInP2PNetwork() {
  P2PNetwork<GCounter> network;

  GCounter a_counter("A");
  GCounter b_counter("B");
  GCounter c_counter("C");

  const size_t a = network.add(&a_counter);  // a=0
  const size_t b = network.add(&b_counter);  // b=0
  const size_t c = network.add(&c_counter);  // c=0
  (void)c;
  network.dump();
  assert(a_counter.query() == 0);
  assert(b_counter.query() == 0);
  assert(c_counter.query() == 0);

  a_counter.increment(1);  // a=1
  b_counter.increment(2);  // b=2
  c_counter.increment(3);  // c=3
  network.dump();
  assert(a_counter.query() == 1);
  assert(b_counter.query() == 2);
  assert(c_counter.query() == 3);
  assert(network.countPartitions() == 3);

  network.broadcast(a);  // a=1, b=3, c=4
  network.dump();
  assert(network.countPartitions() == 3);

  network.broadcastAll();  // a=6
  network.dump();
  assert(network.countPartitions() == 1);

  network.disconnect(b);
  a_counter.increment(10);  // a=16
  network.dump();

  network.broadcastAll();
  network.dump();
  assert(a_counter.query() == 16);
  assert(b_counter.query() == 6);
  assert(c_counter.query() == 16);
  assert(network.countPartitions() == 2);

  b_counter.increment(3);
  network.dump();
  assert(network.countPartitions() == 2);

  network.reconnect(b);
  network.broadcastAll();
  network.dump();
  assert(network.countPartitions() == 1);
}

void simulateGCountersInStarNetwork() {
  StarNetwork<GCounter> network;

  GCounter server_counter("SERVER");
  GCounter a_counter("A");
  GCounter b_counter("B");
  GCounter c_counter("C");

  const size_t server = network.setServerReplica(&server_counter);
  const size_t a = network.add(&a_counter);  // a=0
  const size_t b = network.add(&b_counter);  // b=0
  const size_t c = network.add(&c_counter);  // c=0
  network.disconnect(server);
  (void)c;
  network.dump();
  assert(a_counter.query() == 0);
  assert(b_counter.query() == 0);
  assert(c_counter.query() == 0);

  a_counter.increment(1);
  b_counter.increment(2);
  c_counter.increment(3);
  network.dump();
  assert(a_counter.query() == 1);
  assert(b_counter.query() == 2);
  assert(c_counter.query() == 3);
  assert(network.countPartitions() == 4);

  network.syncWithServer(a);
  network.dump();
  assert(network.countPartitions() == 4);  // nothing happened because server is down

  network.reconnect(server);  // Server is UP!
  network.syncAllReplicasToServer();
  network.dump();
  assert(network.countPartitions() == 3);  // Only SERVER and C have seen all updates.

  network.syncAllReplicasToServer();
  network.dump();
  assert(network.countPartitions() == 1);  // Full convergence now.

  network.disconnect(b);
  a_counter.increment(10);
  network.dump();

  network.syncAllReplicasToServer();
  network.dump();
  assert(a_counter.query() == 16);
  assert(b_counter.query() == 6);
  assert(c_counter.query() == 16);
  assert(network.countPartitions() == 2);

  b_counter.increment(3);
  network.dump();
  assert(network.countPartitions() == 2);

  network.reconnect(b);
  network.syncAllReplicasToServer();
  network.dump();
  assert(network.countPartitions() == 2);  // Not all converged because A hasn't seen B's increment.

  network.syncWithServer(a);
  network.dump();
  assert(network.countPartitions() == 1);
  assert(a_counter.query() == 19);

  network.syncAllReplicasToServer();
  network.dump();
  assert(network.countPartitions() == 1);
  assert(a_counter.query() == 19);  // nothing changes after convergence without increments
}

void simulatePNCountersInP2PNetwork() {
  P2PNetwork<PNCounter> network;

  PNCounter a_counter("A");
  PNCounter b_counter("B");
  PNCounter c_counter("C");

  const size_t a = network.add(&a_counter);  // a=0
  const size_t b = network.add(&b_counter);  // b=0
  const size_t c = network.add(&c_counter);  // c=0
  (void)c;
  network.dump();
  assert(a_counter.query() == 0);
  assert(b_counter.query() == 0);
  assert(c_counter.query() == 0);

  a_counter.increment(-1);
  b_counter.increment(2);
  c_counter.increment(3);
  network.dump();
  assert(a_counter.query() == -1);
  assert(b_counter.query() == 2);
  assert(c_counter.query() == 3);
  assert(network.countPartitions() == 3);

  network.broadcast(a);
  network.dump();
  assert(network.countPartitions() == 3);

  network.broadcastAll();
  network.dump();
  assert(network.countPartitions() == 1);

  network.disconnect(b);
  a_counter.increment(10);
  network.dump();

  network.broadcastAll();
  network.dump();
  assert(a_counter.query() == 14);
  assert(b_counter.query() == 4);
  assert(c_counter.query() == 14);
  assert(network.countPartitions() == 2);

  b_counter.increment(-3);
  network.dump();
  assert(network.countPartitions() == 2);

  network.reconnect(b);
  network.broadcastAll();
  network.dump();
  assert(network.countPartitions() == 1);
  assert(a_counter.query() == 11);

  b_counter.increment(-12);
  network.broadcast(b);
  network.dump();
  assert(network.countPartitions() == 1);
  assert(a_counter.query() == -1);
}

int main(int argc, char *argv[]) {
  // simulateGCountersInP2PNetwork();
  // simulateGCountersInStarNetwork();
  simulatePNCountersInP2PNetwork();
  return 0;
}