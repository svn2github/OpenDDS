// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_REPOIDSET_H
#define OPENDDS_DCPS_REPOIDSET_H

#include "TransportDefs.h"
#include "dds/DCPS/dcps_export.h"
#include "dds/DCPS/RcObject_T.h"
#include "dds/DdsDcpsInfoUtilsC.h"
#include "dds/DCPS/Serializer.h"
#include "ace/Synch.h"

#include <map>

namespace OpenDDS
{

  namespace DCPS
  {

    class OpenDDS_Dcps_Export RepoIdSet : public RcObject<ACE_SYNCH_MUTEX>
    {
      public:

        typedef std::map<RepoId, RepoId> MapType;

        RepoIdSet();
        virtual ~RepoIdSet();

        int insert_id(RepoId key, RepoId value);
        int remove_id(RepoId id);

        size_t size() const;

        /// Give access to the underlying map for iteration purposes.
        MapType& map();
        const MapType& map() const;

        /// Serialize the map. The data order in the serialized 
        /// stream: size of map, list of keys in the map.
        void serialize(TAO::DCPS::Serializer & serializer);

        /// Check if contents in the two RepoIdSet are same.
        bool is_subset (RepoIdSet& map);

        bool exist (const RepoId& remote_id, 
                    bool& last);

      private:

        MapType  map_;
    };

  }  /* namespace DCPS */

}  /* namespace OpenDDS */

#if defined (__ACE_INLINE__)
#include "RepoIdSet.inl"
#endif /* __ACE_INLINE__ */

#endif /* OPENDDS_DCPS_REPOIDSET_H */
