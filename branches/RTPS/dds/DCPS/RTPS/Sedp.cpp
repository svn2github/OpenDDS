/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "Sedp.h"
#include "Spdp.h"

#include "RtpsMessageTypesC.h"
#include "RtpsBaseMessageTypesTypeSupportImpl.h"

#include "dds/DCPS/transport/framework/ReceivedDataSample.h"

#include "dds/DCPS/Serializer.h"
#include "dds/DCPS/RepoIdBuilder.h"
#include "dds/DCPS/GuidConverter.h"
#include "dds/DCPS/AssociationData.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/DataSampleList.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/BuiltInTopicUtils.h"

#include "dds/DdsDcpsInfrastructureTypeSupportImpl.h"

#include <cstdio>
#include <ctime>
#include <iostream>
#include <sstream>


namespace OpenDDS {
namespace RTPS {
using DCPS::RepoId;

DDS::TopicBuiltinTopicDataDataReaderImpl*
Sedp::topic_bit()
{
  DDS::Subscriber_var sub = spdp_.bit_subscriber();
  DDS::DataReader_var d =
    sub->lookup_datareader(DCPS::BUILT_IN_TOPIC_TOPIC);
  return dynamic_cast<DDS::TopicBuiltinTopicDataDataReaderImpl*>(d.in());
}

DDS::PublicationBuiltinTopicDataDataReaderImpl*
Sedp::pub_bit()
{
  DDS::Subscriber_var sub = spdp_.bit_subscriber();
  DDS::DataReader_var d =
    sub->lookup_datareader(DCPS::BUILT_IN_PUBLICATION_TOPIC);
  return dynamic_cast<DDS::PublicationBuiltinTopicDataDataReaderImpl*>(d.in());
}

DDS::SubscriptionBuiltinTopicDataDataReaderImpl*
Sedp::sub_bit()
{
  DDS::Subscriber_var sub = spdp_.bit_subscriber();
  DDS::DataReader_var d =
    sub->lookup_datareader(DCPS::BUILT_IN_SUBSCRIPTION_TOPIC);
  return dynamic_cast<DDS::SubscriptionBuiltinTopicDataDataReaderImpl*>(d.in());
}

DCPS::TopicStatus
Sedp::assert_topic(DCPS::RepoId_out topicId, const char* topicName,
                   const char* dataTypeName, const DDS::TopicQos& qos)
{
  if (topics_.count(topicName)) { // types must match, RtpsInfo checked for us
    topics_[topicName].qos_ = qos;
    topicId = topics_[topicName].repo_id_;
    return DCPS::FOUND;
  }

  TopicDetails& td = topics_[topicName];
  td.data_type_ = dataTypeName;
  td.qos_ = qos;
  td.repo_id_ = participant_id_;
  td.repo_id_.entityId.entityKind = DCPS::ENTITYKIND_OPENDDS_TOPIC;
  assign(td.repo_id_.entityId.entityKey, topic_counter_++);
  topic_names_[td.repo_id_] = topicName;
  topicId = td.repo_id_;

  if (topic_counter_ == 0x1000000) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: Spdp::assert_topic: ")
               ACE_TEXT("Exceeded Maximum number of topic entity keys!")
               ACE_TEXT("Next key will be a duplicate!\n")));
    topic_counter_ = 0;
  }

  return DCPS::CREATED;
}

DCPS::TopicStatus
Sedp::remove_topic(const RepoId& topicId, std::string& name)
{
  name = topic_names_[topicId];
  topics_.erase(name);
  topic_names_.erase(topicId);
  return DCPS::REMOVED;
}

bool
Sedp::update_topic_qos(const RepoId& topicId, const DDS::TopicQos& qos,
                       std::string& name)
{
  if (topic_names_.count(topicId)) {
    name = topic_names_[topicId];
    topics_[name].qos_ = qos;
    return true;
  }
  return false;
}

RepoId
Sedp::add_publication(const RepoId& topicId,
                      DCPS::DataWriterRemote_ptr publication,
                      const DDS::DataWriterQos& qos,
                      const DCPS::TransportLocatorSeq& transInfo,
                      const DDS::PublisherQos& publisherQos)
{
  RepoId rid = participant_id_;
  rid.entityId.entityKind = DCPS::ENTITYKIND_USER_WRITER_WITH_KEY; //TODO: WITH_KEY ???
  assign(rid.entityId.entityKey, publication_counter_++);
  PublicationDetails& pb = publications_[rid];
  pb.topic_id_ = topicId;
  pb.publication_ = publication;
  pb.qos_ = qos;
  pb.trans_info_ = transInfo;
  pb.publisher_qos_ = publisherQos;

  // TODO: use SEDP to advertise the new publication

  return rid;
}

void
Sedp::remove_publication(const RepoId& publicationId)
{
  // TODO: use SEDP to dispose the publication
  publications_.erase(publicationId);
}

bool
Sedp::update_publication_qos(const RepoId& publicationId,
                             const DDS::DataWriterQos& qos,
                             const DDS::PublisherQos& publisherQos)
{
  if (publications_.count(publicationId)) {
    PublicationDetails& pb = publications_[publicationId];
    pb.qos_ = qos;
    pb.publisher_qos_ = publisherQos;
    // TODO: tell the world about the change with SEDP

    return true;
  }
  return false;
}

