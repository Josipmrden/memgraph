#include <condition_variable>
#include <mutex>

#include "distributed/coordination_worker.hpp"

namespace distributed {

WorkerCoordination::WorkerCoordination(communication::messaging::System &system,
                                       const Endpoint &master_endpoint)
    : system_(system),
      client_(system_, master_endpoint, kCoordinationServerName),
      server_(system_, kCoordinationServerName) {}

int WorkerCoordination::RegisterWorker(int desired_worker_id) {
  auto result = client_.Call<RegisterWorkerRpc>(300ms, desired_worker_id,
                                                system_.endpoint());
  CHECK(result) << "Failed to RegisterWorker with the master";
  return result->member;
}

Endpoint WorkerCoordination::GetEndpoint(int worker_id) {
  auto accessor = endpoint_cache_.access();
  auto found = accessor.find(worker_id);
  if (found != accessor.end()) return found->second;
  auto result = client_.Call<GetEndpointRpc>(300ms, worker_id);
  CHECK(result) << "Failed to GetEndpoint from the master";
  accessor.insert(worker_id, result->member);
  return result->member;
}

void WorkerCoordination::WaitForShutdown() {
  std::mutex mutex;
  std::condition_variable cv;
  bool shutdown = false;

  server_.Register<StopWorkerRpc>([&](const StopWorkerReq &) {
    std::unique_lock<std::mutex> lk(mutex);
    shutdown = true;
    lk.unlock();
    cv.notify_one();
    return std::make_unique<StopWorkerRes>();
  });

  std::unique_lock<std::mutex> lk(mutex);
  cv.wait(lk, [&shutdown] { return shutdown; });
  // Sleep to allow the server to return the StopWorker response. This is
  // necessary because Shutdown will most likely be called after this function.
  // TODO (review): Should we call server_.Shutdown() here? Not the usual
  // convention, but maybe better...
  std::this_thread::sleep_for(100ms);
};
}  // namespace distributed