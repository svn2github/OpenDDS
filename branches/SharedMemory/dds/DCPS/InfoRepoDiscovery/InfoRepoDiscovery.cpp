/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */
#include "InfoRepoDiscovery.h"

#include "dds/DCPS/InfoRepoDiscovery/DataReaderRemoteC.h"
#include "dds/DCPS/InfoRepoDiscovery/DataReaderRemoteImpl.h"
#include "dds/DCPS/InfoRepoDiscovery/DataWriterRemoteC.h"
#include "dds/DCPS/InfoRepoDiscovery/DataWriterRemoteImpl.h"
#include "dds/DCPS/InfoRepoDiscovery/FailoverListener.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/RepoIdBuilder.h"
#include "dds/DCPS/ConfigUtils.h"

#if !defined (DDS_HAS_MINIMUM_BIT)
#include "dds/DCPS/DomainParticipantImpl.h"
#include "dds/DCPS/BuiltInTopicUtils.h"
#include "dds/DCPS/Marked_Default_Qos.h"

#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/transport/framework/TransportExceptions.h"

#include "dds/DCPS/transport/tcp/TcpInst.h"
#include "dds/DCPS/transport/tcp/TcpInst_rch.h"
#endif

namespace OpenDDS {
namespace DCPS {

InfoRepoDiscovery::InfoRepoDiscovery(const RepoKey& key,
                                     const std::string& ior)
  : Discovery(key),
    ior_(ior),
    bit_transport_port_(0),
    use_local_bit_config_(false),
    failoverListener_(0)
{
}

InfoRepoDiscovery::InfoRepoDiscovery(const RepoKey& key,
                                     const DCPSInfo_var& info)
  : Discovery(key),
    info_(info),
    bit_transport_port_(0),
    use_local_bit_config_(false),
    failoverListener_(0)
{
}

InfoRepoDiscovery::~InfoRepoDiscovery()
{
  delete this->failoverListener_;
}

namespace
{
  DCPSInfo_ptr get_repo(const char* ior, CORBA::ORB_ptr orb)
  {
    CORBA::Object_var o;
    try {
      o = orb->string_to_object(ior);
    } catch (CORBA::INV_OBJREF&) {
      // host:port format causes an exception; try again
      // with corbaloc format
      std::string second_try("corbaloc:iiop:");
      second_try += ior;
      second_try += "/DCPSInfoRepo";

      o = orb->string_to_object(second_try.c_str());
    }

    return DCPSInfo::_narrow(o.in());
  }
}

DCPSInfo_var
InfoRepoDiscovery::get_dcps_info()
{
  if (CORBA::is_nil(this->info_.in())) {
    CORBA::ORB_var orb = TheServiceParticipant->get_ORB();
    try {
      this->info_ = get_repo(this->ior_.c_str(), orb.in());

      if (CORBA::is_nil(this->info_.in())) {
        ACE_ERROR((LM_ERROR,
                   ACE_TEXT("(%P|%t) ERROR: InfoRepoDiscovery::get_dcps_info: ")
                   ACE_TEXT("unable to narrow DCPSInfo (%C) for key %C.\n"),
                   this->ior_.c_str(),
                   this->key().c_str()));
        return DCPSInfo::_nil();
      }

    } catch (const CORBA::Exception& ex) {
      ex._tao_print_exception("ERROR: InfoRepoDiscovery::get_dcps_info: failed to resolve ior - ");
      return DCPSInfo::_nil();
    }
  }

  return this->info_;
}

std::string
InfoRepoDiscovery::get_stringified_dcps_info_ior()
{
  return this->ior_;
}

TransportConfig_rch
InfoRepoDiscovery::bit_config()
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (bit_config_.is_nil()) {
    const std::string cfg_name = TransportRegistry::DEFAULT_INST_PREFIX +
                                 "_BITTransportConfig_" + key();
    bit_config_ = TransportRegistry::instance()->create_config(cfg_name);

    const std::string inst_name = TransportRegistry::DEFAULT_INST_PREFIX +
                                  "_BITTCPTransportInst_" + key();
    TransportInst_rch inst =
      TransportRegistry::instance()->create_inst(inst_name, "tcp");
    bit_config_->instances_.push_back(inst);

    if (!use_local_bit_config_) {
      bit_transport_ip_ = TheServiceParticipant->bit_transport_ip();
      bit_transport_port_ = TheServiceParticipant->bit_transport_port();
    }

    // Use a static cast to avoid dependency on the Tcp library
    TcpInst_rch tcp_inst = static_rchandle_cast<TcpInst>(inst);

    tcp_inst->datalink_release_delay_ = 0;
    if (bit_transport_ip_ == "") {
      tcp_inst->local_address_.set_port_number(bit_transport_port_);
    } else {
      tcp_inst->local_address_ = ACE_INET_Addr(bit_transport_port_,
                                               bit_transport_ip_.c_str());
    }

    std::stringstream out;
    out << bit_transport_ip_ << ':' << bit_transport_port_;
    tcp_inst->local_address_str_ = out.str();
  }
  return bit_config_;
#else
  return 0;
#endif
}

DDS::Subscriber_ptr
InfoRepoDiscovery::init_bit(DomainParticipantImpl* participant)
{
#if defined (DDS_HAS_MINIMUM_BIT)
  ACE_UNUSED_ARG(participant);
  return 0;
#else
  if (!TheServiceParticipant->get_BIT()) {
    return 0;
  }

  if (create_bit_topics(participant) != DDS::RETCODE_OK) {
    return 0;
  }

  DDS::Subscriber_var bit_subscriber =
    participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                                   DDS::SubscriberListener::_nil(),
                                   DEFAULT_STATUS_MASK);
  try {
    TransportConfig_rch config = bit_config();
    TransportRegistry::instance()->bind_config(config, bit_subscriber);

  } catch (const Transport::Exception&) {
    ACE_ERROR((LM_ERROR, "(%P|%t) InfoRepoDiscovery::init_bit, "
                         "exception during transport initialization\n"));
    return 0;
  }