RepoId
Sedp::add_subscription(const RepoId& topicId,
                       DCPS::DataReaderRemote_ptr subscription,
                       const DDS::DataReaderQos& qos,
                       const DCPS::TransportLocatorSeq& transInfo,
                       const DDS::SubscriberQos& subscriberQos,
                       const char* filterExpr,
                       const DDS::StringSeq& params)
{
  RepoId rid = participant_id_;
  rid.entityId.entityKind = DCPS::ENTITYKIND_USER_READER_WITH_KEY; //TODO: WITH_KEY ???
  assign(rid.entityId.entityKey, subscription_counter_++);
  SubscriptionDetails& sb = subscriptions_[rid];
  sb.topic_id_ = topicId;
  sb.subscription_ = subscription;
  sb.qos_ = qos;
  sb.trans_info_ = transInfo;
  sb.subscriber_qos_ = subscriberQos;
  sb.params_ = params;

  // TODO: use SEDP to advertise the new subscription

  return rid;

}

void
Sedp::remove_subscription(const RepoId& subscriptionId)
{
  // TODO: use SEDP to dispose the subscription
  subscriptions_.erase(subscriptionId);
}

bool
Sedp::update_subscription_qos(const RepoId& subscriptionId,
                              const DDS::DataReaderQos& qos,
                              const DDS::SubscriberQos& subscriberQos)
{
  if (subscriptions_.count(subscriptionId)) {
    SubscriptionDetails& sb = subscriptions_[subscriptionId];
    sb.qos_ = qos;
    sb.subscriber_qos_ = subscriberQos;
    // TODO: tell the world about the change with SEDP

    return true;
  }
  return false;
}

bool
Sedp::update_subscription_params(const RepoId& subId,
                                 const DDS::StringSeq& params)
{
  if (subscriptions_.count(subId)) {
    SubscriptionDetails& sb = subscriptions_[subId];
    sb.params_ = params;
    // TODO: tell the world about the change with SEDP

    return true;
  }
  return false;
}

void
Sedp::association_complete(const RepoId& localId, const RepoId& remoteId)
{
}

void
Sedp::disassociate_participant(const RepoId& remoteId)
{
}

void
Sedp::disassociate_publication(const RepoId& localId, const RepoId& remoteId)
{
}

void
Sedp::disassociate_subscription(const RepoId& localId, const RepoId& remoteId)
{
}

Sedp::Endpoint::~Endpoint()
{
}

//---------------------------------------------------------------

Sedp::Writer::~Writer()
{
}

bool
Sedp::Writer::assoc(const DCPS::AssociationData& subscription)
{
  return associate(subscription, true);
}

void
Sedp::Writer::data_delivered(const DCPS::DataSampleListElement*)
{
}

void
Sedp::Writer::data_dropped(const DCPS::DataSampleListElement*, bool)
{
}

void
Sedp::Writer::control_delivered(ACE_Message_Block*)
{
}

void
Sedp::Writer::control_dropped(ACE_Message_Block*, bool)
{
}

//-------------------------------------------------------------------------

Sedp::Reader::~Reader()
{}

bool
Sedp::Reader::assoc(const DCPS::AssociationData& publication)
{
  return associate(publication, false);
}


// Implementing TransportReceiveListener

void
Sedp::Reader::data_received(const DCPS::ReceivedDataSample& sample)
{
  switch (sample.header_.message_id_) {
  case DCPS::SAMPLE_DATA: {
    DCPS::Serializer ser(sample.sample_,
                         sample.header_.byte_order_ != ACE_CDR_BYTE_ORDER,
                         DCPS::Serializer::ALIGN_CDR);
    bool ok = true;
    ACE_CDR::ULong encap;
    ok &= (ser >> encap); // read and ignore 32-bit CDR Encapsulation header
    ParameterList data;
    ok &= (ser >> data);
    if (!ok) {
      ACE_DEBUG((LM_ERROR, "ERROR: failed to deserialize data\n"));
      return;
    }

//TODO:    spdp_.add_discovered_endpoint(data);

    break;
  }
  case DCPS::DISPOSE_INSTANCE:
  case DCPS::UNREGISTER_INSTANCE:
  case DCPS::DISPOSE_UNREGISTER_INSTANCE: {
    DCPS::Serializer ser(sample.sample_,
                         sample.header_.byte_order_ != ACE_CDR_BYTE_ORDER,
                         DCPS::Serializer::ALIGN_CDR);
    bool ok = true;
    ACE_CDR::ULong encap;
    ok &= (ser >> encap); // read and ignore 32-bit CDR Encapsulation header
    DiscoveredWriterData data;
    ok &= (ser >> data);

    if (!ok) {
      ACE_DEBUG((LM_ERROR, "ERROR: failed to deserialize key data\n"));
      return;
    }

    //TODO: process dispose/unregister
    break;
  }
  default:
    break;
  }
}


}
}
