/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_SHMEMTRANSPORT_H
#define OPENDDS_SHMEMTRANSPORT_H

#include "Shmem_Export.h"

#include "ShmemDataLink.h"
#include "ShmemDataLink_rch.h"

#include "dds/DCPS/transport/framework/TransportImpl.h"

#include "ace/Local_Memory_Pool.h"
#include "ace/Malloc_T.h"
#include "ace/Pagefile_Memory_Pool.h"
#include "ace/PI_Malloc.h"
#include "ace/Process_Mutex.h"
#include "ace/Shared_Memory_Pool.h"

#include <map>
#include <string>

namespace OpenDDS {
namespace DCPS {

class ShmemInst;

class OpenDDS_Shmem_Export ShmemTransport : public TransportImpl {
public:
  explicit ShmemTransport(const TransportInst_rch& inst);

  void passive_connection(const std::string& remote_address,
                          ACE_Message_Block* data);

#ifdef ACE_WIN32
  typedef ACE_Pagefile_Memory_Pool Pool; // default size is 16 MB
  typedef HANDLE SharedSemaphore;
#elif !defined ACE_LACKS_SYSV_SHMEM
  typedef ACE_Shared_Memory_Pool Pool;   // default size is 768 KB?
  typedef sem_t SharedSemaphore;
#else
  typedef ACE_Local_Memory_Pool Pool;    // no shared memory support
  typedef int SharedSemaphore;
#endif

  typedef ACE_Malloc_T<Pool, ACE_Process_Mutex,
                       ACE_PI_Control_Block> Allocator;

protected:
  virtual DataLink* find_datalink_i(const RepoId& local_id,
                                    const RepoId& remote_id,
                                    const TransportBLOB& remote_data,
                                    bool remote_reliable,
                                    const ConnectionAttribs& attribs,
                                    bool active);

  virtual DataLink* connect_datalink_i(const RepoId& local_id,
                                       const RepoId& remote_id,
                                       const TransportBLOB& remote_data,
                                       bool remote_reliable,
                                       const ConnectionAttribs& attribs);

  virtual DataLink* accept_datalink(ConnectionEvent& ce);
  virtual void stop_accepting(ConnectionEvent& ce);

  virtual bool configure_i(TransportInst* config);

  virtual void shutdown_i();

  virtual bool connection_info_i(TransportLocator& info) const;

  virtual void release_datalink(DataLink* link);

  virtual std::string transport_type() const { return "shmem"; }

private:
  ShmemDataLink* make_datalink(const std::string& remote_address, bool active);

  std::pair<std::string, std::string> blob_to_key(const TransportBLOB& blob);

  void read_from_links() {} // callback from ReadTask

  RcHandle<ShmemInst> config_i_;

  typedef ACE_Thread_Mutex        LockType;
  typedef ACE_Guard<LockType>     GuardType;

  /// This lock is used to protect the client_links_ data member.
  LockType links_lock_;

  /// Map of fully associated DataLinks for this transport.  Protected
  // by links_lock_.
  typedef std::map<std::string, ShmemDataLink_rch> ShmemDataLinkMap;
  ShmemDataLinkMap links_;

  /// This protects the pending_connections_ data member.
  LockType connections_lock_;

  /// Locked by connections_lock_.  Tracks expected connections
  /// that we have learned about in accept_datalink() but have
  /// not yet performed the handshake.
  std::multimap<ConnectionEvent*, std::string> pending_connections_;

  std::set<std::string> pending_link_keys_;

  Allocator* alloc_;

  struct ReadTask : ACE_Task_Base {
    ReadTask(ShmemTransport* outer, ACE_sema_t semaphore);
    int svc();
    void stop();

    ShmemTransport* outer_;
    ACE_sema_t semaphore_;
    bool stopped_;

  }* read_task_;

  std::string hostname_, poolname_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif  /* OPENDDS_SHMEMTRANSPORT_H */
