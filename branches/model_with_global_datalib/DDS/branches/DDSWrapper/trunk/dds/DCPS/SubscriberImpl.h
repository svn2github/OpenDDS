// -*- C++ -*-
//
// $Id$
#ifndef TAO_DDS_DCPS_SUBSCRIBER_H
#define TAO_DDS_DCPS_SUBSCRIBER_H

#include "dds/DdsDcpsSubscriptionExtS.h"
#include "dds/DdsDcpsDataReaderRemoteC.h"
#include "dds/DdsDcpsInfoC.h"
#include "EntityImpl.h"
#include "Definitions.h"
#include "dds/DCPS/transport/framework/TransportInterface.h"
#include "ace/Synch.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include <map>
#include <set>
#include <list>
#include <vector>

namespace OpenDDS
{
  namespace DCPS
  {
    // Forward declarations
    class DomainParticipantImpl;
    class DataReaderImpl ;

    // Keep track of all the DataReaders attached to this
    // Subscriber
    struct  SubscriberDataReaderInfo
    {
      ::OpenDDS::DCPS::DataReaderRemote_ptr remote_reader_objref_ ;
      ::DDS::DataReader_ptr       local_reader_objref_;
      DataReaderImpl*             local_reader_impl_;
      RepoId                      topic_id_ ;
      RepoId                      subscription_id_ ;
    } ;

    typedef std::multimap<ACE_CString,
                          SubscriberDataReaderInfo*> DataReaderMap ;

    // Keep track of DataReaders with data
    // std::set for now, want to encapsulate
    // this so we can switch between a set or
    // list depending on Presentation Qos.
    typedef std::set<DataReaderImpl *> DataReaderSet ;

    // DataReader id to qos map.
    typedef std::map<RepoId, ::DDS::DataReaderQos, GUID_tKeyLessThan> DrIdToQosMap;

    //Class SubscriberImpl
    class OpenDDS_Dcps_Export SubscriberImpl
      : public virtual OpenDDS::DCPS::LocalObject<SubscriberExt>,
        public virtual EntityImpl,
        public virtual TransportInterface
    {
    public:

      //Constructor
      SubscriberImpl (const ::DDS::SubscriberQos & qos,
                      ::DDS::SubscriberListener_ptr a_listener,
                      DomainParticipantImpl*       participant);

      //Destructor
      virtual ~SubscriberImpl (void);

      virtual ::DDS::DataReader_ptr create_datareader (
        ::DDS::TopicDescription_ptr a_topic_desc,
        const ::DDS::DataReaderQos & qos,
        ::DDS::DataReaderListener_ptr a_listener
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

      virtual ::DDS::DataReader_ptr create_opendds_datareader (
        ::DDS::TopicDescription_ptr a_topic_desc,
        const ::DDS::DataReaderQos & qos,
        const DataReaderQosExt & ext_qos,
        ::DDS::DataReaderListener_ptr a_listener
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t delete_datareader (
        ::DDS::DataReader_ptr a_datareader
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t delete_contained_entities (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::DataReader_ptr lookup_datareader (
        const char * topic_name
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t get_datareaders (
        ::DDS::DataReaderSeq_out readers,
        ::DDS::SampleStateMask sample_states,
        ::DDS::ViewStateMask view_states,
        ::DDS::InstanceStateMask instance_states
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual void notify_datareaders (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t set_qos (
        const ::DDS::SubscriberQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual void get_qos (
        ::DDS::SubscriberQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t set_listener (
        ::DDS::SubscriberListener_ptr a_listener,
        ::DDS::StatusKindMask mask
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::SubscriberListener_ptr get_listener (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t begin_access (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t end_access (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::DomainParticipant_ptr get_participant (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t set_default_datareader_qos (
        const ::DDS::DataReaderQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual void get_default_datareader_qos (
        ::DDS::DataReaderQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual void get_default_datareader_qos_ext (
        DataReaderQosExt & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t copy_from_topic_qos (
        ::DDS::DataReaderQos & a_datareader_qos,
        const ::DDS::TopicQos & a_topic_qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    virtual ::DDS::ReturnCode_t enable (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));


    /** This method is not defined in the IDL and is defined for
    *  internal use.
    *  Check if there is any datareader associated with it.
    */
    int is_clean () const;

    void add_associations (
                const WriterAssociationSeq& writers,
                DataReaderImpl* reader,
                const ::DDS::DataReaderQos reader_qos) ;

    void remove_associations(
        const WriterIdSeq& writers,
        const RepoId&      reader
      ) ;

    // called by DataReaderImpl::data_received
    void data_received(DataReaderImpl *reader);

    void reader_enabled(DataReaderRemote_ptr     remote_reader,
                        ::DDS::DataReader_ptr    local_reader,
			DataReaderImpl*          local_reader_impl,
                        const char *topic_name,
                        RepoId topic_id
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ));

    //void cleanup();

    ::DDS::SubscriberListener* listener_for (::DDS::StatusKind kind);

    private:

      ::DDS::SubscriberQos          qos_;
      ::DDS::DataReaderQos          default_datareader_qos_;

      DDS::StatusKindMask           listener_mask_;
      ::DDS::SubscriberListener_var  listener_;
      ::DDS::SubscriberListener* fast_listener_;

      DataReaderMap                 datareader_map_ ;
      DataReaderSet                 datareader_set_ ;

      DomainParticipantImpl*        participant_;
      ::DDS::DomainParticipant_var  participant_objref_;

      ::DDS::DomainId_t             domain_id_;

      /// this lock protects the data structures in this class.
      /// It also projects the TransportInterface (it must be held when
      /// calling any TransportInterface method).
      ACE_Recursive_Thread_Mutex    si_lock_;
    };

  } // namespace DCPS
} // namespace OpenDDS

#endif /* TAO_DDS_DCPS_SUBSCRIBER_H  */
