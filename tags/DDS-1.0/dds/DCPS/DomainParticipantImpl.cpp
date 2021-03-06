// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "DomainParticipantImpl.h"
#include "Service_Participant.h"
#include "Qos_Helper.h"
#include "PublisherImpl.h"
#include "SubscriberImpl.h"
#include "Marked_Default_Qos.h"
#include "Registered_Data_Types.h"
#include "Transient_Kludge.h"
#include "Util.h"

#if !defined (DDS_HAS_MINIMUM_BIT)
#include "BuiltInTopicUtils.h"
#include "dds/ParticipantBuiltinTopicDataTypeSupportImpl.h"
#include "dds/PublicationBuiltinTopicDataTypeSupportImpl.h"
#include "dds/SubscriptionBuiltinTopicDataTypeSupportImpl.h"
#include "dds/TopicBuiltinTopicDataTypeSupportImpl.h"
#endif // !defined (DDS_HAS_MINIMUM_BIT)

#include "tao/debug.h"

namespace Util
{
  template <typename Key>
  int find(
    OpenDDS::DCPS::DomainParticipantImpl::TopicMap& c,
    const Key& key,
    OpenDDS::DCPS::DomainParticipantImpl::TopicMap::mapped_type*& value
    )
  {
    OpenDDS::DCPS::DomainParticipantImpl::TopicMap::iterator iter =
      c.find(key);
    if (iter == c.end())
    {
      return -1;
    }
    value = &iter->second;
    return 0;
  }
}

namespace OpenDDS
{
  namespace DCPS
  {
    //TBD - add check for enabled in most methods.
    //      Currently this is not needed because auto_enable_created_entities
    //      cannot be false.

    // Implementation skeleton constructor
    DomainParticipantImpl::DomainParticipantImpl (const ::DDS::DomainId_t&             domain_id,
                                                  const RepoId&                        dp_id,
                                                  const ::DDS::DomainParticipantQos &  qos,
                                                  ::DDS::DomainParticipantListener_ptr a_listener)
      : default_topic_qos_(TheServiceParticipant->initial_TopicQos()),
        default_publisher_qos_(TheServiceParticipant->initial_PublisherQos()),
        default_subscriber_qos_(TheServiceParticipant->initial_SubscriberQos()),
        qos_(qos),
        domain_id_(domain_id),
        dp_id_(dp_id)
    {
      repository_ = TheServiceParticipant->get_repository();
      DDS::ReturnCode_t ret;
      ret = this->set_listener(a_listener, DEFAULT_STATUS_KIND_MASK);
    }


    // Implementation skeleton destructor
    DomainParticipantImpl::~DomainParticipantImpl (void)
    {
    }


