// -*- C++ -*-

//=============================================================================
/**
 *  @file   DataCache.h
 *
 *  $Id$
 *
 *  Underlying data cache for both OpenDDS @c TRANSIENT and
 *  @c PERSISTENT @c DURABILITY implementations.
 *
 *  @author Ossama Othman <othmano@ociweb.com>
 */
//=============================================================================

#ifndef OPENDDS_DATA_DURABILITY_CACHE_H
#define OPENDDS_DATA_DURABILITY_CACHE_H

#include "dds/DdsDcpsInfrastructureC.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "dds/DCPS/DataSampleList.h"
#include "dds/DCPS/DurabilityArray.h"
#include "dds/DCPS/DurabilityQueue.h"

#include "ace/Hash_Map_With_Allocator_T.h"
#include "ace/Array_Base.h"
#include "ace/String_Base.h"
#include "ace/SStringfwd.h"
#include "ace/Thread_Mutex.h"
#include "ace/Null_Mutex.h"
#include "ace/Synch_Traits.h"
#include "ace/Functor_T.h"

#include <string>
#include <list>
#include <memory>
#include <utility>


class ACE_Message_Block;

namespace DDS
{
  struct DurabilityServiceQosPolicy;
  struct LifespanQosPolicy;
}

namespace OpenDDS
{
  namespace DCPS
  {
    class WriteDataContainer;
    class DataWriterImpl;
    class DataSampleList;

    /**
     * @class DataDurabilityCache
     *
     * @brief Underlying data cache for both OpenDDS @c TRANSIENT and
     *        @c PERSISTENT @c DURABILITY implementations..
     *
     * This class implements a cache that outlives @c DataWriters.
     */
    class DataDurabilityCache
    {
    public:

      /**
       * @class key_type
       *
       * @brief Key type for underlying maps.
       *
       * Each sample may be uniquely identified by its domain ID,
       * topic name and type name.  We use that property to establish
       * a map key type.
       */
      class key_type
      {
      public:

        key_type ()
          : domain_id_ ()
          , topic_name_ ()
          , type_name_ ()
        {
        }

        key_type (::DDS::DomainId_t domain_id,
                  char const * topic,
                  char const * type,
                  ACE_Allocator * allocator)
          : domain_id_ (domain_id)
          , topic_name_ (topic, allocator)
          , type_name_ (type, allocator)
        {
        }

        key_type (key_type const & rhs)
          : domain_id_ (rhs.domain_id_)
          , topic_name_ (rhs.topic_name_)
          , type_name_ (rhs.type_name_)
        {
        }

        key_type & operator= (key_type const & rhs)
        {
          this->domain_id_ = rhs.domain_id_;
          this->topic_name_ = rhs.topic_name_;
          this->type_name_ = rhs.type_name_;

          return *this;
        }

        bool operator== (key_type const & rhs) const
        {
          return
            this->domain_id_ == rhs.domain_id_
            && this->topic_name_ == rhs.topic_name_
            && this->type_name_ == rhs.type_name_;
        }

        bool operator< (key_type const & rhs) const
        {
          return
            this->domain_id_ < rhs.domain_id_
            && this->topic_name_ < rhs.topic_name_
            && this->type_name_ < rhs.type_name_;
        }

        u_long hash () const
        {
          return
            static_cast<u_long> (this->domain_id_)
            + this->topic_name_.hash()
            + this->type_name_.hash ();
        }

      private:

        ::DDS::DomainId_t domain_id_;
        ACE_CString topic_name_;
        ACE_CString type_name_;

      };

      /**
       * @class sample_data_type
       *
       * @brief Sample list data type for all samples.
       */
      class sample_data_type
      {
      public:

        sample_data_type ();
        explicit sample_data_type (DataSampleListElement & element,
                                   ACE_Allocator * allocator);
        sample_data_type (sample_data_type const & rhs);

        ~sample_data_type ();

        sample_data_type & operator= (sample_data_type const & rhs);

        void get_sample (char const *& s,
                         size_t & len,
                         ::DDS::Time_t & source_timestamp);

        void set_allocator (ACE_Allocator * allocator);

      private:

        size_t length_;
        char * sample_;
        ::DDS::Time_t source_timestamp_;
        ACE_Allocator * allocator_;

      };

      /**
       * @typedef Define an ACE array of ACE queues to simplify
       *          access to data corresponding to a specific
       *          DurabilityServiceQosPolicy's cleanup delay.
       */
      typedef DurabilityArray<
        DurabilityQueue<sample_data_type> *> sample_list_type;

      typedef ACE_Hash_Map_With_Allocator<key_type,
                                          sample_list_type *> sample_map_type;
      typedef std::list<long> timer_id_list_type;

      /// Constructor.
      DataDurabilityCache (::DDS::DurabilityQosPolicyKind kind);

      /// Destructor.
      ~DataDurabilityCache ();

      /// Insert the samples corresponding to the given topic instance
      /// (uniquely identify by its domain, topic name and type name)
      /// into the data durability cache.
      bool insert (::DDS::DomainId_t domain_id,
                   char const * topic_name,
                   char const * type_name,
                   DataSampleList & the_data,
                   ::DDS::DurabilityServiceQosPolicy const & qos);

      /// Write cached data corresponding to given domain, topic and
      /// type to @c DataWriter.
      bool get_data (::DDS::DomainId_t domain_id,
                     char const * topic_name,
                     char const * type_name,
                     DataWriterImpl * data_writer,
                     ACE_Allocator * mb_allocator,
                     ACE_Allocator * db_allocator,
                     ::DDS::LifespanQosPolicy const & /* lifespan */);

    private:

      // Prevent copying.
      DataDurabilityCache (DataDurabilityCache const &);
      DataDurabilityCache & operator=  (DataDurabilityCache const &);

      /// Make allocator suitable to support specified kind of
      /// @c DURABILITY.
      static std::auto_ptr<ACE_Allocator>
      make_allocator (::DDS::DurabilityQosPolicyKind kind);

    private:

      /// Allocator used to allocate memory for sample map and lists.
      /**
       * This allocator will either be an ACE_New_Allocator for the
       * TRANSIENT durability case or an mmap()-based allocator for
       * PERSISTENT durability.
       */
      std::auto_ptr<ACE_Allocator> const allocator_;

      /// Map of all data samples.
      sample_map_type * samples_;

      /// Timer ID list.
      /**
       * Keep track of cleanup timer IDs in case we need to cancel
       * before they expire.
       */
      timer_id_list_type cleanup_timer_ids_;

      /// Lock for synchronized access to the underlying map.
      ACE_SYNCH_MUTEX lock_;

      /// Reactor with which cleanup timers will be registered.
      ACE_Reactor * reactor_;

    };

  } // DCPS
} // OpenDDS


#endif  /* OPENDDS_DATA_DURABILITY_CACHE_H */
