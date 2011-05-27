// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h"
#include  "PublisherImpl.h"
#include  "DomainParticipantImpl.h"
#include  "DataWriterImpl.h"
#include  "Service_Participant.h"
#include  "Qos_Helper.h"
#include  "Marked_Default_Qos.h"
#include  "TopicImpl.h"
#include  "dds/DdsDcpsTypeSupportTaoS.h"
#include  "dds/DCPS/transport/framework/ReceivedDataSample.h"
#include  "AssociationData.h"
#include  "dds/DCPS/transport/framework/TransportInterface.h"
#include  "dds/DCPS/transport/framework/DataLinkSet.h"
#include  "dds/DCPS/transport/framework/TransportImpl.h"
#include  "tao/debug.h"

namespace TAO
{
  namespace DCPS
  {
    const CoherencyGroup DEFAULT_GROUP_ID = 0;

    //TBD - add check for enabled in most methods.
    //      currently this is not needed because auto_enable_created_entities
    //      cannot be false.

    // Implementation skeleton constructor
    PublisherImpl::PublisherImpl (const ::DDS::PublisherQos & qos,
                                  ::DDS::PublisherListener_ptr a_listener,
                                  DomainParticipantImpl*       participant,
                                  ::DDS::DomainParticipant_ptr participant_objref)
      : qos_(qos),
        default_datawriter_qos_(TheServiceParticipant->initial_DataWriterQos ()),
        listener_mask_(DEFAULT_STATUS_KIND_MASK), 
        fast_listener_ (0),
        group_id_ (DEFAULT_GROUP_ID),
        repository_ (TheServiceParticipant->get_repository ()),
        participant_ (participant),
        participant_objref_ (::DDS::DomainParticipant::_duplicate (participant_objref)),
        suspend_depth_count_ (0),
        sequence_number_ (),
        aggregation_period_start_ (ACE_Time_Value::zero)
    {
       participant_->_add_ref ();

      //Note: OK to duplicate a nil.
      listener_ = ::DDS::PublisherListener::_duplicate(a_listener);
      if (! CORBA::is_nil (a_listener))
        {
          fast_listener_ = reference_to_servant<POA_DDS::PublisherListener, 
                                               DDS::PublisherListener_ptr> 
                            (listener_.in() ACE_ENV_ARG_PARAMETER);
          ACE_CHECK;
        }
    }

