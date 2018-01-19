#include "glog/logging.h"

#include "storage/concurrent_id_mapper_master.hpp"
#include "storage/concurrent_id_mapper_rpc_messages.hpp"
#include "storage/types.hpp"

namespace storage {

namespace {
template <typename TId>
void RegisterRpc(MasterConcurrentIdMapper<TId> &mapper,
                 communication::rpc::Server &rpc_server);
#define ID_VALUE_RPC_CALLS(type)                                              \
  template <>                                                                 \
  void RegisterRpc<type>(MasterConcurrentIdMapper<type> & mapper,             \
                         communication::rpc::Server & rpc_server) {           \
    rpc_server.Register<type##IdRpc>([&mapper](const type##IdReq &req) {      \
      return std::make_unique<type##IdRes>(mapper.value_to_id(req.member));   \
    });                                                                       \
    rpc_server.Register<Id##type##Rpc>([&mapper](const Id##type##Req &req) {  \
      return std::make_unique<Id##type##Res>(mapper.id_to_value(req.member)); \
    });                                                                       \
  }

using namespace storage;
ID_VALUE_RPC_CALLS(Label)
ID_VALUE_RPC_CALLS(EdgeType)
ID_VALUE_RPC_CALLS(Property)
#undef ID_VALUE_RPC
}  // namespace

template <typename TId>
MasterConcurrentIdMapper<TId>::MasterConcurrentIdMapper(
    communication::messaging::System &system)
    // We have to make sure our rpc server name is unique with regards to type.
    // Otherwise we will try to reuse the same rpc server name for different
    // types (Label/EdgeType/Property)
    : rpc_server_(system, impl::RpcServerNameFromType<TId>()) {
  RegisterRpc(*this, rpc_server_);
}

template class MasterConcurrentIdMapper<Label>;
template class MasterConcurrentIdMapper<EdgeType>;
template class MasterConcurrentIdMapper<Property>;

}  // namespace storage