    ::DDS::Publisher_ptr
    DomainParticipantImpl::create_publisher (
        const ::DDS::PublisherQos & qos,
        ::DDS::PublisherListener_ptr a_listener
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      ::DDS::PublisherQos pub_qos;

      if (qos == PUBLISHER_QOS_DEFAULT)
        {
          this->get_default_publisher_qos(pub_qos);
        }
      else
        {
          pub_qos = qos;
        }

      if (! Qos_Helper::valid (pub_qos))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::create_publisher, ")
                      ACE_TEXT("invalid qos.\n")));
          return ::DDS::Publisher::_nil();
        }

      if (! Qos_Helper::consistent (pub_qos))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::create_publisher, ")
                      ACE_TEXT("inconsistent qos.\n")));
          return ::DDS::Publisher::_nil();
        }

      PublisherImpl* pub = 0;
      ACE_NEW_RETURN(pub,
                     PublisherImpl(pub_qos,
                                   a_listener,
                                   this,
                                   participant_objref_.in ()),
                     ::DDS::Publisher::_nil());

      if ((enabled_ == true) && (qos_.entity_factory.autoenable_created_entities == 1))
        {
          pub->enable();
        }

      ::DDS::Publisher_ptr pub_obj
        = servant_to_reference (pub);

      pub->set_object_reference (pub_obj);

      ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                        tao_mon,
                        this->publishers_protector_,
                        ::DDS::Publisher::_nil());

      Publisher_Pair pair(pub, pub_obj, NO_DUP);

      if (OpenDDS::DCPS::insert(publishers_, pair) == -1)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_publisher, ")
                      ACE_TEXT("%p\n"),
                      ACE_TEXT("insert")));
          return ::DDS::Publisher::_nil();
        }

      // Increase ref count when the servant is added to
      // publisher set.
      pub->_add_ref ();

      return ::DDS::Publisher::_duplicate (pub_obj);
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::delete_publisher (
        ::DDS::Publisher_ptr p
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_publisher, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      // The servant's ref count should be 2 at this point,
      // one referenced by poa, one referenced by the subscriber
      // set.
      PublisherImpl* the_servant = reference_to_servant<PublisherImpl> (p);

      if (the_servant->is_clean () == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::delete_publisher, ")
                      ACE_TEXT("The publisher is not empty.\n")));
          return ::DDS::RETCODE_PRECONDITION_NOT_MET;
        }

      ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                        tao_mon,
                        this->publishers_protector_,
                        ::DDS::RETCODE_ERROR);

      Publisher_Pair pair (the_servant, p, DUP);

      if (OpenDDS::DCPS::remove(publishers_, pair) == -1)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_publisher, ")
                      ACE_TEXT("%p\n"),
                      ACE_TEXT("remove")));
          return ::DDS::RETCODE_ERROR;
        }
      else
        {
          // Remove ref count after the servant is removed
          // from publisher set.
          the_servant->_remove_ref ();

          deactivate_object < ::DDS::Publisher_ptr > (p);

          return ::DDS::RETCODE_OK;
        }
    }


    ::DDS::Subscriber_ptr
    DomainParticipantImpl::create_subscriber (
        const ::DDS::SubscriberQos & qos,
        ::DDS::SubscriberListener_ptr a_listener
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      ::DDS::SubscriberQos sub_qos;
      if (qos == SUBSCRIBER_QOS_DEFAULT)
        {
          this->get_default_subscriber_qos(sub_qos);
        }
      else
        {
          sub_qos = qos;
        }

      if (! Qos_Helper::valid (sub_qos))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::create_subscriber, ")
                      ACE_TEXT("invalid qos.\n")));
          return ::DDS::Subscriber::_nil();
        }

      if (! Qos_Helper::consistent (sub_qos))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::create_subscriber, ")
                      ACE_TEXT("inconsistent qos.\n")));
          return ::DDS::Subscriber::_nil();
        }

      SubscriberImpl* sub = 0 ;
      ACE_NEW_RETURN(sub,
                     SubscriberImpl(sub_qos,
                                    a_listener,
                                    this,
                                    participant_objref_.in ()),
                     ::DDS::Subscriber::_nil());


      if ((enabled_ == true) && (qos_.entity_factory.autoenable_created_entities == 1))
        {
          sub->enable();
        }

      ::DDS::Subscriber_ptr sub_obj
        = servant_to_reference (sub);

      sub->set_object_reference (sub_obj);

      ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                        tao_mon,
                        this->subscribers_protector_,
                        ::DDS::Subscriber::_nil());

      Subscriber_Pair pair (sub, sub_obj, NO_DUP);

      if (OpenDDS::DCPS::insert(subscribers_, pair) == -1)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_subscriber, ")
                      ACE_TEXT("%p\n"),
                      ACE_TEXT("insert")));
          return ::DDS::Subscriber::_nil();
        }

      // Increase ref count when the servant is added to
      // subscriber set.
      sub->_add_ref ();

      return ::DDS::Subscriber::_duplicate (sub_obj);
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::delete_subscriber (
        ::DDS::Subscriber_ptr s
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_subscriber, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      // The servant's ref count should be 2 at this point,
      // one referenced by poa, one referenced by the subscriber
      // set.
      SubscriberImpl* the_servant = reference_to_servant<SubscriberImpl> (s);

      if (the_servant->is_clean () == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::delete_subscriber, ")
                      ACE_TEXT("The subscriber is not empty.\n")));
          return ::DDS::RETCODE_PRECONDITION_NOT_MET;
        }

      ::DDS::ReturnCode_t ret
        = the_servant->delete_contained_entities ();

      if (ret != ::DDS::RETCODE_OK)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::delete_subscriber, ")
                      ACE_TEXT("Failed to delete contained entities.\n")));
          return ::DDS::RETCODE_ERROR;
        }

      ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                        tao_mon,
                        this->subscribers_protector_,
                        ::DDS::RETCODE_ERROR);

      Subscriber_Pair pair (the_servant, s, DUP);

      if (OpenDDS::DCPS::remove(subscribers_, pair) == -1)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_subscriber, ")
                      ACE_TEXT("%p\n"),
                      ACE_TEXT("remove")));
          return ::DDS::RETCODE_ERROR;
        }
      else
        {
          // Decrease ref count after the servant is removed
          // from subscriber set.
          the_servant->_remove_ref ();

          deactivate_object < ::DDS::Subscriber_ptr > (s);
          return ::DDS::RETCODE_OK;
        }
    }


    ::DDS::Subscriber_ptr
    DomainParticipantImpl::get_builtin_subscriber (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (enabled_ == false)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::get_builtin_subscriber, ")
                      ACE_TEXT(" Entity is not enabled. \n")));
          return ::DDS::Subscriber::_nil ();
        }

        return ::DDS::Subscriber::_duplicate (bit_subscriber_.in ());
    }

    ::DDS::Topic_ptr
    DomainParticipantImpl::create_topic (
        const char * topic_name,
        const char * type_name,
        const ::DDS::TopicQos & qos,
        ::DDS::TopicListener_ptr a_listener
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      ::DDS::TopicQos topic_qos;

      if (qos == TOPIC_QOS_DEFAULT)
        {
          this->get_default_topic_qos(topic_qos);
        }
      else
        {
          topic_qos = qos;
        }

      if (! Qos_Helper::valid (topic_qos))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::create_topic, ")
                      ACE_TEXT("invalid qos.\n")));
          return ::DDS::Topic::_nil ();
        }

      if (! Qos_Helper::consistent (topic_qos))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("DomainParticipantImpl::create_topic, ")
                      ACE_TEXT("inconsistent qos.\n")));
          return ::DDS::Topic::_nil();
        }

      TopicMap::mapped_type* entry = 0;
      bool found = false;
      {
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                          tao_mon,
                          this->topics_protector_,
                          ::DDS::Topic::_nil());

        if (Util::find(topics_, topic_name, entry) == 0)
          {
            found = true;
          }
      }

      if ( found )
      {
      	CORBA::String_var found_type
      	  = entry->pair_.svt_->get_type_name();

        if (ACE_OS::strcmp(type_name, found_type) == 0)
          {
            ::DDS::TopicQos found_qos;
            entry->pair_.svt_->get_qos(found_qos);
            if (topic_qos == found_qos)  // match type name, qos
              {
                {
                  ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                                    tao_mon,
                                    this->topics_protector_,
                                    ::DDS::Topic::_nil());
                  entry->client_refs_ ++;
                }
                return ::DDS::Topic::_duplicate(entry->pair_.obj_.in ());
              }
            else
              {
                if (DCPS_debug_level >= 1)
                  {
                  ACE_DEBUG ((LM_DEBUG,
                              ACE_TEXT("(%P|%t) DomainParticipantImpl::create_topic, ")
                              ACE_TEXT("qos not match: topic_name=%s type_name=%s\n"),
                              topic_name, type_name));
                  }
                return ::DDS::Topic::_nil();
              }
          }
        else  // no match
          {
            if (DCPS_debug_level >= 1)
              {
                ACE_DEBUG ((LM_DEBUG,
                            ACE_TEXT("(%P|%t) DomainParticipantImpl::create_topic, ")
                            ACE_TEXT(" not match: topic_name=%s type_name=%s\n"),
                            topic_name, type_name));
              }
            return ::DDS::Topic::_nil();
          }
        }
      else
        {
          RepoId topic_id;

          try
            {

              TopicStatus status = repository_->assert_topic(topic_id,
                                                             domain_id_,
                                                             dp_id_,
                                                             topic_name,
                                                             type_name,
                                                             topic_qos);
              if (status == CREATED || status == FOUND)
                {
                  ::DDS::Topic_ptr new_topic = create_topic_i(topic_id,
                                                              topic_name,
                                                              type_name,
                                                              topic_qos,
                                                              a_listener);
                  return new_topic;
                }
              else
                {
                  ACE_ERROR ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_topic, ")
                              ACE_TEXT("assert_topic failed.\n")));
                  return ::DDS::Topic::_nil();
                }
            }
          catch (const CORBA::SystemException& sysex)
            {
              sysex._tao_print_exception (
                "ERROR: System Exception"
                " in DomainParticipantImpl::create_topic");
              return ::DDS::Topic::_nil();
            }
          catch (const CORBA::UserException& userex)
            {
              userex._tao_print_exception (
                "ERROR: User Exception"
                "in DomainParticipantImpl::create_topic");
              return ::DDS::Topic::_nil();
            }
        }
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::delete_topic (
        ::DDS::Topic_ptr a_topic
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      return delete_topic_i (a_topic, false);
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::delete_topic_i (
        ::DDS::Topic_ptr a_topic,
        bool             remove_objref)
    {

      ::DDS::ReturnCode_t ret = ::DDS::RETCODE_OK;

      try
      {
        if (enabled_ == false)
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                              ACE_TEXT(" Entity is not enabled. \n")),
                              ::DDS::RETCODE_NOT_ENABLED);
          }

        // The servant's ref count should be greater than 2 at this point,
        // one referenced by poa, one referenced by the topic map and
        // others referenced by the datareader/datawriter.
        TopicImpl* the_topic_servant = reference_to_servant<TopicImpl> (a_topic);

        CORBA::String_var topic_name = the_topic_servant->get_name();

        ::DDS::DomainParticipant_var dp = the_topic_servant->get_participant();

        DomainParticipantImpl* the_dp_servant =
          reference_to_servant<DomainParticipantImpl> (dp.in ());

        if (the_dp_servant != this)
          {
            return ::DDS::RETCODE_PRECONDITION_NOT_MET;
          }

        {

          ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                            tao_mon,
                            this->topics_protector_,
                            ::DDS::RETCODE_ERROR);

          TopicMap::mapped_type* entry = 0;
          if (Util::find(topics_, topic_name.in (), entry) == -1)
            {
              ACE_ERROR_RETURN ((LM_ERROR,
                                ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                                ACE_TEXT("%p\n"),
                                ACE_TEXT("find")),
                                ::DDS::RETCODE_ERROR);
            }

          entry->client_refs_ --;

          if (remove_objref == true ||
              0 == entry->client_refs_)
            {
              //TBD - mark the TopicImpl as deleted and make it
              //      reject calls to the TopicImpl.

              TopicStatus status
                = repository_->remove_topic (the_dp_servant->get_domain_id (),
                                              the_dp_servant->get_id (),
                                              the_topic_servant->get_id ()
 );
              if (status != REMOVED)
                {
                  ACE_ERROR_RETURN ((LM_ERROR,
                                    ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                                    ACE_TEXT("remove_topic failed\n")),
                                    ::DDS::RETCODE_ERROR);
                }

              // Decrease ref count after the servant is removed
              // from the topic map.
              the_topic_servant->_remove_ref ();

              deactivate_object < ::DDS::Topic_ptr > (a_topic);

              // note: this will destroy the TopicImpl if there are no
              // client object reference to it.
              if (topics_.erase(topic_name.in ()) == 0)
                {
                  ACE_ERROR_RETURN ((LM_ERROR,
                                    ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                                    ACE_TEXT("%p \n"),
                                    ACE_TEXT("unbind")),
                                    ::DDS::RETCODE_ERROR);
                }
              else
                return ::DDS::RETCODE_OK;

            }
         }
      }
      catch (const CORBA::SystemException& sysex)
        {
          sysex._tao_print_exception (
            "ERROR: System Exception"
            " in DomainParticipantImpl::delete_topic_i");
          ret = ::DDS::RETCODE_ERROR;
        }
      catch (const CORBA::UserException& userex)
        {
          userex._tao_print_exception (
            "ERROR: User Exception"
            " in DomainParticipantImpl::delete_topic_i");
          ret = ::DDS::RETCODE_ERROR;
        }
      catch (...)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_topic_i, ")
                      ACE_TEXT(" Caught Unknown Exception \n")));
          ret = ::DDS::RETCODE_ERROR;
        }

      return ret;
    }


    //Note: caller should NOT assign to Topic_var (without _duplicate'ing)
    //      because it will steal the framework's reference.
    ::DDS::Topic_ptr
    DomainParticipantImpl::find_topic (
        const char * topic_name,
        const ::DDS::Duration_t & timeout
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      try
        {
          ACE_Time_Value timeout_tv
            = ACE_OS::gettimeofday() + ACE_Time_Value(timeout.sec, timeout.nanosec/1000);

          int first_time = 1;

          while (first_time || ACE_OS::gettimeofday() < timeout_tv)
            {
              if (first_time)
                {
                  first_time = 0;
                }

              TopicMap::mapped_type* entry = 0;
              {
                ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                                  tao_mon,
                                  this->topics_protector_,
                                  ::DDS::Topic::_nil());

                if (Util::find(topics_, topic_name, entry) == 0)
                  {
                    entry->client_refs_ ++;
                    return ::DDS::Topic::_duplicate(entry->pair_.obj_.in ());
                  }
              }

              RepoId topic_id;
              CORBA::String_var type_name;
              ::DDS::TopicQos_var qos;

              TopicStatus status = repository_->find_topic(domain_id_,
                                                           topic_name,
                                                           type_name.out(),
                                                           qos.out(),
                                                           topic_id);

              if (status == FOUND)
              {
                ::DDS::Topic_ptr new_topic = create_topic_i(topic_id,
                                                          topic_name,
                                                          type_name,
                                                          qos,
                                                          ::DDS::TopicListener::_nil ());
                return new_topic;
              }
              else
                {
                  ACE_Time_Value now = ACE_OS::gettimeofday();
                  if (now < timeout_tv)
                    {
                      ACE_Time_Value remainging = timeout_tv - now;
                      if (remainging.sec () >= 1)
                        {
                          ACE_OS::sleep(1);
                        }
                      else
                        {
                          ACE_OS::sleep(remainging);
                        }
                    }
                }
            }
        }
      catch (const CORBA::SystemException& sysex)
        {
          sysex._tao_print_exception (
            "ERROR: System Exception"
            " in DomainParticipantImpl::find_topic");
          return ::DDS::Topic::_nil();
        }
      catch (const CORBA::UserException& userex)
        {
          userex._tao_print_exception (
            "ERROR: User Exception"
            " in DomainParticipantImpl::find_topic");
          return ::DDS::Topic::_nil();
        }

      if (DCPS_debug_level >= 1)
        {
          // timed out
          ACE_DEBUG((LM_DEBUG,
                    ACE_TEXT("(%P|%t) DomainParticipantImpl::find_topic, ")
                    ACE_TEXT("timed out. \n")));
        }
      return ::DDS::Topic::_nil();
    }


    //Note: caller should NOT assign to DataReader_var (without _duplicate'ing)
    //      because it will steal the framework's reference.
    ::DDS::TopicDescription_ptr
    DomainParticipantImpl::lookup_topicdescription (
        const char * name
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (enabled_ == false)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::lookup_topicdescription, ")
                      ACE_TEXT(" Entity is not enabled. \n")));
          return ::DDS::TopicDescription::_nil ();
        }

      ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                        tao_mon,
                        this->topics_protector_,
                        ::DDS::Topic::_nil());

      TopicMap::mapped_type* entry = 0;
      if (Util::find(topics_, name, entry) == -1)
        {
          return ::DDS::TopicDescription::_nil();
        }
      else
        {
          return ::DDS::TopicDescription::_duplicate(entry->pair_.obj_.in ());
        }
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::delete_contained_entities (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::delete_contained_entities, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      // mark that the entity is being deleted
      set_deleted (true);

      // delete publishers
      {
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                          tao_mon,
                          this->publishers_protector_,
                          ::DDS::RETCODE_ERROR);

        PublisherSet::iterator pubIter = publishers_.begin();
        ::DDS::Publisher_ptr pubPtr;
        size_t pubsize = publishers_.size();

        while (pubsize > 0)
          {
            pubPtr = (*pubIter).obj_.in ();
            ++pubIter;

            ::DDS::ReturnCode_t result
              = pubPtr->delete_contained_entities ();
            if (result != ::DDS::RETCODE_OK)
              {
                return result;
              }

            result = delete_publisher (pubPtr);
            if (result != ::DDS::RETCODE_OK)
              {
                return result;
              }
            pubsize--;
          }

      }

      // delete subscribers
      {
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                          tao_mon,
                          this->subscribers_protector_,
                          ::DDS::RETCODE_ERROR);

        SubscriberSet::iterator subIter = subscribers_.begin();
        ::DDS::Subscriber_ptr subPtr;
        size_t subsize = subscribers_.size();

        while (subsize > 0)
          {
            subPtr = (*subIter).obj_.in ();
            ++subIter;

            ::DDS::ReturnCode_t result = subPtr->delete_contained_entities ();
            if (result != ::DDS::RETCODE_OK)
              {
                return result;
              }

            result = delete_subscriber (subPtr);
            if (result != ::DDS::RETCODE_OK)
              {
                return result;
              }

            subsize--;
          }
      }

      ::DDS::ReturnCode_t ret = ::DDS::RETCODE_OK;
      // delete topics
      {
        ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                          tao_mon,
                          this->topics_protector_,
                          ::DDS::RETCODE_ERROR);


        while (1)
          {
            if (topics_.begin() == topics_.end())
              {
                break;
              }

            // Delete the topic the reference count.
            ::DDS::ReturnCode_t result = this->delete_topic_i(
                                    topics_.begin()->second.pair_.obj_.in (), true);
            if (result != ::DDS::RETCODE_OK)
              {
                ret = result;
              }
          }
      }

      // the participant can now start creating new contained entities
      set_deleted (false);

      return ret;
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::set_qos (
        const ::DDS::DomainParticipantQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
        {
          if (enabled_.value())
            {
              if (! Qos_Helper::changeable (qos_, qos))
                {
                  return ::DDS::RETCODE_IMMUTABLE_POLICY;
                }
            }
          qos_ = qos;
          // TBD - when there are changable QoS supported
          //       this code may need to do something
          //       with the changed values.
          // TBD - when there are changable QoS then we
          //       need to tell the DCPSInfo/repository_ about
          //       the changes in Qos.

          // repository_->update_domain_participant_qos(domain_id_,
          //                                     participant_id_,
          //                                     qos);
          return ::DDS::RETCODE_OK;
        }
      else
        {
          return ::DDS::RETCODE_INCONSISTENT_POLICY;
        }
    }


    void
    DomainParticipantImpl::get_qos (
        ::DDS::DomainParticipantQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      qos = qos_;
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::set_listener (
        ::DDS::DomainParticipantListener_ptr a_listener,
        ::DDS::StatusKindMask mask
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      listener_mask_ = mask;
      //note: OK to duplicate  and reference_to_servant a nil object ref
      listener_ = ::DDS::DomainParticipantListener::_duplicate(a_listener);
      fast_listener_
        = reference_to_servant<DDS::DomainParticipantListener> (listener_.in ());
      return ::DDS::RETCODE_OK;
    }


    ::DDS::DomainParticipantListener_ptr
    DomainParticipantImpl::get_listener (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      return ::DDS::DomainParticipantListener::_duplicate(listener_.in());
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::ignore_participant (
        ::DDS::InstanceHandle_t handle
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_participant, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      RepoId ignore_id = 0;

      BIT_Helper_1 < ::DDS::ParticipantBuiltinTopicDataDataReader,
               ::DDS::ParticipantBuiltinTopicDataDataReader_var,
               ::DDS::ParticipantBuiltinTopicDataSeq > hh;
      ::DDS::ReturnCode_t ret
        = hh.instance_handle_to_repo_id(this, BUILT_IN_PARTICIPANT_TOPIC, handle, ignore_id);


      if (ret != ::DDS::RETCODE_OK)
        {
          return ret;
        }

      try
        {
          if (DCPS_debug_level >= 4)
            ACE_DEBUG((LM_DEBUG,
                "%P|%t) DomainParticipantImpl::ignore_participant"
                " %d calling repo\n",
                dp_id_ ));
          repository_->ignore_domain_participant(domain_id_,
                                                 dp_id_,
                                                 ignore_id);
          if (DCPS_debug_level >= 4)
            ACE_DEBUG((LM_DEBUG,
                "%P|%t) DomainParticipantImpl::ignore_participant"
                " %d repo call returned.\n",
                dp_id_ ));
        }
      catch (const CORBA::SystemException& sysex)
        {
          sysex._tao_print_exception (
            "ERROR: System Exception"
            " in DomainParticipantImpl::ignore_participant");
          return ::DDS::RETCODE_ERROR;
        }
      catch (const CORBA::UserException& userex)
        {
          userex._tao_print_exception (
            "ERROR: User Exception"
            " in DomainParticipantImpl::ignore_participant");
          return ::DDS::RETCODE_ERROR;
        }

      return ::DDS::RETCODE_OK;
#else
      ACE_UNUSED_ARG (handle);
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::ignore_topic (
        ::DDS::InstanceHandle_t handle
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_topic, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      RepoId ignore_id = 0;

      BIT_Helper_1 < ::DDS::TopicBuiltinTopicDataDataReader,
               ::DDS::TopicBuiltinTopicDataDataReader_var,
               ::DDS::TopicBuiltinTopicDataSeq > hh;
      ::DDS::ReturnCode_t ret =
                  hh.instance_handle_to_repo_id(this, BUILT_IN_TOPIC_TOPIC, handle, ignore_id);

      if (ret != ::DDS::RETCODE_OK)
        {
          return ret;
        }

      try
        {
          repository_->ignore_topic(domain_id_,
                                    dp_id_,
                                    ignore_id);
        }
      catch (const CORBA::SystemException& sysex)
        {
          sysex._tao_print_exception (
            "System Exception"
            " in DomainParticipantImpl::ignore_topic");
          return ::DDS::RETCODE_OK;
        }
      catch (const CORBA::UserException& userex)
        {
          userex._tao_print_exception (
            "ERROR: User Exception"
            " in DomainParticipantImpl::ignore_topic");
          return ::DDS::RETCODE_OK;
        }

      return ::DDS::RETCODE_OK;
#else
      ACE_UNUSED_ARG (handle);
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::ignore_publication (
        ::DDS::InstanceHandle_t handle
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_publication, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      RepoId ignore_id = 0;

      BIT_Helper_1 < ::DDS::PublicationBuiltinTopicDataDataReader,
               ::DDS::PublicationBuiltinTopicDataDataReader_var,
               ::DDS::PublicationBuiltinTopicDataSeq > hh;
      ::DDS::ReturnCode_t ret =
                  hh.instance_handle_to_repo_id(this, BUILT_IN_PUBLICATION_TOPIC, handle, ignore_id);

      if (ret != ::DDS::RETCODE_OK)
        {
          return ret;
        }

      try
        {
          repository_->ignore_publication(domain_id_,
                                          dp_id_,
                                          ignore_id);
        }
      catch (const CORBA::SystemException& sysex)
        {
          sysex._tao_print_exception (
            "ERROR: System Exception"
            " in DomainParticipantImpl::ignore_publication");
          return ::DDS::RETCODE_ERROR;
        }
      catch (const CORBA::UserException& userex)
        {
          userex._tao_print_exception (
            "ERROR: User Exception"
            " in DomainParticipantImpl::ignore_publication");
          return ::DDS::RETCODE_ERROR;
        }

      return ::DDS::RETCODE_OK;
#else
      ACE_UNUSED_ARG (handle);
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::ignore_subscription (
        ::DDS::InstanceHandle_t handle
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      if (enabled_ == false)
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::ignore_subscription, ")
                            ACE_TEXT(" Entity is not enabled. \n")),
                            ::DDS::RETCODE_NOT_ENABLED);
        }

      RepoId ignore_id = 0;

      BIT_Helper_1 < ::DDS::SubscriptionBuiltinTopicDataDataReader,
               ::DDS::SubscriptionBuiltinTopicDataDataReader_var,
               ::DDS::SubscriptionBuiltinTopicDataSeq > hh;
      ::DDS::ReturnCode_t ret =
                  hh.instance_handle_to_repo_id(this, BUILT_IN_SUBSCRIPTION_TOPIC, handle, ignore_id);

      if (ret != ::DDS::RETCODE_OK)
        {
          return ret;
        }

      try
        {
          repository_->ignore_subscription(domain_id_,
                                           dp_id_,
                                           ignore_id);
        }
      catch (const CORBA::SystemException& sysex)
        {
          sysex._tao_print_exception (
            "ERROR: System Exception"
            " in DomainParticipantImpl::ignore_subscription");
          return ::DDS::RETCODE_ERROR;
        }
      catch (const CORBA::UserException& userex)
        {
          userex._tao_print_exception (
            "ERROR: User Exception"
            " in DomainParticipantImpl::ignore_subscription");
          return ::DDS::RETCODE_ERROR;
        }

      return ::DDS::RETCODE_OK;
#else
      ACE_UNUSED_ARG (handle);
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::DomainId_t
    DomainParticipantImpl::get_domain_id (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      return domain_id_;
    }


    void
    DomainParticipantImpl::assert_liveliness (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      // This operation needs to only be used if the DomainParticipant contains
      // DataWriter entities with the LIVELINESS set to MANUAL_BY_PARTICIPANT and
      // it only affects the liveliness of those DataWriter entities. Otherwise,
      // it has no effect.
      // This will do nothing in current implementation since we only
      // support the AUTOMATIC liveliness qos for datawriter.
      // Add implementation here.
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::set_default_publisher_qos (
        const ::DDS::PublisherQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
        {
          default_publisher_qos_ = qos;
          return ::DDS::RETCODE_OK;
        }
      else
        {
          return ::DDS::RETCODE_INCONSISTENT_POLICY;
        }
    }


    void
    DomainParticipantImpl::get_default_publisher_qos (
        ::DDS::PublisherQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      qos = default_publisher_qos_;
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::set_default_subscriber_qos (
        const ::DDS::SubscriberQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
        {
          default_subscriber_qos_ = qos;
          return ::DDS::RETCODE_OK;
        }
      else
        {
          return ::DDS::RETCODE_INCONSISTENT_POLICY;
        }
    }


    void
    DomainParticipantImpl::get_default_subscriber_qos (
        ::DDS::SubscriberQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      qos = default_subscriber_qos_;
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::set_default_topic_qos (
        const ::DDS::TopicQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      if (Qos_Helper::valid(qos) && Qos_Helper::consistent(qos))
        {
          default_topic_qos_ = qos;
          return ::DDS::RETCODE_OK;
        }
      else
        {
          return ::DDS::RETCODE_INCONSISTENT_POLICY;
        }
    }


    void
    DomainParticipantImpl::get_default_topic_qos (
        ::DDS::TopicQos & qos
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      qos = default_topic_qos_;
    }

    ::DDS::ReturnCode_t
    DomainParticipantImpl::enable (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      //TDB - check if factory is enables and then enable all entities
      // (don't need to do it for now because
      //  entity_factory.autoenable_created_entities is always = 1)

      ::DDS::ReturnCode_t ret = this->set_enabled ();

      if (ret == ::DDS::RETCODE_OK && ! TheTransientKludge->is_enabled ())
        {
#if !defined (DDS_HAS_MINIMUM_BIT)
          if (TheServiceParticipant->get_BIT ())
            {
              return init_bit ();
            }
          else
            {
              return ::DDS::RETCODE_OK;
            }
#else
          return ::DDS::RETCODE_OK;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
        }
      else
        {
          return ret;
        }
    }


    ::DDS::StatusKindMask
    DomainParticipantImpl::get_status_changes (
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      return EntityImpl::get_status_changes ();
    }


    RepoId
    DomainParticipantImpl::get_id ()
    {
      return dp_id_;
    }


    ::DDS::Topic_ptr
    DomainParticipantImpl::create_topic_i (
        const RepoId topic_id,
        const char * topic_name,
        const char * type_name,
        const ::DDS::TopicQos & qos,
        ::DDS::TopicListener_ptr a_listener
      )
      ACE_THROW_SPEC ((
        CORBA::SystemException
      ))
    {
      ACE_GUARD_RETURN (ACE_Recursive_Thread_Mutex,
                        tao_mon,
                        this->topics_protector_,
                        ::DDS::Topic::_nil());

      TopicMap::mapped_type* entry = 0;
      if (Util::find(topics_, topic_name, entry) == 0)
        {
          entry->client_refs_ ++;
          return ::DDS::Topic::_duplicate(entry->pair_.obj_.in ());
        }

      OpenDDS::DCPS::TypeSupport_ptr type_support =
        OpenDDS::DCPS::Registered_Data_Types->lookup(this->participant_objref_.in (),type_name);

      if (0 == type_support)
        {
          return ::DDS::Topic::_nil();
        }

      TopicImpl* topic_servant;

      ACE_NEW_RETURN (topic_servant,
                      TopicImpl(topic_id,
                                topic_name,
                                type_name,
                                type_support,
                                qos,
                                a_listener,
                                participant_objref_.in ()),
                      ::DDS::Topic::_nil());

      if ((enabled_ == true) && (qos_.entity_factory.autoenable_created_entities == 1))
        {
          topic_servant->enable();
        }

      ::DDS::Topic_ptr obj  = servant_to_reference (topic_servant);

      RefCounted_Topic refCounted_topic (Topic_Pair (topic_servant, obj, NO_DUP));

      if (bind(topics_, topic_name, refCounted_topic) == -1)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::create_topic, ")
                      ACE_TEXT("%p \n"),
                      ACE_TEXT("bind")));
          return ::DDS::Topic::_nil();
        }

      // Increase ref count when the servant is added to
      // topic map.
      topic_servant->_add_ref ();

      // the topics_ map has one reference and we duplicate to give
      // the caller another reference.
      return ::DDS::Topic::_duplicate(refCounted_topic.pair_.obj_.in ());
    }


    int
    DomainParticipantImpl::is_clean () const
    {
      int sub_is_clean = subscribers_.empty();
      int topics_is_clean = topics_.size () == 0;

      if (! TheTransientKludge->is_enabled ())
        {
          // There are four topics and builtin topic subscribers
          // left.

          sub_is_clean = sub_is_clean == 0 ? subscribers_.size () == 1 : 1;
          topics_is_clean = topics_is_clean == 0 ? topics_.size () == 4 : 1;
        }

      return (publishers_.empty()
              && sub_is_clean == 1
              && topics_is_clean == 1);
    }


    void
    DomainParticipantImpl::set_object_reference (const ::DDS::DomainParticipant_ptr& dp)
    {
      if (! CORBA::is_nil (participant_objref_.in ()))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::set_object_reference, ")
                      ACE_TEXT("This participant is already activated. \n")));
          return;
        }

        participant_objref_ = ::DDS::DomainParticipant::_duplicate (dp);
    }

    ::DDS::DomainParticipantListener*
    DomainParticipantImpl::listener_for (::DDS::StatusKind kind)
    {
      if ((listener_mask_ & kind) == 0)
        {
          return 0;
        }
      else
        {
          return fast_listener_;
        }
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::init_bit ()
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      ::DDS::ReturnCode_t ret;

      if (((ret = init_bit_subscriber ()) == ::DDS::RETCODE_OK)
           && ((ret = attach_bit_transport ()) == ::DDS::RETCODE_OK)
           && ((ret = init_bit_topics ()) == ::DDS::RETCODE_OK)
           && ((ret = init_bit_datareaders ()) == ::DDS::RETCODE_OK))
        {
          return ::DDS::RETCODE_OK;
        }
      else
        {
          return ret;
        }
#else
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::init_bit_topics ()
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      try
      {
        ::DDS::TopicQos topic_qos;
        this->get_default_topic_qos(topic_qos);

        OpenDDS::DCPS::TypeSupport_ptr type_support =
          OpenDDS::DCPS::Registered_Data_Types->lookup(this->participant_objref_.in (), BUILT_IN_PARTICIPANT_TOPIC_TYPE);

        if (0 == type_support)
          {
            // Participant topic
            ::DDS::ParticipantBuiltinTopicDataTypeSupportImpl* participantTypeSupport_servant
              = new ::DDS::ParticipantBuiltinTopicDataTypeSupportImpl();
            ::DDS::ReturnCode_t ret
              = participantTypeSupport_servant->register_type(participant_objref_.in (),
                                                      BUILT_IN_PARTICIPANT_TOPIC_TYPE);
            if (ret != ::DDS::RETCODE_OK)
              {
                ACE_ERROR_RETURN ((LM_ERROR,
                                  ACE_TEXT("(%P|%t) ")
                                  ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                                  ACE_TEXT("register BUILT_IN_PARTICIPANT_TOPIC_TYPE returned %d.\n"),
                                  ret),
                                  ret);
              }
          }

        bit_part_topic_ = this->create_topic (::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC,
                                               ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC_TYPE,
                                               topic_qos,
                                               ::DDS::TopicListener::_nil());
        if (CORBA::is_nil (bit_part_topic_.in ()))
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                               ACE_TEXT("(%P|%t) ")
                               ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                               ACE_TEXT("Nil %s Topic \n"),
                               ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC),
                               ::DDS::RETCODE_ERROR);
          }

        // Topic topic
        type_support =
          OpenDDS::DCPS::Registered_Data_Types->lookup(this->participant_objref_.in (), BUILT_IN_TOPIC_TOPIC_TYPE);

        if (0 == type_support)
          {
            ::DDS::TopicBuiltinTopicDataTypeSupportImpl* topicTypeSupport_servant =
              new ::DDS::TopicBuiltinTopicDataTypeSupportImpl();

            ::DDS::ReturnCode_t ret
              = topicTypeSupport_servant->register_type(participant_objref_.in (),
                                                BUILT_IN_TOPIC_TOPIC_TYPE);
            if (ret != ::DDS::RETCODE_OK)
              {

                ACE_ERROR_RETURN ((LM_ERROR,
                                  ACE_TEXT("(%P|%t) ")
                                  ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                                  ACE_TEXT("register BUILT_IN_TOPIC_TOPIC_TYPE returned %d.\n"),
                                  ret),
                                  ret);
              }
          }

        bit_topic_topic_ = this->create_topic (::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC,
                                               ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC_TYPE,
                                               topic_qos,
                                               ::DDS::TopicListener::_nil());
        if (CORBA::is_nil (bit_topic_topic_.in ()))
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                               ACE_TEXT("(%P|%t) ")
                               ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                               ACE_TEXT("Nil %s Topic \n"),
                               ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC),
                               ::DDS::RETCODE_ERROR);
          }

        // Subscription topic
        type_support =
          OpenDDS::DCPS::Registered_Data_Types->lookup(this->participant_objref_.in (), BUILT_IN_SUBSCRIPTION_TOPIC_TYPE);

        if (0 == type_support)
          {
            ::DDS::SubscriptionBuiltinTopicDataTypeSupportImpl* subscriptionTypeSupport_servant
              = new ::DDS::SubscriptionBuiltinTopicDataTypeSupportImpl();

            ::DDS::ReturnCode_t ret
              = subscriptionTypeSupport_servant->register_type(participant_objref_.in (),
                                                       BUILT_IN_SUBSCRIPTION_TOPIC_TYPE);
            if (ret != ::DDS::RETCODE_OK)
              {
                ACE_ERROR_RETURN ((LM_ERROR,
                                  ACE_TEXT("(%P|%t) ")
                                  ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                                  ACE_TEXT("register BUILT_IN_SUBSCRIPTION_TOPIC_TYPE returned %d.\n"),
                                  ret),
                                  ret);
              }
          }

        bit_sub_topic_ =
          this->create_topic (::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC,
                              ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC_TYPE,
                              topic_qos,
                              ::DDS::TopicListener::_nil());
        if (CORBA::is_nil (bit_sub_topic_.in ()))
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                               ACE_TEXT("(%P|%t) ")
                               ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                               ACE_TEXT("Nil %s Topic \n"),
                               ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC),
                               ::DDS::RETCODE_ERROR);
          }

        // Publication topic
        type_support =
          OpenDDS::DCPS::Registered_Data_Types->lookup(this->participant_objref_.in (), BUILT_IN_PUBLICATION_TOPIC_TYPE);

        if (0 == type_support)
          {
            ::DDS::PublicationBuiltinTopicDataTypeSupportImpl* publicationTypeSupport_servant
              = new ::DDS::PublicationBuiltinTopicDataTypeSupportImpl();

            ::DDS::ReturnCode_t ret
              = publicationTypeSupport_servant->register_type(participant_objref_.in (),
                                                      BUILT_IN_PUBLICATION_TOPIC_TYPE);
            if (ret != ::DDS::RETCODE_OK)
              {
                ACE_ERROR_RETURN ((LM_ERROR,
                                  ACE_TEXT("(%P|%t) ")
                                  ACE_TEXT("DomainParticipantImpl::init_bit_topics, ")
                                  ACE_TEXT("register BUILT_IN_PUBLICATION_TOPIC_TYPE returned %d.\n"),
                                  ret),
                                  ret);
              }
          }

        bit_pub_topic_ =
          this->create_topic (::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC,
                              ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC_TYPE,
                              topic_qos,
                              ::DDS::TopicListener::_nil());
        if (CORBA::is_nil (bit_pub_topic_.in ()))
          {
            ACE_ERROR_RETURN ((LM_ERROR,
                               ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::init_bit_topics, ")
                               ACE_TEXT("Nil %s Topic \n"),
                               ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC),
                               ::DDS::RETCODE_ERROR);
          }
      }
      catch (const CORBA::Exception& ex)
        {
          ex._tao_print_exception (
            "ERROR: Exception caught in DomainParticipant::init_bit_topics.");
          return ::DDS::RETCODE_ERROR;
        }

      return ::DDS::RETCODE_OK;
