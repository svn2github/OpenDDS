/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef TAO_DDS_DCPS_DOMAIN_PARTICIPANT_IMPL_H
#define TAO_DDS_DCPS_DOMAIN_PARTICIPANT_IMPL_H

#include "EntityImpl.h"
#include "Definitions.h"
#include "InstanceHandle.h"
#include "TopicImpl.h"
#include "dds/DdsDcpsPublicationC.h"
#include "dds/DdsDcpsSubscriptionExtC.h"
#include "dds/DdsDcpsTopicC.h"
#include "dds/DdsDcpsDomainExtS.h"
#include "dds/DdsDcpsInfoC.h"
#include "dds/DCPS/GuidUtils.h"

#if !defined (DDS_HAS_MINIMUM_BIT)
#include "dds/DdsDcpsInfrastructureTypeSupportC.h"
#endif // !defined (DDS_HAS_MINIMUM_BIT)

#include "dds/DCPS/transport/framework/TransportImpl_rch.h"
#include "ace/Null_Mutex.h"
#include "ace/Recursive_Thread_Mutex.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

namespace OpenDDS {
namespace DCPS {

class FailoverListener;
class PublisherImpl;
class SubscriberImpl;
class DomainParticipantFactoryImpl;
class Monitor;

/**
* @class DomainParticipantImpl
*
* @brief Implements the OpenDDS::DCPS::DomainParticipant interfaces.
*
* This class acts as an entrypoint of the service and a factory
* for publisher, subscriber and topic. It also acts as a container
* for the publisher, subscriber and topic objects.
*
* See the DDS specification, OMG formal/04-12-02, for a description of
* the interface this class is implementing.
*/
class OpenDDS_Dcps_Export DomainParticipantImpl
  : public virtual OpenDDS::DCPS::LocalObject<DomainParticipantExt>,
    public virtual OpenDDS::DCPS::EntityImpl {
public:
  typedef Objref_Servant_Pair <SubscriberImpl, DDS::Subscriber,
          DDS::Subscriber_ptr, DDS::Subscriber_var> Subscriber_Pair;

  typedef Objref_Servant_Pair <PublisherImpl, DDS::Publisher,
          DDS::Publisher_ptr, DDS::Publisher_var> Publisher_Pair;

  typedef Objref_Servant_Pair <TopicImpl, DDS::Topic,
          DDS::Topic_ptr, DDS::Topic_var> Topic_Pair;

  typedef std::set<Subscriber_Pair> SubscriberSet;
  typedef std::set<Publisher_Pair> PublisherSet;

  struct RefCounted_Topic {
    RefCounted_Topic()
      : client_refs_(0)
    {}

    RefCounted_Topic(const Topic_Pair & pair)
      : pair_(pair),
        client_refs_(1)
    {}

    /// The topic object reference.
    Topic_Pair     pair_;
    /// The reference count on the obj_.
    CORBA::Long    client_refs_;
  };

  typedef std::map<std::string, RefCounted_Topic> TopicMap;
  typedef std::map<RepoId, DDS::InstanceHandle_t, GUID_tKeyLessThan> HandleMap;

  ///Constructor
  DomainParticipantImpl(DomainParticipantFactoryImpl *       factory,
                        const DDS::DomainId_t&               domain_id,
                        const RepoId&                        dp_id,
                        const DDS::DomainParticipantQos &    qos,
                        DDS::DomainParticipantListener_ptr   a_listener,
                        const DDS::StatusMask &              mask,
                        bool                                 federated = false);

  ///Destructor
  virtual ~DomainParticipantImpl();

  virtual DDS::InstanceHandle_t get_instance_handle()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::Publisher_ptr create_publisher(
    const DDS::PublisherQos & qos,
    DDS::PublisherListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_publisher(
    DDS::Publisher_ptr p)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::Subscriber_ptr create_subscriber(
    const DDS::SubscriberQos & qos,
    DDS::SubscriberListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_subscriber(
    DDS::Subscriber_ptr s)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::Subscriber_ptr get_builtin_subscriber()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::Topic_ptr create_topic(
    const char * topic_name,
    const char * type_name,
    const DDS::TopicQos & qos,
    DDS::TopicListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_topic(
    DDS::Topic_ptr a_topic)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::Topic_ptr find_topic(
    const char * topic_name,
    const DDS::Duration_t & timeout)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::TopicDescription_ptr lookup_topicdescription(
    const char * name)
  ACE_THROW_SPEC((CORBA::SystemException));


#ifndef OPENDDS_NO_CONTENT_SUBSCRIPTION_PROFILE

  virtual DDS::ContentFilteredTopic_ptr create_contentfilteredtopic(
    const char * name,
    DDS::Topic_ptr related_topic,
    const char * filter_expression,
    const DDS::StringSeq & expression_parameters)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_contentfilteredtopic(
    DDS::ContentFilteredTopic_ptr a_contentfilteredtopic)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::MultiTopic_ptr create_multitopic(
    const char * name,
    const char * type_name, 
    const char * subscription_expression,
    const DDS::StringSeq & expression_parameters)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t delete_multitopic(DDS::MultiTopic_ptr a_multitopic)
  ACE_THROW_SPEC((CORBA::SystemException));

#endif

  virtual DDS::ReturnCode_t delete_contained_entities()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual CORBA::Boolean contains_entity(DDS::InstanceHandle_t a_handle)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_qos(
    const DDS::DomainParticipantQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_qos(
    DDS::DomainParticipantQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_listener(
    DDS::DomainParticipantListener_ptr a_listener,
    DDS::StatusMask mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::DomainParticipantListener_ptr get_listener()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t ignore_participant(
    DDS::InstanceHandle_t handle)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t ignore_topic(
    DDS::InstanceHandle_t handle)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t ignore_publication(
    DDS::InstanceHandle_t handle)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t ignore_subscription(
    DDS::InstanceHandle_t handle)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::DomainId_t get_domain_id()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t assert_liveliness()
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_default_publisher_qos(
    const DDS::PublisherQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_default_publisher_qos(
    DDS::PublisherQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_default_subscriber_qos(
    const DDS::SubscriberQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_default_subscriber_qos(
    DDS::SubscriberQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t set_default_topic_qos(
    const DDS::TopicQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_default_topic_qos(
    DDS::TopicQos & qos)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_current_time(
    DDS::Time_t & current_time)
  ACE_THROW_SPEC((CORBA::SystemException));

#if !defined (DDS_HAS_MINIMUM_BIT)

  virtual DDS::ReturnCode_t get_discovered_participants(
    DDS::InstanceHandleSeq & participant_handles)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_discovered_participant_data(
    DDS::ParticipantBuiltinTopicData & participant_data,
    DDS::InstanceHandle_t participant_handle)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_discovered_topics(
    DDS::InstanceHandleSeq & topic_handles)
  ACE_THROW_SPEC((CORBA::SystemException));

  virtual DDS::ReturnCode_t get_discovered_topic_data(
    DDS::TopicBuiltinTopicData & topic_data,
    DDS::InstanceHandle_t topic_handle)
  ACE_THROW_SPEC((CORBA::SystemException));

#endif

  virtual DDS::ReturnCode_t enable()
  ACE_THROW_SPEC((CORBA::SystemException));

  /// Following methods are not the idl interfaces and are
  /// local operations.

  /**
  *  Return the id given by the DCPSInfo repositoy.
  */
  RepoId get_id();

  CORBA::Long get_federation_id()
  ACE_THROW_SPEC((CORBA::SystemException));

  CORBA::Long get_participant_id()
  ACE_THROW_SPEC((CORBA::SystemException));

  /**
   * Obtain a local handle representing a GUID.
   */
  DDS::InstanceHandle_t get_handle(const RepoId& id = GUID_UNKNOWN);

  /**
  *  Associate the servant with the object reference.
  *  This is required to pass to the topic servant.
  */
  void set_object_reference(const DDS::DomainParticipant_ptr& dp);

  /**
  *  Check if the topic is used by any datareader or datawriter.
  */
  int is_clean() const;

  /**
  * This is used to retrieve the listener for a certain status change.
  * If this DomainParticipant has a registered listener and the status
  * kind is in the listener mask then the listener is returned.
  * Otherwise, return nil.
  */
  DDS::DomainParticipantListener* listener_for(DDS::StatusKind kind);

  typedef std::vector<RepoId> TopicIdVec;
  /**
  * Populates an std::vector with the RepoId of the topics this
  * participant has created/found.
  */
  void get_topic_ids(TopicIdVec& topics);

private:

  /** The implementation of create_topic.
  */
  DDS::Topic_ptr create_topic_i(
    const RepoId topic_id,
    const char * topic_name,
    const char * type_name,
    const DDS::TopicQos & qos,
    DDS::TopicListener_ptr a_listener,
    const DDS::StatusMask & mask)
  ACE_THROW_SPEC((CORBA::SystemException));

  /** Delete the topic with option of whether the
   *  topic object reference should be removed.
   */
  DDS::ReturnCode_t delete_topic_i(
    DDS::Topic_ptr a_topic,
    bool             remove_objref);

  /// Initialize the built in topic.
  DDS::ReturnCode_t init_bit();
  /// Initialize the built in topic topics
  DDS::ReturnCode_t init_bit_topics();
  /// Create the built in topic subscriber.
  DDS::ReturnCode_t init_bit_subscriber();
  /// Initialize the built in topic datareaders.
  DDS::ReturnCode_t init_bit_datareaders();
  /// Attach the subscriber with the transport.
  DDS::ReturnCode_t attach_bit_transport();

  DomainParticipantFactoryImpl* factory_;
  /// The default topic qos.
  DDS::TopicQos        default_topic_qos_;
  /// The default publisher qos.
  DDS::PublisherQos    default_publisher_qos_;
  /// The default subscriber qos.
  DDS::SubscriberQos   default_subscriber_qos_;

  /// The qos of this DomainParticipant.
  DDS::DomainParticipantQos            qos_;
  /// Used to notify the entity for relevant events.
  DDS::DomainParticipantListener_var   listener_;
  /// The DomainParticipant listener servant.
  DDS::DomainParticipantListener*      fast_listener_;
  /// The StatusKind bit mask indicates which status condition change
  /// can be notified by the listener of this entity.
  DDS::StatusMask                      listener_mask_;
  /// The id of the domain that creates this participant.
  DDS::DomainId_t                      domain_id_;
  /// This participant id given by DCPSInfo repository.
  RepoId                               dp_id_;

  /// Whether this DomainParticipant is attached to a federated
  /// repository.
  bool                                 federated_;

  /// Collection of publishers.
  PublisherSet   publishers_;
  /// Collection of subscribers.
  SubscriberSet  subscribers_;
  /// Collection of topics.
  TopicMap       topics_;
  /// Collection of handles.
  HandleMap      handles_;
  /// Collection of ignored participants.
  HandleMap      ignored_participants_;
  /// Collection of ignored topics.
  HandleMap      ignored_topics_;
  /// Protect the publisher collection.
  ACE_Recursive_Thread_Mutex   publishers_protector_;
  /// Protect the subscriber collection.
  ACE_Recursive_Thread_Mutex   subscribers_protector_;
  /// Protect the topic collection.
  ACE_Recursive_Thread_Mutex   topics_protector_;
  /// Protect the handle collection.
  ACE_Recursive_Thread_Mutex   handle_protector_;

  /// The object reference activated from this servant.
  DDS::DomainParticipant_var participant_objref_;

  /// The built in topic subscriber.
  DDS::Subscriber_var        bit_subscriber_;

  /// The topic for built in topic participant.
  DDS::Topic_var       bit_part_topic_;
  /// The topic for built in topic topic.
  DDS::Topic_var       bit_topic_topic_;
  /// The topic for built in topic publication.
  DDS::Topic_var       bit_pub_topic_;
  /// The topic for built in topic subscription.
  DDS::Topic_var       bit_sub_topic_;

  /// Listener to initiate failover with.
  FailoverListener*    failoverListener_;

  /// Instance handle generators for non-repo backed entities
  /// (i.e. subscribers and publishers).
  InstanceHandleGenerator participant_handles_;

#if !defined (DDS_HAS_MINIMUM_BIT)
  /// The datareader for built in topic participant.
  DDS::ParticipantBuiltinTopicDataDataReader_var  bit_part_dr_;
  /// The datareader for built in topic topic.
  DDS::TopicBuiltinTopicDataDataReader_var        bit_topic_dr_;
  /// The datareader for built in topic publication.
  DDS::PublicationBuiltinTopicDataDataReader_var  bit_pub_dr_;
  /// The datareader for built in topic subscription.
  DDS::SubscriptionBuiltinTopicDataDataReader_var bit_sub_dr_;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
  Monitor* monitor_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif /* TAO_DDS_DCPS_DOMAIN_PARTICIPANT_IMPL_H  */