    // Implementation skeleton destructor
    PublisherImpl::~PublisherImpl (void)
      {
        participant_->_remove_ref ();
 
        // Tell the transport to detach this 
        // Publisher/TransportInterface.
        this->detach_transport ();

        //The datawriters should be deleted already before calling delete 
        //publisher.
        if (! is_clean ())
          {
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::~PublisherImpl, ")
                        ACE_TEXT("some datawriters still exist.\n")));
          }
      }
      
    //Note: caller should NOT assign to DataWriter_var (without _duplicate'ing)
    //      because it will steal the framework's reference.
    ::DDS::DataWriter_ptr PublisherImpl::create_datawriter (
        ::DDS::Topic_ptr a_topic,
        const ::DDS::DataWriterQos & qos,
        ::DDS::DataWriterListener_ptr a_listener
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (CORBA::is_nil (a_topic))
          {
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::create_datawriter, ")
                        ACE_TEXT("topic is nil.\n")));
            return ::DDS::DataWriter::_nil();
          }

        ::DDS::DataWriterQos dw_qos;
        if (qos == DATAWRITER_QOS_DEFAULT) 
          {
            this->get_default_datawriter_qos(dw_qos ACE_ENV_ARG_PARAMETER);
            ACE_CHECK_RETURN (::DDS::DataWriter::_nil());
          }
        else if (qos == DATAWRITER_QOS_USE_TOPIC_QOS)
          {
            ::DDS::TopicQos topic_qos;
            a_topic->get_qos (topic_qos ACE_ENV_ARG_PARAMETER);
            ACE_CHECK_RETURN (::DDS::DataWriter::_nil());
              
            this->get_default_datawriter_qos(dw_qos ACE_ENV_ARG_PARAMETER);
            ACE_CHECK_RETURN (::DDS::DataWriter::_nil());
             
            this->copy_from_topic_qos (dw_qos, topic_qos ACE_ENV_ARG_PARAMETER);
            ACE_CHECK_RETURN (::DDS::DataWriter::_nil());
          }
        else
          {
            dw_qos = qos;
          }

        if (! Qos_Helper::valid (dw_qos))
          {
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::create_datawriter, ")
                        ACE_TEXT("invalid qos.\n")));
            return ::DDS::DataWriter::_nil();
          }

        if (! Qos_Helper::consistent (dw_qos))
          {
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::create_datawriter, ")
                        ACE_TEXT("inconsistent qos.\n")));
            return ::DDS::DataWriter::_nil();
          }

        TopicImpl* topic_servant 
          = reference_to_servant<TopicImpl, 
                                 ::DDS::Topic_ptr> 
                                 (a_topic ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::DataWriter::_nil());

        POA_TAO::DCPS::TypeSupport_ptr typesupport = topic_servant->get_type_support();

        if (typesupport == 0)
          {
            CORBA::String_var name = topic_servant->get_name ();
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::create_datawriter, ")
                        ACE_TEXT("typesupport(topic_name=%s) is nil.\n"), 
                        name.in ()));
            return ::DDS::DataWriter::_nil ();
          }

        DataWriterRemote_var dw_obj = typesupport->create_datawriter ();
        
        DataWriterImpl* dw_servant = reference_to_servant <DataWriterImpl, 
                                                           ::DDS::DataWriter_ptr> 
                                       (dw_obj.in () ACE_ENV_SINGLE_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::DataWriter::_nil ());
        
        // Give owner ship to poa.
        dw_servant->_remove_ref ();

        DomainParticipantImpl* participant 
          = reference_to_servant<DomainParticipantImpl, ::DDS::DomainParticipant_ptr> 
              (participant_objref_.in ()  ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::DataWriter::_nil());

        dw_servant->init (a_topic,
                          topic_servant, 
                          dw_qos, 
                          a_listener, 
                          participant,
                          publisher_objref_.in (),
                          this,
                          dw_obj.in ()
                          ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::DataWriter::_nil());

        if (this->enabled_ == true 
            && qos_.entity_factory.autoenable_created_entities == 1)
          {
            ::DDS::ReturnCode_t ret 
              = dw_servant->enable (ACE_ENV_SINGLE_ARG_PARAMETER);
            ACE_CHECK_RETURN (::DDS::DataWriter::_nil ());

            if (ret != ::DDS::RETCODE_OK)
              {
                ACE_ERROR ((LM_ERROR, 
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("PublisherImpl::create_datawriter, ")
                            ACE_TEXT("enable failed.\n")));
                return ::DDS::DataWriter::_nil ();
              }
          }

        TAO::DCPS::TransportImpl_rch impl = this->get_transport_impl();
        if (impl.is_nil ())
          {
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::create_datawriter, ")
                        ACE_TEXT("the publisher has not been attached to the TransportImpl.\n")));
            return ::DDS::DataWriter::_nil ();
          }
        // Register the DataWriterImpl object with the TransportImpl.
        else if (impl->register_publication (dw_servant->get_publication_id(), 
                                             dw_servant) == -1)
          {
            ACE_ERROR ((LM_ERROR, 
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("PublisherImpl::create_datawriter, ")
                        ACE_TEXT("failed to register datawriter %d with TransportImpl.\n"),
                        dw_servant->get_publication_id()));
            return ::DDS::DataWriter::_nil ();
          }
        return ::DDS::DataWriter::_duplicate (dw_obj.in ());
      }
      
    ::DDS::ReturnCode_t PublisherImpl::delete_datawriter (
        ::DDS::DataWriter_ptr a_datawriter
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (enabled_ == false)
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: PublisherImpl::delete_datawriter, ")
                              ACE_TEXT(" Entity is not enabled. \n")),
                              ::DDS::RETCODE_NOT_ENABLED);
          }

        DataWriterImpl* dw_servant 
          = reference_to_servant <DataWriterImpl, ::DDS::DataWriter_ptr> 
              (a_datawriter ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::RETCODE_ERROR);

        if (dw_servant->get_publisher_servant () != this)
          {
            return ::DDS::RETCODE_PRECONDITION_NOT_MET;
          }

        CORBA::String_var topic_name = dw_servant->get_topic_name ();
        DataWriterImpl* local_writer = 0;
        RepoId publication_id  = 0;
        {
          ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                            guard, 
                            this->pi_lock_, 
                            ::DDS::RETCODE_ERROR);

          publication_id = dw_servant->get_publication_id ();
          PublicationMap::iterator it = publication_map_.find (publication_id);

          if (it == publication_map_.end ()) 
            {
              ACE_ERROR_RETURN ((LM_ERROR, 
                                 ACE_TEXT("(%P|%t) ERROR: ")
                                 ACE_TEXT("PublisherImpl::delete_datawriter, ")
                                 ACE_TEXT("The datawriter(repoid=%d) is not found\n"),
                                 publication_id),
                                 ::DDS::RETCODE_ERROR); 
            }
          local_writer = it->second->local_writer_;

          PublisherDataWriterInfo* dw_info = it->second;
          
          // We can not erase the datawriter from datawriter map by the topic name
          // because the map might have multiple datawriters with the same topic 
          // name.
          // Find the iterator to the datawriter in the datawriter map and erase 
          // by the iterator.
          DataWriterMap::iterator writ;
          DataWriterMap::iterator the_writ = datawriter_map_.end ();

          for (writ = datawriter_map_.begin (); 
               writ != datawriter_map_.end (); 
               writ ++)
            {
              if (writ->second == it->second)
                {
                  the_writ = writ;
                  break;
                }
            }
            
          if (the_writ != datawriter_map_.end ())
            {
              datawriter_map_.erase (the_writ);
            }

          publication_map_.erase (publication_id);

          TAO::DCPS::TransportImpl_rch impl = this->get_transport_impl();
          if (impl.is_nil ())
            {
              ACE_ERROR ((LM_ERROR, 
                          ACE_TEXT("(%P|%t) ERROR: ")
                          ACE_TEXT("PublisherImpl::delete_datawriter, ")
                          ACE_TEXT("the publisher has not been attached to the TransportImpl.\n")));
              return ::DDS::RETCODE_ERROR;
            }
          // Unregister the DataWriterImpl object with the TransportImpl.
          else if (impl->unregister_publication (publication_id) == -1)
            {
              ACE_ERROR ((LM_ERROR, 
                          ACE_TEXT("(%P|%t) ERROR: ")
                          ACE_TEXT("PublisherImpl::delete_datawriter, ")
                          ACE_TEXT("failed to unregister datawriter %d with TransportImpl.\n"),
                          publication_id));
              return ::DDS::RETCODE_ERROR;
            }
          
          delete dw_info;

          dw_servant->remove_all_associations();
          dw_servant->cleanup ();
        }

        // not just unregister but remove any pending writes/sends.
        local_writer->unregister_all ();

        ACE_TRY  
          { 
            this->repository_->remove_publication(
                  participant_->get_domain_id (), 
                  participant_->get_id (),  
                  publication_id
                  ACE_ENV_ARG_PARAMETER) ;
            ACE_TRY_CHECK;
          }
        ACE_CATCH (CORBA::SystemException, sysex)
          {
            ACE_PRINT_EXCEPTION (sysex, 
                                 "ERROR: System Exception"
                                  " in PublisherImpl::delete_datawriter");
            return ::DDS::RETCODE_ERROR;
          }
        ACE_CATCH (CORBA::UserException, userex)
          {
            ACE_PRINT_EXCEPTION (userex, 
                                 "ERROR: User Exception"
                                 " in PublisherImpl::delete_datawriter");
            return ::DDS::RETCODE_ERROR;
          }
        ACE_ENDTRY;
        
        // Decrease ref count after the servant is removed from the
        // map.
        local_writer->_remove_ref ();

        return ::DDS::RETCODE_OK;
      }
      
    ::DDS::DataWriter_ptr PublisherImpl::lookup_datawriter (
        const char * topic_name
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (enabled_ == false)
          {
            ACE_ERROR ((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: PublisherImpl::lookup_datawriter, ")
                        ACE_TEXT(" Entity is not enabled. \n")));
            return ::DDS::DataWriter::_nil ();
          }

        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                          guard, 
                          this->pi_lock_, 
                          ::DDS::DataWriter::_nil ());

        // If multiple entries whose key is "topic_name" then which one is 
        // returned ? Spec does not limit which one should give.
        DataWriterMap::iterator it = datawriter_map_.find (topic_name);
        if (it == datawriter_map_.end ()) 
          {
            if (DCPS_debug_level >= 2)
              {
                ACE_DEBUG ((LM_DEBUG, 
                            ACE_TEXT("(%P|%t) ")
                            ACE_TEXT("PublisherImpl::lookup_datawriter, ")
                            ACE_TEXT("The datawriter(topic_name=%s) is not found\n"),
                            topic_name)); 
              }
            return ::DDS::DataWriter::_nil ();
          }
        else 
          {
            return ::DDS::DataWriter::_duplicate (it->second->remote_writer_);
          }
      }
      
    ::DDS::ReturnCode_t PublisherImpl::delete_contained_entities (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (enabled_ == false)
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: PublisherImpl::delete_contained_entities, ")
                              ACE_TEXT(" Entity is not enabled. \n")),
                              ::DDS::RETCODE_NOT_ENABLED);
          }

        // mark that the entity is being deleted
        set_deleted (true);

        DataWriterMap::iterator it;
        DataWriterMap::iterator next;

        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                          guard, 
                          this->pi_lock_, 
                          ::DDS::RETCODE_ERROR);

        for (it = datawriter_map_.begin (); it != datawriter_map_.end (); )
          {
            // Get the iterator for next entry before erasing current entry since
            // the iterator will be invalid after deletion.
            next = it;
            next ++;
            ::DDS::ReturnCode_t ret = delete_datawriter (it->second->remote_writer_);
            if (ret != ::DDS::RETCODE_OK)
              {
                ACE_ERROR_RETURN ((LM_ERROR, 
                  ACE_TEXT("(%P|%t) ERROR: ")
                  ACE_TEXT("PublisherImpl::delete_contained_entities, ")
                  ACE_TEXT("failed to delete datawriter(publication_id=%d)\n"), 
                  it->second->publication_id_), ret);
              }
            it = next;
          }

        // the publisher can now start creating new publications
        set_deleted (false);

        return ::DDS::RETCODE_OK;
      }
      
    ::DDS::ReturnCode_t PublisherImpl::set_qos (
        const ::DDS::PublisherQos & qos
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
          {
            if (enabled_ == true) 
              {
                if (! Qos_Helper::changeable (qos_, qos))
                  {
                    return ::DDS::RETCODE_IMMUTABLE_POLICY;
                  }
              }
            if (! (qos_ == qos))
              {
                qos_ = qos;
                // TBD - when there are changable QoS supported
                //       this code may need to do something
                //       with the changed values.
                // TBD - when there are changable QoS then we
                //       need to tell the DCPSInfo/repo about
                //       the changes in Qos.
                // tbd: why ? The repo does not know the publisher.
                // repository_->set_qos(qos_);
              }
            return ::DDS::RETCODE_OK;
          }
        else 
          {
            return ::DDS::RETCODE_INCONSISTENT_POLICY;
          }
      }
      
    void PublisherImpl::get_qos (
        ::DDS::PublisherQos & qos
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        qos = qos_;
      }
      
    ::DDS::ReturnCode_t PublisherImpl::set_listener (
        ::DDS::PublisherListener_ptr a_listener,
        ::DDS::StatusKindMask mask
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        listener_mask_ = mask;
        //note: OK to duplicate  and reference_to_servant a nil object ref
        listener_ = ::DDS::PublisherListener::_duplicate(a_listener);
        fast_listener_ 
          = reference_to_servant< ::POA_DDS::PublisherListener, 
                                  ::DDS::PublisherListener_ptr > 
            (listener_.in () ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::RETCODE_ERROR);
        return ::DDS::RETCODE_OK;
      }
      
    ::DDS::PublisherListener_ptr PublisherImpl::get_listener (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        return ::DDS::PublisherListener::_duplicate (listener_.in ()); 
      }
      
    ::DDS::ReturnCode_t PublisherImpl::suspend_publications (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (enabled_ == false)
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: PublisherImpl::suspend_publications, ")
                              ACE_TEXT(" Entity is not enabled. \n")),
                              ::DDS::RETCODE_NOT_ENABLED);
          }

        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                          guard, 
                          this->pi_lock_, 
                          ::DDS::RETCODE_ERROR);
        suspend_depth_count_ ++;
        return ::DDS::RETCODE_OK;
      }
      
    ::DDS::ReturnCode_t PublisherImpl::resume_publications (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (enabled_ == false)
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: PublisherImpl::resume_publications, ")
                              ACE_TEXT(" Entity is not enabled. \n")),
                              ::DDS::RETCODE_NOT_ENABLED);
          }
        
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                          guard, 
                          this->pi_lock_, 
                          ::DDS::RETCODE_ERROR);

        suspend_depth_count_ --;
        if (suspend_depth_count_ < 0)
          {
            suspend_depth_count_ = 0;
            return ::DDS::RETCODE_PRECONDITION_NOT_MET;
          }

        if (suspend_depth_count_ == 0)
          {
            this->send (available_data_list_);
            available_data_list_.head_ = available_data_list_.tail_ = 0;
          }

        return ::DDS::RETCODE_OK;
      }
      
    ::DDS::ReturnCode_t PublisherImpl::begin_coherent_changes (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        //NOT REQUIRED FOR FIRST IMPLEMENTATION
        return ::DDS::RETCODE_UNSUPPORTED;
      }
      
    ::DDS::ReturnCode_t PublisherImpl::end_coherent_changes (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        //NOT REQUIRED FOR FIRST IMPLEMENTATION
        return ::DDS::RETCODE_UNSUPPORTED;
      }
      
    ::DDS::DomainParticipant_ptr PublisherImpl::get_participant (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        return ::DDS::DomainParticipant::_duplicate (participant_objref_.in ());
      }
      
    ::DDS::ReturnCode_t PublisherImpl::set_default_datawriter_qos (
        const ::DDS::DataWriterQos & qos
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
          {
            default_datawriter_qos_ = qos;
            return ::DDS::RETCODE_OK;
          }
        else 
          {
            return ::DDS::RETCODE_INCONSISTENT_POLICY;
          }
      }
      
    void PublisherImpl::get_default_datawriter_qos (
        ::DDS::DataWriterQos & qos
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        qos = default_datawriter_qos_;
      }
      
    ::DDS::ReturnCode_t PublisherImpl::copy_from_topic_qos (
        ::DDS::DataWriterQos & a_datawriter_qos,
        const ::DDS::TopicQos & a_topic_qos
        ACE_ENV_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
         if (Qos_Helper::valid(a_topic_qos) 
           && Qos_Helper::consistent(a_topic_qos)) 
          {
            // Some members in the DataWriterQos are not contained
            // in the TopicQos. The caller needs initialize them.
            a_datawriter_qos.durability = a_topic_qos.durability;
            a_datawriter_qos.deadline = a_topic_qos.deadline;
            a_datawriter_qos.latency_budget = a_topic_qos.latency_budget;
            a_datawriter_qos.liveliness = a_topic_qos.liveliness;
            a_datawriter_qos.reliability = a_topic_qos.reliability;
            a_datawriter_qos.destination_order = a_topic_qos.destination_order;
            a_datawriter_qos.history = a_topic_qos.history;
            a_datawriter_qos.resource_limits = a_topic_qos.resource_limits;
            a_datawriter_qos.transport_priority = a_topic_qos.transport_priority;
            a_datawriter_qos.lifespan = a_topic_qos.lifespan;

            return ::DDS::RETCODE_OK;
          }
        else 
          {
            return ::DDS::RETCODE_INCONSISTENT_POLICY;
          }
      }
      
    ::DDS::ReturnCode_t PublisherImpl::enable (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        //TDB - check if factory is enables and then enable all entities 
        // (don't need to do it for now because
        //  entity_factory.autoenable_created_entities is always = 1)

        //if (factory not enabled)
        //{
        //  return ::DDS::RETCODE_PRECONDITION_NOT_MET;
        //}
        
        this->set_enabled ();
        return ::DDS::RETCODE_OK;
      }
      
    ::DDS::StatusKindMask PublisherImpl::get_status_changes (
        ACE_ENV_SINGLE_ARG_DECL
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
      {
        return EntityImpl::get_status_changes (ACE_ENV_SINGLE_ARG_PARAMETER);
      }


      int 
      PublisherImpl::is_clean () const
      {
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                          guard, 
                          this->pi_lock_,
                          -1);
        return datawriter_map_.empty () && publication_map_.empty ();
      }
 
      void PublisherImpl::add_associations (
          const ReaderAssociationSeq & readers,
          DataWriterImpl* writer,
          const ::DDS::DataWriterQos writer_qos)
      {
        if (entity_deleted_ == true)
          {
            if (DCPS_debug_level >= 1) 
              ACE_DEBUG ((LM_DEBUG,
                        ACE_TEXT("(%P|%t) PublisherImpl::add_associations")
                        ACE_TEXT(" This is a deleted publisher, ignoring add.\n")));
            return;
          }

        size_t length = readers.length ();
        AssociationData* associations = new AssociationData[length];
        for (size_t i = 0; i < length; i++)
          {
            associations[i].remote_id_ = readers[i].readerId;
            associations[i].remote_data_ = readers[i].readerTransInfo;
          }

        this->add_subscriptions (writer->get_publication_id (), 
                                 writer_qos.transport_priority.value,
                                 length,
                                 associations);
        delete []associations; // TransportInterface does not take ownership
      }

      void PublisherImpl::remove_associations(
        const ReaderIdSeq & readers)
      {
        // Delegate to the (inherited) TransportInterface version.

        // TMB - I don't know why I have to do it this way, but the compiler
        //       on linux complains with an error otherwise.
        this->TransportInterface::remove_associations(readers.length(),
                                                      readers.get_buffer());
      }

      ::DDS::ReturnCode_t PublisherImpl::writer_enabled(
        DataWriterRemote_ptr     writer,
        const char*              topic_name,
        //BuiltinTopicKey_t topic_key
        RepoId                   topic_id)
      {
        PublisherDataWriterInfo* info = new PublisherDataWriterInfo;
        info->remote_writer_ = writer ;
        info->local_writer_  
          = reference_to_servant<DataWriterImpl, DataWriterRemote_ptr> 
              (writer ACE_ENV_ARG_PARAMETER);
        ACE_CHECK_RETURN (::DDS::RETCODE_ERROR);

        info->topic_id_      = topic_id ;
        // all other info memebers default in constructor

        /// Load the publication into the repository and get the
        /// publication_id_ in return.
        ACE_TRY  
          { 
            ::DDS::DataWriterQos qos;
            info->remote_writer_->get_qos(qos ACE_ENV_ARG_PARAMETER);
            ACE_TRY_CHECK;

            TAO::DCPS::TransportInterfaceInfo trans_conf_info = connection_info ();

            info->publication_id_
              = this->repository_->add_publication(
                  participant_->get_domain_id (), // Loaded during Publisher construction
                  participant_->get_id (),  // Loaded during Publisher construction.
                  info->topic_id_, // Loaded during DataWriter construction.
                  info->remote_writer_,
                  qos,
                  trans_conf_info ,   // Obtained during setup.
                  qos_
                  ACE_ENV_ARG_PARAMETER) ;
            ACE_TRY_CHECK;
            info->local_writer_->set_publication_id (info->publication_id_);
          }
        ACE_CATCH (CORBA::SystemException, sysex)
          {
            ACE_PRINT_EXCEPTION (sysex, 
                                 "ERROR: System Exception"
                                  " in PublisherImpl::writer_enabled");
            return ::DDS::RETCODE_ERROR;
          }
        ACE_CATCH (CORBA::UserException, userex)
          {
            ACE_PRINT_EXCEPTION (userex, 
                                 "ERROR:  Exception"
                                 " in PublisherImpl::writer_enabled");
            return ::DDS::RETCODE_ERROR;
          }
        ACE_ENDTRY;

        {
          ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                            guard, 
                            this->pi_lock_,
                            ::DDS::RETCODE_ERROR);
          DataWriterMap::iterator it
            = datawriter_map_.insert(DataWriterMap::value_type(topic_name, info));
          if (it == datawriter_map_.end ())
            {
              ACE_ERROR_RETURN ((LM_ERROR, 
                                 ACE_TEXT("(%P|%t) ERROR: PublisherImpl::writer_enabled, ")
                                 ACE_TEXT("insert datawriter(topic_name=%s) failed. \n"),
                                 topic_name),
                                 ::DDS::RETCODE_ERROR);
            }

          std::pair<PublicationMap::iterator, bool> pair 
            = publication_map_.insert(PublicationMap::value_type(info->publication_id_, info));

          if (pair.second == false)
            {
              ACE_ERROR_RETURN ((LM_ERROR, 
                                 ACE_TEXT("(%P|%t) ERROR: PublisherImpl::writer_enabled, ")
                                 ACE_TEXT("insert publication(id=%d) failed. \n"),
                                 info->publication_id_),
                                 ::DDS::RETCODE_ERROR);
            }

          // Increase ref count when the servant is added to the 
          // datawriter/publication map.
          info->local_writer_->_add_ref ();
        }

        return ::DDS::RETCODE_OK;
      }

      void
      PublisherImpl::set_object_reference (const ::DDS::Publisher_ptr& pub)
      {
        if (! CORBA::is_nil (publisher_objref_.in ()))
        {
          ACE_ERROR ((LM_ERROR, 
                      ACE_TEXT("(%P|%t) ERROR: PublisherImpl::set_object_reference, ")
                      ACE_TEXT("This publisher is already activated. \n")));
          return;
        }

        publisher_objref_ = ::DDS::Publisher::_duplicate (pub);
      }

      ::DDS::ReturnCode_t
      PublisherImpl::data_available(DataWriterImpl* writer)
      {
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex, 
                          guard, 
                          this->pi_lock_,
                          ::DDS::RETCODE_ERROR);

        DataSampleList list = writer->get_unsent_data() ;

        if( this->suspend_depth_count_ > 0) 
          {
            // append list to the avaliable data list.
            // Collect samples from all of the Publisher's Datawriters
            // in this list so when resume_publication is called 
            // the Publisher does not have to iterate over its
            // DataWriters to get the unsent data samples.
            available_data_list_.enqueue_tail_next_send_sample (list);
          } 
        else 
        {
          // Do LATENCY_BUDGET processing here.
          // Do coherency processing here.
          // tell the transport to send the data sample(s).
          this->send(list) ;
        }
        return ::DDS::RETCODE_OK;
      }

      ::POA_DDS::PublisherListener*
      PublisherImpl::listener_for (::DDS::StatusKind kind)
      {
       // per 2.1.4.3.1 Listener Access to Plain Communication Status
       // use this entities factory if listener is mask not enabled
       // for this kind.
       if ((listener_mask_ & kind) == 0)
          {
            return participant_->listener_for (kind);
          }
        else 
          {
            return fast_listener_;
          }
      }
  } // namespace DCPS
} // namespace TAO


#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)

template class std::multimap<ACE_CString, PublisherDataWriterInfo*>;
template class std::map< PublicationId, PublisherDataWriterInfo*>;
template class std::vector< MessageData*> MessageDataList;

#elif defined(ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)

#pragma instantiate std::multimap<ACE_CString, PublisherDataWriterInfo*>;
#pragma instantiate std::map< PublicationId, PublisherDataWriterInfo*>;
#pragma instantiate std::vector< MessageData*> MessageDataList;

#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

