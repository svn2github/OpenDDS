/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_RTPSUDPDATALINK_H
#define DCPS_RTPSUDPDATALINK_H

#include "Rtps_Udp_Export.h"

#include "RtpsUdpSendStrategy.h"
#include "RtpsUdpSendStrategy_rch.h"
#include "RtpsUdpReceiveStrategy.h"
#include "RtpsUdpReceiveStrategy_rch.h"

#include "ace/Basic_Types.h"
#include "ace/SOCK_Dgram.h"
#include "ace/SOCK_Dgram_Mcast.h"

#include "dds/DCPS/transport/framework/DataLink.h"
#include "dds/DCPS/transport/framework/TransportReactorTask.h"
#include "dds/DCPS/transport/framework/TransportReactorTask_rch.h"
#include "dds/DCPS/transport/framework/TransportSendBuffer.h"

#include "dds/DCPS/DataSampleList.h"
#include "dds/DCPS/DisjointSequence.h"
#include "dds/DCPS/GuidConverter.h"

#include <map>
#include <set>

class DDS_TEST;

namespace OpenDDS {
namespace DCPS {

class RtpsUdpInst;
class RtpsUdpTransport;
class ReceivedDataSample;

class OpenDDS_Rtps_Udp_Export RtpsUdpDataLink : public DataLink {
public:

  RtpsUdpDataLink(RtpsUdpTransport* transport,
                  const GuidPrefix_t& local_prefix,
                  RtpsUdpInst* config,
                  TransportReactorTask* reactor_task);

  void send_strategy(RtpsUdpSendStrategy* send_strategy);
  void receive_strategy(RtpsUdpReceiveStrategy* recv_strategy);

  RtpsUdpInst* config();

  ACE_Reactor* get_reactor();

  ACE_SOCK_Dgram& unicast_socket();
  ACE_SOCK_Dgram_Mcast& multicast_socket();

  bool open();

  void control_received(ReceivedDataSample& sample,
                        const ACE_INET_Addr& remote_address);

  const GuidPrefix_t& local_prefix() const { return local_prefix_; }

  void add_locator(const RepoId& remote_id, const ACE_INET_Addr& address);

  /// Given a 'local_id' of a publication or subscription, populate the set of
  /// 'addrs' with the network addresses of any remote peers (or if 'local_id'
  /// is GUID_UNKNOWN, all known addresses).
  void get_locators(const RepoId& local_id,
                    std::set<ACE_INET_Addr>& addrs) const;

  void associated(const RepoId& local, const RepoId& remote, bool reliable);

private:
  virtual void stop_i();

  virtual TransportQueueElement* customize_queue_element(
    TransportQueueElement* element);

  virtual void release_remote_i(const RepoId& remote_id);
  virtual void release_reservations_i(const RepoId& remote_id,
                                      const RepoId& local_id);

  friend class ::DDS_TEST;
  /// static member used by testing code to force inline qos
  static bool force_inline_qos_;
  bool requires_inline_qos(const PublicationId& pub_id);

  RtpsUdpInst* config_;
  TransportReactorTask_rch reactor_task_;

  RtpsUdpSendStrategy_rch send_strategy_;
  RtpsUdpReceiveStrategy_rch recv_strategy_;

  GuidPrefix_t local_prefix_;
  std::map<RepoId, ACE_INET_Addr, GUID_tKeyLessThan> locators_;

  ACE_SOCK_Dgram unicast_socket_;
  ACE_SOCK_Dgram_Mcast multicast_socket_;

  TransportCustomizedElementAllocator transport_customized_element_allocator_;

  struct MultiSendBuffer : TransportSendBuffer {

    MultiSendBuffer(RtpsUdpDataLink* outer, size_t capacity)
      : TransportSendBuffer(capacity)
      , outer_(outer)
    {}

    void retain_all(RepoId pub_id);
    void insert(SequenceNumber sequence,
                TransportSendStrategy::QueueType* queue,
                ACE_Message_Block* chain);

    RtpsUdpDataLink* outer_;

  } multi_buff_;


  // RTPS reliability support for local writers:

  struct ReaderInfo {
    SequenceNumber seq_;
  };

  typedef std::map<RepoId, ReaderInfo, GUID_tKeyLessThan> ReaderInfoMap;

  struct RtpsWriter {
    ReaderInfoMap remote_readers_;
    RcHandle<SingleSendBuffer> send_buff_;
  };

  typedef std::map<RepoId, RtpsWriter, GUID_tKeyLessThan> RtpsWriterMap;
  RtpsWriterMap writers_;


  // RTPS reliability support for local readers:

  struct WriterInfo {
    DisjointSequence dis_;
  };

  typedef std::map<RepoId, WriterInfo, GUID_tKeyLessThan> WriterInfoMap;

  struct RtpsReader {
    WriterInfoMap remote_writers_;
  };

  typedef std::map<RepoId, RtpsReader, GUID_tKeyLessThan> RtpsReaderMap;
  RtpsReaderMap readers_;
};

} // namespace DCPS
} // namespace OpenDDS

#ifdef __ACE_INLINE__
# include "RtpsUdpDataLink.inl"
#endif  /* __ACE_INLINE__ */

#endif  /* DCPS_RTPSUDPDATALINK_H */