#else
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::init_bit_subscriber ()
    {
      try
        {
          ::DDS::SubscriberQos sub_qos;
          this->get_default_subscriber_qos(sub_qos);

          bit_subscriber_
            = this->create_subscriber (sub_qos,
                                       ::DDS::SubscriberListener::_nil ());
        }
      catch (const CORBA::Exception& ex)
        {
          ex._tao_print_exception (
            "ERROR: Exception caught in DomainParticipant::create_bit_subscriber.");
          return ::DDS::RETCODE_ERROR;
        }

      return ::DDS::RETCODE_OK;
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::init_bit_datareaders ()
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      try
        {
          ::DDS::DataReaderQos dr_qos;
          bit_subscriber_->get_default_datareader_qos(dr_qos);

          ::DDS::TopicDescription_var bit_part_topic_desc
            = this->lookup_topicdescription (::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC);

          ::DDS::DataReader_var dr
            = bit_subscriber_->create_datareader (bit_part_topic_desc.in (),
                                                  dr_qos,
                                                  ::DDS::DataReaderListener::_nil ());

          bit_part_dr_
            = ::DDS::ParticipantBuiltinTopicDataDataReader::_narrow (dr.in ());

          ::DDS::TopicDescription_var bit_topic_topic_desc
            = this->lookup_topicdescription (::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC);

          dr = bit_subscriber_->create_datareader (bit_topic_topic_desc.in (),
                                                   dr_qos,
                                                   ::DDS::DataReaderListener::_nil ());

          bit_topic_dr_
            = ::DDS::TopicBuiltinTopicDataDataReader::_narrow (dr.in ());

          ::DDS::TopicDescription_var bit_pub_topic_desc
            = this->lookup_topicdescription (::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC);

          dr = bit_subscriber_->create_datareader (bit_pub_topic_desc.in (),
                                                   dr_qos,
                                                   ::DDS::DataReaderListener::_nil ());

          bit_pub_dr_
            = ::DDS::PublicationBuiltinTopicDataDataReader::_narrow (dr.in ());

          ::DDS::TopicDescription_var bit_sub_topic_desc
            = this->lookup_topicdescription (::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC);

          dr = bit_subscriber_->create_datareader (bit_sub_topic_desc.in (),
                                                   dr_qos,
                                                   ::DDS::DataReaderListener::_nil ());

          bit_sub_dr_
            = ::DDS::SubscriptionBuiltinTopicDataDataReader::_narrow (dr.in ());
        }
      catch (const CORBA::Exception& ex)
        {
          ex._tao_print_exception (
            "ERROR: Exception caught in DomainParticipant::init_bit_datareaders.");
          return ::DDS::RETCODE_ERROR;
        }

      return ::DDS::RETCODE_OK;
#else

      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }


    ::DDS::ReturnCode_t
    DomainParticipantImpl::attach_bit_transport ()
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
       try
      {
        // Attach the Subscriber with the TransportImpl.
        ::OpenDDS::DCPS::SubscriberImpl* sub_servant
          = reference_to_servant<OpenDDS::DCPS::SubscriberImpl> (bit_subscriber_.in ());

        TransportImpl_rch impl = TheServiceParticipant->bit_transport_impl ();

        OpenDDS::DCPS::AttachStatus status
          = sub_servant->attach_transport(impl.in());

        if (status != OpenDDS::DCPS::ATTACH_OK)
          {
            // We failed to attach to the transport for some reason.
            const char* status_str = "" ;

            switch (status)
              {
              case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
                status_str = "ATTACH_BAD_TRANSPORT";
                break;
              case OpenDDS::DCPS::ATTACH_ERROR:
                status_str = "ATTACH_ERROR";
                break;
              case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
                status_str = "ATTACH_INCOMPATIBLE_QOS";
                break;
              default:
                status_str = "Unknown Status";
                break;
              }

            ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) ERROR: DomainParticipantImpl::init_bit_transport, "),
                              ACE_TEXT("Failed to attach to the transport. ")
                              ACE_TEXT("AttachStatus == %s\n"), status_str),
                              ::DDS::RETCODE_ERROR);
          }
      }
      catch (const CORBA::Exception& ex)
      {
        ex._tao_print_exception (
          "ERROR: Exception caught in DomainParticipantImpl::init_bit_transport.");
        return ::DDS::RETCODE_ERROR;
      }

      return ::DDS::RETCODE_OK;
#else
      return ::DDS::RETCODE_UNSUPPORTED;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
    }

   } // namespace DCPS
} // namespace OpenDDS
