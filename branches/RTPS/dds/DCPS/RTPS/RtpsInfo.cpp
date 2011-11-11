/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "RtpsInfo.h"
#include "BaseMessageTypes.h"

namespace OpenDDS {
namespace RTPS {
using DCPS::RepoId;


// Participant operations:

bool
RtpsInfo::attach_participant(DDS::DomainId_t /*domainId*/,
                             const RepoId& /*participantId*/)
{
  return false; // This is just for DCPSInfoRepo?
}

DCPS::AddDomainStatus
RtpsInfo::add_domain_participant(DDS::DomainId_t domain,
                                 const DDS::DomainParticipantQos& qos)
{
  DCPS::AddDomainStatus ads = {RepoId(), false /*federated*/};
  guid_gen_.populate(ads.id);
  ads.id.entityId = RTPS::ENTITYID_PARTICIPANT;
  participants_[domain][ads.id] = new Spdp(domain, ads.id, qos);
  return ads;
}

void
RtpsInfo::remove_domain_participant(DDS::DomainId_t domain,
                                    const RepoId& participantId)
{
  participants_[domain].erase(participantId);
  if (participants_[domain].empty()) {
    participants_.erase(domain);
  }
}

void
RtpsInfo::ignore_domain_participant(DDS::DomainId_t domain,
                                    const RepoId& myParticipantId,
                                    const RepoId& ignoreId)
{
  participants_[domain][myParticipantId]->ignore_domain_participant(ignoreId);
}

bool
RtpsInfo::update_domain_participant_qos(DDS::DomainId_t domain,
                                        const RepoId& participant,
                                        const DDS::DomainParticipantQos& qos)
{
  return participants_[domain][participant]->update_domain_participant_qos(qos);
}


// Topic operations:

DCPS::TopicStatus
RtpsInfo::assert_topic(DCPS::RepoId_out topicId,
                       DDS::DomainId_t domainId, const RepoId& participantId,
                       const char* topicName, const char* dataTypeName,
                       const DDS::TopicQos& qos)
{
  if (topics_.count(domainId)) {
    const std::map<std::string, Spdp::TopicDetails>::iterator it =
      topics_[domainId].find(topicName);
    if (it != topics_[domainId].end()
        && it->second.data_type_ != dataTypeName) {
      topicId = GUID_UNKNOWN;
      return DCPS::CONFLICTING_TYPENAME;
    }
  }

  const DCPS::TopicStatus stat =
    participants_[domainId][participantId]->assert_topic(topicId, topicName,
                                                         dataTypeName, qos);
  if (stat == DCPS::CREATED || stat == DCPS::FOUND) { // qos change (FOUND)
    Spdp::TopicDetails& td = topics_[domainId][topicName];
    td.data_type_ = dataTypeName;
    td.qos_ = qos;
    td.repo_id_ = topicId;
  }
  return stat;
}

DCPS::TopicStatus
RtpsInfo::find_topic(DDS::DomainId_t domainId, const char* topicName,
                     CORBA::String_out dataTypeName, DDS::TopicQos_out qos,
                     DCPS::RepoId_out topicId)
{
  if (!topics_.count(domainId)) {
    return DCPS::NOT_FOUND;
  }
  if (!topics_[domainId].count(topicName)) {
    return DCPS::NOT_FOUND;
  }
  Spdp::TopicDetails& td = topics_[domainId][topicName];
  dataTypeName = td.data_type_.c_str();
  qos = new DDS::TopicQos(td.qos_);
  topicId = td.repo_id_;
  return DCPS::FOUND;
}

DCPS::TopicStatus
RtpsInfo::remove_topic(DDS::DomainId_t domainId, const RepoId& participantId,
                       const RepoId& topicId)
{
  if (!topics_.count(domainId)) {
    return DCPS::NOT_FOUND;
  }

  std::string name;
  const DCPS::TopicStatus stat =
    participants_[domainId][participantId]->remove_topic(topicId, name);

  if (stat == DCPS::REMOVED) {
    topics_[domainId].erase(name);
    if (topics_[domainId].empty()) {
      topics_.erase(domainId);
    }
  }
  return stat;
}

void
RtpsInfo::ignore_topic(DDS::DomainId_t domainId, const RepoId& myParticipantId,
                       const RepoId& ignoreId)
{
  participants_[domainId][myParticipantId]->ignore_topic(ignoreId);
}

bool
RtpsInfo::update_topic_qos(const RepoId& topicId, DDS::DomainId_t domainId,
                           const RepoId& participantId, const DDS::TopicQos& qos)
{
  std::string name;
  if (participants_[domainId][participantId]->update_topic_qos(topicId,
                                                               qos, name)) {
    topics_[domainId][name].qos_ = qos;
    return true;
  }
  return false;
}


// Publication operations:

RepoId
RtpsInfo::add_publication(DDS::DomainId_t domainId, const RepoId& participantId,
                          const RepoId& topicId,
                          DCPS::DataWriterRemote_ptr publication,
                          const DDS::DataWriterQos& qos,
                          const DCPS::TransportLocatorSeq& transInfo,
                          const DDS::PublisherQos& publisherQos)
{
  return RepoId();
}

void
RtpsInfo::remove_publication(DDS::DomainId_t domainId,
                             const RepoId& participantId,
                             const RepoId& publicationId)
{
}

void
RtpsInfo::ignore_publication(DDS::DomainId_t domainId,
                             const RepoId& myParticipantId,
                             const RepoId& ignoreId)
{
}

bool
RtpsInfo::update_publication_qos(DDS::DomainId_t domainId, const RepoId& partId,
                                 const RepoId& dwId,
                                 const DDS::DataWriterQos& qos,
                                 const DDS::PublisherQos& publisherQos)
{
  return false;
}


// Subscription operations:

RepoId
RtpsInfo::add_subscription(DDS::DomainId_t domainId,
                           const RepoId& participantId, const RepoId& topicId,
                           DCPS::DataReaderRemote_ptr subscription,
                           const DDS::DataReaderQos& qos,
                           const DCPS::TransportLocatorSeq& transInfo,
                           const DDS::SubscriberQos& subscriberQos,
                           const char* filterExpression,
                           const DDS::StringSeq& exprParams)
{
  return RepoId();
}

void
RtpsInfo::remove_subscription(DDS::DomainId_t domainId,
                              const RepoId& participantId,
                              const RepoId& subscriptionId)
{
}

void
RtpsInfo::ignore_subscription(DDS::DomainId_t domainId,
                              const RepoId& myParticipantId,
                              const RepoId& ignoreId)
{
}

bool
RtpsInfo::update_subscription_qos(DDS::DomainId_t domainId,
                                  const RepoId& partId, const RepoId& drId,
                                  const DDS::DataReaderQos& qos,
                                  const DDS::SubscriberQos& subscriberQos)
{
  return false;
}

bool
RtpsInfo::update_subscription_params(DDS::DomainId_t domainId,
                                     const RepoId& participantId,
                                     const RepoId& subscriptionId,
                                     const DDS::StringSeq& params)

{
  return false;
}

// Managing reader/writer associations:

void
RtpsInfo::association_complete(DDS::DomainId_t domainId,
                               const RepoId& participantId,
                               const RepoId& localId, const RepoId& remoteId)
{
}

void
RtpsInfo::disassociate_participant(DDS::DomainId_t domainId,
                                   const RepoId& local_id,
                                   const RepoId& remote_id)
{
}

void
RtpsInfo::disassociate_subscription(DDS::DomainId_t domainId,
                                    const RepoId& participantId,
                                    const RepoId& local_id,
                                    const RepoId& remote_id)
{
}

void
RtpsInfo::disassociate_publication(DDS::DomainId_t domainId,
                                   const RepoId& participantId,
                                   const RepoId& local_id,
                                   const RepoId& remote_id)
{
}


}
}
