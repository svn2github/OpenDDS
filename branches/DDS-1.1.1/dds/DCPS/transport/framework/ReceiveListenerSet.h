// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_RECEIVELISTENERSET_H
#define OPENDDS_DCPS_RECEIVELISTENERSET_H

#include "dds/DCPS/RcObject_T.h"
#include "dds/DdsDcpsInfoUtilsC.h"
#include "dds/DdsDcpsDataWriterRemoteC.h"
#include "ace/Synch.h"

#include <map>

namespace OpenDDS
{

  namespace DCPS
  {

    class TransportReceiveListener;
    class ReceivedDataSample;


    class OpenDDS_Dcps_Export ReceiveListenerSet :
                                         public RcObject<ACE_SYNCH_MUTEX>
    {
      public:

        typedef std::map<RepoId, TransportReceiveListener*> MapType;

        ReceiveListenerSet();
        virtual ~ReceiveListenerSet();

        int insert(RepoId                    subscriber_id,
                   TransportReceiveListener* listener);
        int remove(RepoId subscriber_id);

        ssize_t size() const;

        void data_received(const ReceivedDataSample& sample);

        /// Give access to the underlying map for iteration purposes.
        //MapType& map();
        //const MapType& map() const;

        /// Check if the key is in the map and if it's the only left entry
        /// in the map.
        bool exist (const RepoId& key, 
                    bool& last);

        void  get_keys (ReaderIdSeq & ids);

        bool exist (const RepoId& local_id);

      private:

        typedef ACE_SYNCH_MUTEX     LockType;
        typedef ACE_Guard<LockType> GuardType;

        /// This lock will protect the map.
        mutable LockType lock_;

        MapType  map_;
    };

  }  /* namespace DCPS */

}  /* namespace OpenDDS */

#if defined (__ACE_INLINE__)
#include "ReceiveListenerSet.inl"
#endif /* __ACE_INLINE__ */

#endif /* OPENDDS_DCPS_RECEIVELISTENERSET_H */