  // DataReaders
  try {
    DDS::DataReaderQos participantReaderQos;
    bit_subscriber->get_default_datareader_qos(participantReaderQos);
    participantReaderQos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;

    if (participant->federated()) {
      participantReaderQos.liveliness.lease_duration.nanosec = 0;
      participantReaderQos.liveliness.lease_duration.sec =
        TheServiceParticipant->federation_liveliness();
    }

    DDS::TopicDescription_var bit_part_topic =
      participant->lookup_topicdescription(BUILT_IN_PARTICIPANT_TOPIC);

    DDS::DataReader_var dr =
      bit_subscriber->create_datareader(bit_part_topic,
                                        participantReaderQos,
                                        DDS::DataReaderListener::_nil(),
                                        DEFAULT_STATUS_MASK);

    if (participant->federated()) {
      DDS::ParticipantBuiltinTopicDataDataReader* pbit_dr =
        DDS::ParticipantBuiltinTopicDataDataReader::_narrow(dr.in());

      // Create and attach the listener.
      failoverListener_ = new FailoverListener(key());
      pbit_dr->set_listener(failoverListener_, DEFAULT_STATUS_MASK);
    }

    DDS::DataReaderQos dr_qos;
    bit_subscriber->get_default_datareader_qos(dr_qos);
    dr_qos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;

    DDS::TopicDescription_var bit_topic_topic =
      participant->lookup_topicdescription(BUILT_IN_TOPIC_TOPIC);

    dr = bit_subscriber->create_datareader(bit_topic_topic,
                                           dr_qos,
                                           DDS::DataReaderListener::_nil(),
                                           DEFAULT_STATUS_MASK);

    DDS::TopicDescription_var bit_pub_topic =
      participant->lookup_topicdescription(BUILT_IN_PUBLICATION_TOPIC);

    dr = bit_subscriber->create_datareader(bit_pub_topic,
                                           dr_qos,
                                           DDS::DataReaderListener::_nil(),
                                           OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    DDS::TopicDescription_var bit_sub_topic =
      participant->lookup_topicdescription(BUILT_IN_SUBSCRIPTION_TOPIC);

    dr = bit_subscriber->create_datareader(bit_sub_topic,
                                           dr_qos,
                                           DDS::DataReaderListener::_nil(),
                                           OpenDDS::DCPS::DEFAULT_STATUS_MASK);

  } catch (const CORBA::Exception&) {
    ACE_ERROR((LM_ERROR, "(%P|%t) InfoRepoDiscovery::init_bit, "
                         "exception during DataReader initialization\n"));
    return 0;
  }
  return bit_subscriber._retn();
#endif
}

RepoId
InfoRepoDiscovery::bit_key_to_repo_id(DomainParticipantImpl* /*participant*/,
                                      const char* /*bit_topic_name*/,
                                      const DDS::BuiltinTopicKey_t& key) const
{
  RepoId id = RepoIdBuilder::create();
  RepoIdBuilder builder(id);
  builder.federationId(key.value[0]);
  builder.participantId(key.value[1]);
  builder.entityId(key.value[2]);
  return id;
}

bool
InfoRepoDiscovery::active()
{
  try {
    // invoke a CORBA call, if we are active then there will be no exception
    get_dcps_info()->_is_a("Not_An_IDL_Type");
    return true;
  } catch (const CORBA::Exception&) {
    return false;
  }
}

// Participant operations:

bool
InfoRepoDiscovery::attach_participant(DDS::DomainId_t domainId,
                                      const RepoId& participantId)
{
  try {
    return get_dcps_info()->attach_participant(domainId, participantId);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::attach_participant: ");
    return false;
  }
}

DCPS::AddDomainStatus
InfoRepoDiscovery::add_domain_participant(DDS::DomainId_t domainId,
                                          const DDS::DomainParticipantQos& qos)
{
  try {
    return get_dcps_info()->add_domain_participant(domainId, qos);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::add_domain_participant: ");
    const DCPS::AddDomainStatus ads = {OpenDDS::DCPS::GUID_UNKNOWN, false /*federated*/};
    return ads;
  }
}

bool
InfoRepoDiscovery::remove_domain_participant(DDS::DomainId_t domainId,
                                             const RepoId& participantId)
{
  try {
    get_dcps_info()->remove_domain_participant(domainId, participantId);
    return true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::remove_domain_participant: ");
    return false;
  }
}

bool
InfoRepoDiscovery::ignore_domain_participant(DDS::DomainId_t domainId,
                                             const RepoId& myParticipantId,
                                             const RepoId& ignoreId)
{
  try {
    get_dcps_info()->ignore_domain_participant(domainId, myParticipantId, ignoreId);
    return true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::ignore_domain_participant: ");
    return false;
  }
}

bool
InfoRepoDiscovery::update_domain_participant_qos(DDS::DomainId_t domainId,
                                                 const RepoId& participant,
                                                 const DDS::DomainParticipantQos& qos)
{
  try {
    return get_dcps_info()->update_domain_participant_qos(domainId, participant, qos);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::update_domain_participant_qos: ");
    return false;
  }
}

// Topic operations:

DCPS::TopicStatus
InfoRepoDiscovery::assert_topic(DCPS::RepoId_out topicId, DDS::DomainId_t domainId,
                                const RepoId& participantId, const char* topicName,
                                const char* dataTypeName, const DDS::TopicQos& qos,
                                bool hasDcpsKey)
{
  try {
    return get_dcps_info()->assert_topic(topicId, domainId, participantId, topicName,
      dataTypeName, qos, hasDcpsKey);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::assert_topic: ");
    return DCPS::INTERNAL_ERROR;
  }
}

DCPS::TopicStatus
InfoRepoDiscovery::find_topic(DDS::DomainId_t domainId, const char* topicName,
                              CORBA::String_out dataTypeName, DDS::TopicQos_out qos,
                              DCPS::RepoId_out topicId)
{
  try {
    return get_dcps_info()->find_topic(domainId, topicName, dataTypeName, qos, topicId);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::find_topic: ");
    return DCPS::INTERNAL_ERROR;
  }
}

DCPS::TopicStatus
InfoRepoDiscovery::remove_topic(DDS::DomainId_t domainId, const RepoId& participantId,
                                const RepoId& topicId)
{
  try {
    return get_dcps_info()->remove_topic(domainId, participantId, topicId);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::remove_topic: ");
    return DCPS::INTERNAL_ERROR;
  }
}

bool
InfoRepoDiscovery::ignore_topic(DDS::DomainId_t domainId, const RepoId& myParticipantId,
                                const RepoId& ignoreId)
{
  try {
    get_dcps_info()->ignore_topic(domainId, myParticipantId, ignoreId);
    return true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::ignore_topic: ");
    return false;
  }
}

bool
InfoRepoDiscovery::update_topic_qos(const RepoId& topicId, DDS::DomainId_t domainId,
                                    const RepoId& participantId, const DDS::TopicQos& qos)
{
  try {
    return get_dcps_info()->update_topic_qos(topicId, domainId, participantId, qos);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::update_topic_qos: ");
    return false;
  }
}


// Publication operations:

RepoId
InfoRepoDiscovery::add_publication(DDS::DomainId_t domainId,
                                   const RepoId& participantId,
                                   const RepoId& topicId,
                                   DCPS::DataWriterCallbacks* publication,
                                   const DDS::DataWriterQos& qos,
                                   const DCPS::TransportLocatorSeq& transInfo,
                                   const DDS::PublisherQos& publisherQos)
{
  RepoId pubId;
  try {
    DCPS::DataWriterRemoteImpl* writer_remote_impl = 0;
    ACE_NEW_RETURN(writer_remote_impl,
                   DataWriterRemoteImpl(publication),
                   DCPS::GUID_UNKNOWN);

    //this is taking ownership of the DataWriterRemoteImpl (server side) allocated above
    PortableServer::ServantBase_var writer_remote(writer_remote_impl);

    //this is the client reference to the DataWriterRemoteImpl
    OpenDDS::DCPS::DataWriterRemote_var dr_remote_obj =
      servant_to_remote_reference(writer_remote_impl);

    pubId = get_dcps_info()->add_publication(domainId, participantId, topicId,
      dr_remote_obj, qos, transInfo, publisherQos);

    ACE_GUARD_RETURN(ACE_Thread_Mutex, g, this->lock_, DCPS::GUID_UNKNOWN);
    // take ownership of the client allocated above
    dataWriterMap_[pubId] = dr_remote_obj;

  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::add_publication: ");
    pubId = DCPS::GUID_UNKNOWN;
  }

  return pubId;
}

bool
InfoRepoDiscovery::remove_publication(DDS::DomainId_t domainId,
                                      const RepoId& participantId,
                                      const RepoId& publicationId)
{
  try {
    get_dcps_info()->remove_publication(domainId, participantId, publicationId);
    return true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::remove_publication: ");
    return false;
  }
}

bool
InfoRepoDiscovery::ignore_publication(DDS::DomainId_t domainId,
                                      const RepoId& participantId,
                                      const RepoId& ignoreId)
{
  try {
    get_dcps_info()->ignore_publication(domainId, participantId, ignoreId);
    return true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::ignore_publication: ");
    return false;
  }
}

bool
InfoRepoDiscovery::update_publication_qos(DDS::DomainId_t domainId,
                                          const RepoId& participantId,
                                          const RepoId& dwId,
                                          const DDS::DataWriterQos& qos,
                                          const DDS::PublisherQos& publisherQos)
{
  try {
    return get_dcps_info()->update_publication_qos(domainId, participantId, dwId,
      qos, publisherQos);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::update_publication_qos: ");
    return false;
  }
}


// Subscription operations:

RepoId
InfoRepoDiscovery::add_subscription(DDS::DomainId_t domainId,
                                    const RepoId& participantId,
                                    const RepoId& topicId,
                                    DCPS::DataReaderCallbacks* subscription,
                                    const DDS::DataReaderQos& qos,
                                    const DCPS::TransportLocatorSeq& transInfo,
                                    const DDS::SubscriberQos& subscriberQos,
                                    const char* filterExpr,
                                    const DDS::StringSeq& params)
{
  RepoId subId;
  try {
    DCPS::DataReaderRemoteImpl* reader_remote_impl = 0;
    ACE_NEW_RETURN(reader_remote_impl,
                   DataReaderRemoteImpl(subscription),
                   DCPS::GUID_UNKNOWN);

    //this is taking ownership of the DataReaderRemoteImpl (server side) allocated above
    PortableServer::ServantBase_var reader_remote(reader_remote_impl);

    //this is the client reference to the DataReaderRemoteImpl
    OpenDDS::DCPS::DataReaderRemote_var dr_remote_obj =
      servant_to_remote_reference(reader_remote_impl);

    subId = get_dcps_info()->add_subscription(domainId, participantId, topicId,
      dr_remote_obj, qos, transInfo, subscriberQos, filterExpr, params);

    ACE_GUARD_RETURN(ACE_Thread_Mutex, g, this->lock_, DCPS::GUID_UNKNOWN);
    // take ownership of the client allocated above
    dataReaderMap_[subId] = dr_remote_obj;

  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::add_subscription: ");
    subId = DCPS::GUID_UNKNOWN;
  }
  return subId;
}

bool
InfoRepoDiscovery::remove_subscription(DDS::DomainId_t domainId,
                                       const RepoId& participantId,
                                       const RepoId& subscriptionId)
{
  bool removed = false;
  try {
    get_dcps_info()->remove_subscription(domainId, participantId, subscriptionId);
    removed = true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::remove_subscription: ");
  }

  ACE_GUARD_RETURN(ACE_Thread_Mutex, g, this->lock_, false);
  removeDataReaderRemote(subscriptionId);

  return removed;
}

bool
InfoRepoDiscovery::ignore_subscription(DDS::DomainId_t domainId,
                                       const RepoId& participantId,
                                       const RepoId& ignoreId)
{
  try {
    get_dcps_info()->ignore_subscription(domainId, participantId, ignoreId);
    return true;
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::ignore_subscription: ");
    return false;
  }
}

bool
InfoRepoDiscovery::update_subscription_qos(DDS::DomainId_t domainId,
                                           const RepoId& participantId,
                                           const RepoId& drId,
                                           const DDS::DataReaderQos& qos,
                                           const DDS::SubscriberQos& subQos)
{
  try {
    return get_dcps_info()->update_subscription_qos(domainId, participantId,
      drId, qos, subQos);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::update_subscription_qos: ");
    return false;
  }
}

bool
InfoRepoDiscovery::update_subscription_params(DDS::DomainId_t domainId,
                                              const RepoId& participantId,
                                              const RepoId& subId,
                                              const DDS::StringSeq& params)

{
  try {
    return get_dcps_info()->update_subscription_params(domainId, participantId,
      subId, params);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::update_subscription_params: ");
    return false;
  }
}


// Managing reader/writer associations:

void
InfoRepoDiscovery::association_complete(DDS::DomainId_t domainId,
                                        const RepoId& participantId,
                                        const RepoId& localId, const RepoId& remoteId)
{
  try {
    get_dcps_info()->association_complete(domainId, participantId, localId, remoteId);
  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception("ERROR: InfoRepoDiscovery::association_complete: ");
  }
}

void
InfoRepoDiscovery::removeDataReaderRemote(const RepoId& subscriptionId)
{
  DataReaderMap::iterator drr = dataReaderMap_.find(subscriptionId);
  if (drr == dataReaderMap_.end()) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: InfoRepoDiscovery::removeDataReaderRemote: ")
               ACE_TEXT(" could not find DataReader for subscriptionId.\n")));
    return;
  }

  DataReaderRemoteImpl* impl =
    remote_reference_to_servant<DataReaderRemoteImpl>(drr->second.in());
  impl->detach_parent();
  deactivate_remote_object(drr->second.in());

  dataReaderMap_.erase(drr);
}

