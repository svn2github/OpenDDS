// -*- C++ -*-
// ============================================================================
/**
 *  @file   DCPSInfo_i.h
 *
 *  $Id$
 *
 *
 */
// ============================================================================

#ifndef DCPSINFO_I_H
#define DCPSINFO_I_H



#include /**/ "DCPS_IR_Topic.h"
#include /**/ "DCPS_IR_Topic_Description.h"
#include /**/ "DCPS_IR_Participant.h"
#include /**/ "DCPS_IR_Publication.h"
#include /**/ "DCPS_IR_Subscription.h"
#include /**/ "DCPS_IR_Domain.h"

#include /**/ "dds/DdsDcpsInfoS.h"
#include /**/ "dds/DdsDcpsDataReaderRemoteC.h"
#include /**/ "dds/DdsDcpsDataWriterRemoteC.h"


#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

// typedef declarations
typedef ACE_Map_Manager< ::DDS::DomainId_t, DCPS_IR_Domain*, ACE_Null_Mutex> DCPS_IR_Domain_Map;

/**
 * @class TAO_DDS_DCPSInfo_i
 *
 * @brief Implementation of the DCPSInfo
 *
 * This is the Information Repository object.  Clients of 
 * the system will use the CORBA reference of this object.
 */
class  TAO_DDS_DCPSInfo_i : public virtual POA_TAO::DCPS::DCPSInfo
{
public:
  //Constructor 
  TAO_DDS_DCPSInfo_i (void);
  
  //Destructor 
  virtual ~TAO_DDS_DCPSInfo_i (void);
  
  virtual TAO::DCPS::TopicStatus assert_topic (
      TAO::DCPS::RepoId_out topicId,
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      const char * topicName,
      const char * dataTypeName,
      const ::DDS::TopicQos & qos
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
    ));

  virtual TAO::DCPS::TopicStatus find_topic (
      ::DDS::DomainId_t domainId,
      const char * topicName,
      CORBA::String_out dataTypeName,
      ::DDS::TopicQos_out qos,
      TAO::DCPS::RepoId_out topicId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
    ));

  virtual TAO::DCPS::TopicStatus remove_topic (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      TAO::DCPS::RepoId topicId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Topic
    ));

  virtual TAO::DCPS::TopicStatus enable_topic (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      TAO::DCPS::RepoId topicId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Topic
    ));

  virtual TAO::DCPS::RepoId add_publication (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      TAO::DCPS::RepoId topicId,
      TAO::DCPS::DataWriterRemote_ptr publication,
      const ::DDS::DataWriterQos & qos,
      const TAO::DCPS::TransportInterfaceInfo & transInfo,
      const ::DDS::PublisherQos & publisherQos
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Topic
    ));

    virtual void remove_publication (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      TAO::DCPS::RepoId publicationId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Publication
    ));

  virtual TAO::DCPS::RepoId add_subscription (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      TAO::DCPS::RepoId topicId,
      TAO::DCPS::DataReaderRemote_ptr subscription,
      const ::DDS::DataReaderQos & qos,
      const TAO::DCPS::TransportInterfaceInfo & transInfo,
      const ::DDS::SubscriberQos & subscriberQos
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Topic
    ));

    virtual void remove_subscription (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId,
      TAO::DCPS::RepoId subscriptionId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Subscription
    ));

  virtual TAO::DCPS::RepoId add_domain_participant (
      ::DDS::DomainId_t domain,
      const ::DDS::DomainParticipantQos & qos
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
    ));

    virtual void remove_domain_participant (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId participantId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
    ));

  virtual void ignore_domain_participant (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId myParticipantId,
      TAO::DCPS::RepoId otherParticipantId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
    ));

  virtual void ignore_topic (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId myParticipantId,
      TAO::DCPS::RepoId topicId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Topic
    ));

  virtual void ignore_subscription (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId myParticipantId,
      TAO::DCPS::RepoId subscriptionId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Subscription
    ));

  virtual void ignore_publication (
      ::DDS::DomainId_t domainId,
      TAO::DCPS::RepoId myParticipantId,
      TAO::DCPS::RepoId publicationId
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
      , TAO::DCPS::Invalid_Domain
      , TAO::DCPS::Invalid_Participant
      , TAO::DCPS::Invalid_Publication
    ));
  
  /// Called to load the domains
  /// returns the number of domains that were loaded.
  int load_domains (const char* filename, bool use_bit);

  /// Initialize the transport for the Built-In Topics
  /// Returns 0 (zero) if succeeds
  int init_transport (int listen_address_given,
                      const ACE_INET_Addr listen);

private:

  DCPS_IR_Domain_Map domains_;
};


#endif /* DCPSINFO_I_H_  */