void
InfoRepoDiscovery::removeDataWriterRemote(const RepoId& publicationId)
{
  DataWriterMap::iterator dwr = dataWriterMap_.find(publicationId);
  if (dwr == dataWriterMap_.end()) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: InfoRepoDiscovery::removeDataWriterRemote: ")
               ACE_TEXT(" could not find DataWriter for publicationId.\n")));
    return;
  }

  DataWriterRemoteImpl* impl =
    remote_reference_to_servant<DataWriterRemoteImpl>(dwr->second.in());
  impl->detach_parent();
  deactivate_remote_object(dwr->second.in());

  dataWriterMap_.erase(dwr);
}

namespace {
  const ACE_TCHAR REPO_SECTION_NAME[] = ACE_TEXT("repository");
}

int
InfoRepoDiscovery::Config::discovery_config(ACE_Configuration_Heap& cf)
{
  const ACE_Configuration_Section_Key& root = cf.root_section();
  ACE_Configuration_Section_Key repo_sect;

  if (cf.open_section(root, REPO_SECTION_NAME, 0, repo_sect) != 0) {
    if (DCPS_debug_level > 0) {
      // This is not an error if the configuration file does not have
      // any repository (sub)section. The code default configuration will be used.
      ACE_DEBUG((LM_NOTICE,
                 ACE_TEXT("(%P|%t) NOTICE: InfoRepoDiscovery::Config::discovery_config ")
                 ACE_TEXT("failed to open [%s] section.\n"),
                 REPO_SECTION_NAME));
    }

    return 0;

  } else {
    // Ensure there are no properties in this section
    ValueMap vm;
    if (pullValues(cf, repo_sect, vm) > 0) {
      // There are values inside [repo]
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) InfoRepoDiscovery::Config::discovery_config ")
                        ACE_TEXT("repo sections must have a subsection name\n")),
                       -1);
    }
    // Process the subsections of this section (the individual repos)
    KeyList keys;
    if (processSections( cf, repo_sect, keys ) != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) InfoRepoDiscovery::Config::discovery_config ")
                        ACE_TEXT("too many nesting layers in the [repo] section.\n")),
                       -1);
    }

    // Loop through the [repo/*] sections
    for (KeyList::const_iterator it=keys.begin(); it != keys.end(); ++it) {
      std::string repo_name = (*it).first;

      ValueMap values;
      pullValues( cf, (*it).second, values );
      Discovery::RepoKey repoKey = Discovery::DEFAULT_REPO;
      bool repoKeySpecified = false, bitIpSpecified = false,
        bitPortSpecified = false;
      std::string repoIor;
      int bitPort = 0;
      std::string bitIp;
      for (ValueMap::const_iterator it=values.begin(); it != values.end(); ++it) {
        std::string name = (*it).first;
        if (name == "RepositoryKey") {
          repoKey = (*it).second;
          repoKeySpecified = true;
          if (DCPS_debug_level > 0) {
            ACE_DEBUG((LM_DEBUG,
                       ACE_TEXT("(%P|%t) [repository/%C]: RepositoryKey == %C\n"),
                       repo_name.c_str(), repoKey.c_str()));
          }

        } else if (name == "RepositoryIor") {
          repoIor = (*it).second;

          if (DCPS_debug_level > 0) {
            ACE_DEBUG((LM_DEBUG,
                       ACE_TEXT("(%P|%t) [repository/%C]: RepositoryIor == %C\n"),
                       repo_name.c_str(), repoIor.c_str()));
          }
        } else if (name == "DCPSBitTransportIPAddress") {
          bitIp = (*it).second;
          bitIpSpecified = true;
          if (DCPS_debug_level > 0) {
            ACE_DEBUG((LM_DEBUG,
                       ACE_TEXT("(%P|%t) [repository/%C]: DCPSBitTransportIPAddress == %C\n"),
                       repo_name.c_str(), bitIp.c_str()));
          }
        } else if (name == "DCPSBitTransportPort") {
          std::string value = (*it).second;
          bitPort = ACE_OS::atoi(value.c_str());
          bitPortSpecified = true;
          if (convertToInteger(value, bitPort)) {
          } else {
            ACE_ERROR_RETURN((LM_ERROR,
                              ACE_TEXT("(%P|%t) InfoRepoDiscovery::Config::discovery_config ")
                              ACE_TEXT("Illegal integer value for DCPSBitTransportPort (%C) in [repository/%C] section.\n"),
                              value.c_str(), repo_name.c_str()),
                             -1);
          }
          if (DCPS_debug_level > 0) {
            ACE_DEBUG((LM_DEBUG,
                       ACE_TEXT("(%P|%t) [repository/%C]: DCPSBitTransportPort == %d\n"),
                       repo_name.c_str(), bitPort));
          }
        } else {
          ACE_ERROR_RETURN((LM_ERROR,
                            ACE_TEXT("(%P|%t) InfoRepoDiscovery::Config::discovery_config ")
                            ACE_TEXT("Unexpected entry (%C) in [repository/%C] section.\n"),
                            name.c_str(), repo_name.c_str()),
                           -1);
        }
      }

      if (values.find("RepositoryIor") == values.end()) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("(%P|%t) InfoRepoDiscovery::Config::discovery_config ")
                          ACE_TEXT("Repository section [repository/%C] section is missing RepositoryIor value.\n"),
                          repo_name.c_str()),
                         -1);
      }

      if (!repoKeySpecified) {
        // If the RepositoryKey option was not specified, use the section
        // name as the repo key
        repoKey = repo_name;
      }
      InfoRepoDiscovery_rch discovery =
        new InfoRepoDiscovery(repoKey, repoIor.c_str());
      if (bitPortSpecified) discovery->bit_transport_port(bitPort);
      if (bitIpSpecified) discovery->bit_transport_ip(bitIp);
      TheServiceParticipant->add_discovery(
        DCPS::static_rchandle_cast<Discovery>(discovery));
    }
  }

  return 0;
}

InfoRepoDiscovery::StaticInitializer::StaticInitializer()
{
  TheServiceParticipant->register_discovery_type("repository", new Config);
}

int
IRDiscoveryLoader::init(int, ACE_TCHAR*[])
{
  // no-op: since the library is loaded, InfoRepoDiscovery::StaticInitializer
  // has already been constructed.
  return 0;
}

ACE_FACTORY_DEFINE(OpenDDS_InfoRepoDiscovery, IRDiscoveryLoader);
ACE_STATIC_SVC_DEFINE(
  IRDiscoveryLoader,
  ACE_TEXT("OpenDDS_InfoRepoDiscovery"),
  ACE_SVC_OBJ_T,
  &ACE_SVC_NAME(IRDiscoveryLoader),
  ACE_Service_Type::DELETE_THIS | ACE_Service_Type::DELETE_OBJ,
  0)

} // namespace DCPS
} // namespace OpenDDS
