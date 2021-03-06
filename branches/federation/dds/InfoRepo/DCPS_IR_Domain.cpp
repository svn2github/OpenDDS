#include "DcpsInfo_pch.h"
#include /**/ "DCPS_IR_Domain.h"

#include /**/ "DCPS_IR_Participant.h"
#include /**/ "DCPS_IR_Topic_Description.h"
#include "DomainParticipantListener_i.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/BuiltInTopicUtils.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/PublisherImpl.h"


#if !defined (DDS_HAS_MINIMUM_BIT)
#include "dds/DdsDcpsInfrastructureTypeSupportImpl.h"

#include "dds/DCPS/BuiltInTopicUtils.h"
#endif // !defined (DDS_HAS_MINIMUM_BIT)

#include "dds/DCPS/Transient_Kludge.h"

#include /**/ "tao/debug.h"

#include <sstream>
#include <algorithm>

namespace { // Anonymous namespace for predicate functor definitions.

/// obj( description) == true if description.topic_ == topic;
class IsTheTopic {
  public:
    IsTheTopic( const char* topic) : topic_( topic) { }

    bool operator()( DCPS_IR_Topic_Description* description) {
      return (0 == ACE_OS::strcmp( this->topic_, description->get_name()));
    }

  private:
    const char* topic_;
};

} // End of anonymous namespace.

DCPS_IR_Domain::DCPS_IR_Domain (::DDS::DomainId_t id, GuidGenerator& generator)
:
 id_(id),
 participantIdGenerator_( generator),
 useBIT_(false)
{
}



DCPS_IR_Domain::~DCPS_IR_Domain()
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (0 != cleanup_built_in_topics() )
    {
      ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: Failed to clean up the Built-In Topics!\n"));
    }
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}

const DCPS_IR_Participant_Map&
DCPS_IR_Domain::participants() const
{
  return this->participants_;
}

DCPS_IR_Participant*
DCPS_IR_Domain::participant( const OpenDDS::DCPS::RepoId& id) const
{
  DCPS_IR_Participant_Map::const_iterator where
    = this->participants_.find( id);
  if( where != this->participants_.end()) {
    return where->second;

  } else {
    return 0;
  }
}


int DCPS_IR_Domain::add_participant(DCPS_IR_Participant* participant)
{
  OpenDDS::DCPS::RepoId participantId = participant->get_id ();
  CORBA::Long key = ::OpenDDS::DCPS::GuidConverter( participantId);

  DCPS_IR_Participant_Map::iterator where
    = this->participants_.find( participantId);
  if( where == this->participants_.end()) {
    this->participants_.insert(
      DCPS_IR_Participant_Map::value_type( participantId, participant)
    );

    // Publish the BIT information
    publish_participant_bit(participant);

    if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
      std::stringstream buffer;
      buffer << participantId << "(" << std::hex << key << ")";
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::add_participant: ")
        ACE_TEXT("added participant %s in domain %d ")
        ACE_TEXT("at 0x%x.\n"),
        buffer.str().c_str(),
        id_,
        participant
      ));
    }

  } else {
    if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
      std::stringstream buffer;
      buffer << participantId << "(" << std::hex << key << ")";
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) WARNING: DCPS_IR_Domain::add_participant: ")
        ACE_TEXT("attempt to add already existing participant %s in domain %d.\n"),
        buffer.str().c_str(),
        id_
      ));
    }
    return 1;
  }

  KeyToIdMap::iterator location = this->participantKeyToIdMap_.find( key);
  if( location != this->participantKeyToIdMap_.end()) {
    if( !(participantId == location->second)) {
      std::stringstream buffer;
      buffer << participantId << "(" << std::hex << key << ")";
      buffer << ", existing Id: " << location->second;
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::add_participant: ")
        ACE_TEXT("Domain %d attempt to add duplicate key 0x%x ")
        ACE_TEXT("for participant id: %s\n"),
        id_,
        key,
        buffer.str().c_str()
      ));
      /// @TODO: This only affects the 'ignore_*()' interfaces, so allow
      //         the service to continue at this time.
      // return -1;
    }

  } else {
    this->participantKeyToIdMap_.insert(
      location,
      KeyToIdMap::value_type( key, participantId)
    );
  }

  return 0;
}



int DCPS_IR_Domain::remove_participant(const OpenDDS::DCPS::RepoId& participantId,
                                       CORBA::Boolean notify_lost)
{
  DCPS_IR_Participant_Map::iterator where
    = this->participants_.find( participantId);
  if( where != this->participants_.end()) {
    // Extract the participant from the map.
    DCPS_IR_Participant* participant = where->second;

    // make sure the participant has cleaned up all publications,
    // subscriptions, and any topic references
    participant->remove_all_dependents( notify_lost);

    // Then remove it from the map.
    this->participants_.erase( where);

    if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
      std::stringstream buffer;
      long key = ::OpenDDS::DCPS::GuidConverter(
                   const_cast<OpenDDS::DCPS::GUID_t*>(&participantId)
                 );
      buffer << participantId << "(" << std::hex << key << ")";

      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::remove_participant: ")
        ACE_TEXT("removed participant %s at 0x%x from domain %d.\n"),
        buffer.str().c_str(),
        participant,
        id_
      ));
    }

    dispose_participant_bit( participant);
    delete participant;
    return 0;

  } else {
    std::stringstream buffer;
    long key = ::OpenDDS::DCPS::GuidConverter(
                 const_cast<OpenDDS::DCPS::GUID_t*>(&participantId)
               );
    buffer << participantId << "(" << std::hex << key << ")";

    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::remove_participant: ")
      ACE_TEXT("unable to find participant %s in domain %d.\n"),
      buffer.str().c_str(),
      id_
    ));
    return 1;
  }
}


OpenDDS::DCPS::TopicStatus DCPS_IR_Domain::add_topic(OpenDDS::DCPS::RepoId_out topicId,
                                                 const char * topicName,
                                                 const char * dataTypeName,
                                                 const ::DDS::TopicQos & qos,
                                                 DCPS_IR_Participant* participantPtr)
{
  topicId = OpenDDS::DCPS::GUID_UNKNOWN;

  OpenDDS::DCPS::RepoId topic_id = participantPtr->get_next_topic_id();
  OpenDDS::DCPS::TopicStatus status = add_topic_i (topic_id, topicName
                                               , dataTypeName
                                               , qos, participantPtr);

  if (status == OpenDDS::DCPS::CREATED) {

    topicId = topic_id;
  }

  return status;
}

OpenDDS::DCPS::TopicStatus
DCPS_IR_Domain::force_add_topic(const OpenDDS::DCPS::RepoId& topicId,
                                const char* topicName,
                                const char* dataTypeName,
                                const ::DDS::TopicQos & qos,
                                DCPS_IR_Participant* participantPtr)
{
  OpenDDS::DCPS::RepoId topic_id = topicId;
  OpenDDS::DCPS::TopicStatus status = add_topic_i (topic_id, topicName
                                               , dataTypeName
                                               , qos, participantPtr);

  return status;
}

OpenDDS::DCPS::TopicStatus DCPS_IR_Domain::add_topic_i (OpenDDS::DCPS::RepoId& topicId,
                                                    const char * topicName,
                                                    const char * dataTypeName,
                                                    const ::DDS::TopicQos & qos,
                                                    DCPS_IR_Participant* participantPtr)
{
  DCPS_IR_Topic_Description* description;
  int descriptionLookup = find_topic_description(topicName, dataTypeName, description);
  if (1 == descriptionLookup)
    {
      topicId = OpenDDS::DCPS::GUID_UNKNOWN;
      return OpenDDS::DCPS::CONFLICTING_TYPENAME;
    }
  else if (-1 == descriptionLookup)
    {
      ACE_NEW_RETURN(description,
                     DCPS_IR_Topic_Description(
                       this,
                       topicName,
                       dataTypeName),
                       OpenDDS::DCPS::NOT_FOUND);

      int descriptionAddition = add_topic_description (description);
      if (0 != descriptionAddition)
        {
          // unable to add the topic
          delete description;
          description = 0;
          topicId = OpenDDS::DCPS::GUID_UNKNOWN;
          if (2 == descriptionAddition)
            {
              return OpenDDS::DCPS::CONFLICTING_TYPENAME;
            }
          else
            {
              return OpenDDS::DCPS::NOT_FOUND;
            }
        }
    }

  DCPS_IR_Topic* topic;
  ACE_NEW_RETURN(topic,
                 DCPS_IR_Topic(
                   topicId,
                   qos,
                   this,
                   participantPtr,
                   description),
                 OpenDDS::DCPS::NOT_FOUND);

  OpenDDS::DCPS::TopicStatus topicStatus = OpenDDS::DCPS::NOT_FOUND;

  switch( description->add_topic( topic)) {
    case 0:
      {
        switch( participantPtr->add_topic_reference(topic)) {
          case 0:
            {
              if (::OpenDDS::DCPS::DCPS_debug_level > 0)
                {
                  std::stringstream buffer;
                  long handle;
                  handle = ::OpenDDS::DCPS::GuidConverter( topicId);
                  buffer << topicId
                         << "(" << std::hex << handle << ")";

                  ACE_DEBUG((LM_DEBUG,
                    ACE_TEXT("(%P|%t) DCPS_IR_Domain::add_topic_i: ")
                    ACE_TEXT("Domain %d successfully added topic %s ")
                    ACE_TEXT("at 0x%x.\n"),
                    this->id_,
                    buffer.str().c_str(),
                    topic
                  ));
                }
              topicStatus = OpenDDS::DCPS::CREATED;

              // Keep a reference to easily locate the topic by id.
              this->idToTopicMap_[ topicId] = topic;

              CORBA::Long key = ::OpenDDS::DCPS::GuidConverter( topicId);
              KeyToIdMap::iterator where = this->topicKeyToIdMap_.find( key);
              if( where != this->topicKeyToIdMap_.end()) {
                if( !(topicId == where->second)) {
                  std::stringstream buffer;
                  buffer << topicId << "(" << std::hex << key << ")";
                  buffer << ", existing Id: " << where->second;
                  ACE_ERROR((LM_ERROR,
                    ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::add_topic_i: ")
                    ACE_TEXT("Domain %d attempt to add duplicate key 0x%x ")
                    ACE_TEXT("for topic id: %s\n"),
                    id_,
                    key,
                    buffer.str().c_str()
                  ));
                }

                /// @TODO: This only affects the 'ignore_*()' interfaces, so allow
                //         the service to continue at this time.
                // // Since TopicStatus is not robust enough to inform here.
                // throw OpenDDS::DCPS::Invalid_Topic();

              } else {
                this->topicKeyToIdMap_.insert(
                  where,
                  KeyToIdMap::value_type( key, topicId)
                );

                // Publish the BIT information
                publish_topic_bit(topic);
              }
            }
            break;

          case 1:
            if (::OpenDDS::DCPS::DCPS_debug_level > 0)
            {
              std::stringstream buffer;
              long handle;
              handle = ::OpenDDS::DCPS::GuidConverter( topicId);
              buffer << topicId
                     << "(" << std::hex << handle << ")";

              ACE_DEBUG((LM_DEBUG,
                ACE_TEXT("(%P|%t) WARNING: DCPS_IR_Domain::add_topic_i: ")
                ACE_TEXT("Domain %d declined to add duplicate topic %s at 0x%x.\n"),
                this->id_,
                buffer.str().c_str(),
                topic
              ));
            }
            topicStatus = OpenDDS::DCPS::NOT_FOUND;
            topicId = OpenDDS::DCPS::GUID_UNKNOWN;
            description->remove_topic(topic);
            delete topic;
            break;

          case -1:
            {
              std::stringstream buffer;
              long handle;
              handle = ::OpenDDS::DCPS::GuidConverter( topicId);
              buffer << topicId
                     << "(" << std::hex << handle << ")";

              ACE_ERROR((LM_ERROR,
                ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::add_topic_i: ")
                ACE_TEXT("Domain %d failed to add topic %s at 0x%x.\n"),
                this->id_,
                buffer.str().c_str(),
                topic
              ));
              topicStatus = OpenDDS::DCPS::NOT_FOUND;
              topicId = OpenDDS::DCPS::GUID_UNKNOWN;
              description->remove_topic(topic);
              delete topic;
            }
            break;
        }
      }
      break;

    case 1:
      if (::OpenDDS::DCPS::DCPS_debug_level > 0) {
        std::stringstream buffer;
        long handle;
        handle = ::OpenDDS::DCPS::GuidConverter( topicId);
        buffer << topicId
               << "(" << std::hex << handle << ")";

        ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) WARNING: DCPS_IR_Domain::add_topic ")
          ACE_TEXT("Unable to add topic 0x%x id %s to Topic Description\n"),
          topic, buffer.str().c_str()));
      }
      topicStatus = OpenDDS::DCPS::NOT_FOUND;
      topicId = OpenDDS::DCPS::GUID_UNKNOWN;
      delete topic;
      break;

    case -1:
      {
        std::stringstream buffer;
        long handle;
        handle = ::OpenDDS::DCPS::GuidConverter( topicId);
        buffer << topicId
               << "(" << std::hex << handle << ")";

        ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::add_topic ")
          ACE_TEXT("Unable to add topic 0x%x id %s to Topic Description\n"),
          topic, buffer.str().c_str()));
        topicStatus = OpenDDS::DCPS::NOT_FOUND;
        topicId = OpenDDS::DCPS::GUID_UNKNOWN;
        delete topic;
      }
      break;
  }
  return topicStatus;
}


OpenDDS::DCPS::TopicStatus
DCPS_IR_Domain::find_topic( const char* topicName, DCPS_IR_Topic*& topic)
{
  IsTheTopic isTheTopic( topicName);
  DCPS_IR_Topic_Description_Set::iterator which
    = std::find_if(
        this->topicDescriptions_.begin(),
        this->topicDescriptions_.end(),
        isTheTopic
      );
  if( which != this->topicDescriptions_.end()) {
    // Extract the topic from the description.
    topic = (*which)->get_first_topic();

    if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
      std::stringstream buffer;
      OpenDDS::DCPS::RepoId topicId = topic->get_id();
      long key = ::OpenDDS::DCPS::GuidConverter( topicId);
      buffer << topicId << "(" << std::hex << key << ")";

      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::find_topic: ")
        ACE_TEXT("located topic %s in domain %d.\n"),
        buffer.str().c_str(),
        id_
      ));
    }
    return OpenDDS::DCPS::FOUND;

  } else {
    // topic = 0;
    return OpenDDS::DCPS::NOT_FOUND;
  }
}

DCPS_IR_Topic*
DCPS_IR_Domain::find_topic( const OpenDDS::DCPS::RepoId& id)
{
  IdToTopicMap::const_iterator location = this->idToTopicMap_.find( id);
  if( location == this->idToTopicMap_.end()) {
    return 0;
  }
  return location->second;
}

OpenDDS::DCPS::TopicStatus DCPS_IR_Domain::remove_topic(DCPS_IR_Participant* part,
                                                    DCPS_IR_Topic*& topic)
{
  DCPS_IR_Topic_Description* description = topic->get_topic_description();

  if (description->remove_topic(topic) != 0)
    {
      // An unknown error means that the description may still
      // have the topic in its topic set.  We can't remove it.
      // The system is an inconsistent state!
      throw OpenDDS::DCPS::Invalid_Topic();
    }

  if (description->get_number_topics() == 0)
    {
      // Remove the topic description
      if (remove_topic_description(description) != 0)
        {
          // An unknown error means that the description may still
          // have the topic in its topic set.
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("(%P|%t) ERROR: Topic Description %s %s  ")
                     ACE_TEXT("was not correctly removed from Domain %d"),
                     description->get_name(),
                     description->get_dataTypeName(),
                     id_
                     ));
        }
      else
        {
          delete description;
          description = 0;
        }
    }
  // description variable is invalid after this point

  if (part->remove_topic_reference(topic->get_id(), topic) != 0)
    {
      std::stringstream participantIdBuffer;
      std::stringstream topicIdBuffer;
      OpenDDS::DCPS::RepoId participantId = part->get_id();
      OpenDDS::DCPS::RepoId topicId       = topic->get_id();
      long handle;

      handle = ::OpenDDS::DCPS::GuidConverter( participantId);
      participantIdBuffer << participantId << "(" << std::hex << handle << ")";

      handle = ::OpenDDS::DCPS::GuidConverter( topicId);
      topicIdBuffer << topicId << "(" << std::hex << handle << ")";

      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Domain %d Topic %d ")
        ACE_TEXT("was not correctly removed from Participant %d"),
        id_,
        topicIdBuffer.str().c_str(),
        participantIdBuffer.str().c_str()
      ));

    }

  // Dispose the BIT information
  dispose_topic_bit(topic);

  delete topic;
  topic = 0;

  return OpenDDS::DCPS::REMOVED;
}



int
DCPS_IR_Domain::find_topic_description(
  const char* name,
  const char* dataTypeName,
  DCPS_IR_Topic_Description*& desc
)
{
  IsTheTopic isTheTopic( name);
  DCPS_IR_Topic_Description_Set::iterator which
    = std::find_if(
        this->topicDescriptions_.begin(),
        this->topicDescriptions_.end(),
        isTheTopic
      );
  if( which != this->topicDescriptions_.end()) {
    if( 0 == ACE_OS::strcmp( dataTypeName, (*which)->get_dataTypeName())) {
      if( ::OpenDDS::DCPS::DCPS_debug_level > 0) {
        ACE_DEBUG((LM_DEBUG,
          ACE_TEXT("(%P|%t) DCPS_IR_Domain::find_topic_description: ")
          ACE_TEXT("located topic description %s/%s in domain %d.\n"),
          name,
          dataTypeName,
          id_
        ));
      }
      desc = *which;
      return 0;

    } else {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::find_topic_description: ")
        ACE_TEXT("searching for topic description %s/%s, ")
        ACE_TEXT("located topic description %s/%s instead in domain %d.\n"),
        name,
        dataTypeName,
        (*which)->get_name(),
        (*which)->get_dataTypeName(),
        id_
      ));
      // desc = 0;
      return 1;
    }

  } else {
    // desc = 0;
    return -1;
  }
}

int DCPS_IR_Domain::init_built_in_topics()
{

#if !defined (DDS_HAS_MINIMUM_BIT)

  // Indicates that BIT subscriber and datareaders should not be created.
  TheTransientKludge->enable (); 

  if (::OpenDDS::DCPS::DCPS_debug_level > 0)
    {
      ACE_DEBUG((LM_DEBUG,
                ACE_TEXT("(%P|%t) DCPS_IR_Domain::init_built_in_topics() ")
                ACE_TEXT(" Initializing Built In Topics for domain %d\n"),
                id_ ));
    }

  try
    {
      bitParticipantFactory_ = TheParticipantFactory;

      bitParticipantListener_ = new OPENDDS_DCPS_DomainParticipantListener_i;

      bitParticipant_ =
        bitParticipantFactory_->create_participant(id_,
                                                   PARTICIPANT_QOS_DEFAULT,
                                                   bitParticipantListener_.in());
      if (CORBA::is_nil (bitParticipant_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil DomainParticipant in ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n")),
                            1);
        }

      int transportResult = init_built_in_topics_transport();
      if (0 != transportResult)
        {
          return transportResult;
        }

      int topicsResult = init_built_in_topics_topics();
      if (0 != topicsResult)
        {
          return topicsResult;
        }

      int datawritersResult = init_built_in_topics_datawriters();
      if (0 != datawritersResult)
        {
          return datawritersResult;
        }
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("ERROR: Exception caught in main.cpp:");
      return 1;
    }

  // enable the Built-In Topics
  useBIT_ = true;

  return 0;
#else
  return 1;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



int DCPS_IR_Domain::init_built_in_topics_topics()
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  try
    {
      ::DDS::TopicQos topic_qos;
      bitParticipant_->get_default_topic_qos(topic_qos);


      // Participant topic
      ::DDS::ParticipantBuiltinTopicDataTypeSupport_var 
        participantTypeSupport(new ::DDS::ParticipantBuiltinTopicDataTypeSupportImpl());

      if (::DDS::RETCODE_OK !=
          participantTypeSupport->register_type(bitParticipant_.in (),
                                                ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC_TYPE))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("(%P|%t) ERROR: Failed to register the ParticipantBuiltinTopicDataTypeSupport.")));
          return 1;
        }

      bitParticipantTopic_ =
        bitParticipant_->create_topic (::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC,
                                       ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC_TYPE,
                                       topic_qos,
                                       ::DDS::TopicListener::_nil());
      if (CORBA::is_nil (bitParticipantTopic_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil %s Topic from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n"),
                             ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC),
                            1);
        }

      // Topic topic
      ::DDS::TopicBuiltinTopicDataTypeSupport_var 
        topicTypeSupport(new ::DDS::TopicBuiltinTopicDataTypeSupportImpl());

      if (::DDS::RETCODE_OK !=
          topicTypeSupport->register_type(bitParticipant_.in (),
                                          ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC_TYPE))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("(%P|%t) ERROR: Failed to register the TopicBuiltinTopicDataTypeSupport.")));
          return 1;
        }

      bitTopicTopic_ =
        bitParticipant_->create_topic (::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC,
                                       ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC_TYPE,
                                       topic_qos,
                                       ::DDS::TopicListener::_nil());
      if (CORBA::is_nil (bitTopicTopic_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil %s Topic from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n"),
                             ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC),
                            1);
        }

      // Subscription topic
      ::DDS::SubscriptionBuiltinTopicDataTypeSupport_var 
        subscriptionTypeSupport (new ::DDS::SubscriptionBuiltinTopicDataTypeSupportImpl());

      if (::DDS::RETCODE_OK !=
          subscriptionTypeSupport->register_type(bitParticipant_.in (),
                                                 ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC_TYPE))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("(%P|%t) ERROR: Failed to register the SubscriptionBuiltinTopicDataTypeSupport.")));
          return 1;
        }

      bitSubscriptionTopic_ =
        bitParticipant_->create_topic (::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC,
                                       ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC_TYPE,
                                       topic_qos,
                                       ::DDS::TopicListener::_nil());
      if (CORBA::is_nil (bitSubscriptionTopic_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil %s Topic from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n"),
                             ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC),
                            1);
        }

      // Publication topic
      ::DDS::PublicationBuiltinTopicDataTypeSupport_var 
        publicationTypeSupport(new ::DDS::PublicationBuiltinTopicDataTypeSupportImpl());

      if (::DDS::RETCODE_OK !=
          publicationTypeSupport->register_type(bitParticipant_.in (),
                                                ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC_TYPE))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("(%P|%t) ERROR: Failed to register the PublicationBuiltinTopicDataTypeSupport.")));
          return 1;
        }

      bitPublicationTopic_ =
        bitParticipant_->create_topic (::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC,
                                       ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC_TYPE,
                                       topic_qos,
                                       ::DDS::TopicListener::_nil());
      if (CORBA::is_nil (bitPublicationTopic_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil %s Topic from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n"),
                             ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC),
                            1);
        }

    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception (
        "ERROR: Exception caught in DCPS_IR_Domain::init_built_in_topics_topics:");
      return 1;
    }

  return 0;

#else

  return 1;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



int DCPS_IR_Domain::init_built_in_topics_datawriters()
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  try
    {
      ::DDS::DataWriter_var datawriter;
      ::DDS::DataWriterQos dw_qos;
      bitPublisher_->get_default_datawriter_qos (dw_qos);
      dw_qos.durability.kind = DDS::TRANSIENT_LOCAL_DURABILITY_QOS;

      // Participant DataWriter
      datawriter =
        bitPublisher_->create_datawriter(bitParticipantTopic_.in (),
                                         dw_qos,
                                         ::DDS::DataWriterListener::_nil());

      bitParticipantDataWriter_ =
        ::DDS::ParticipantBuiltinTopicDataDataWriter::_narrow(datawriter.in ());
      if (CORBA::is_nil (bitParticipantDataWriter_.in()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil DomainParticipant DataWriter from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n")),
                            1);
        }

      // Topic DataWriter
      datawriter =
        bitPublisher_->create_datawriter(bitTopicTopic_.in (),
                                          dw_qos,
                                          ::DDS::DataWriterListener::_nil());

      bitTopicDataWriter_ =
        ::DDS::TopicBuiltinTopicDataDataWriter::_narrow(datawriter.in ());
      if (CORBA::is_nil (bitTopicDataWriter_.in()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil Topic DataWriter from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n")),
                            1);
        }

      // Subscription DataWriter
      datawriter =
        bitPublisher_->create_datawriter(bitSubscriptionTopic_.in (),
                                          dw_qos,
                                          ::DDS::DataWriterListener::_nil());

      bitSubscriptionDataWriter_ =
        ::DDS::SubscriptionBuiltinTopicDataDataWriter::_narrow(
                        datawriter.in ());
      if (CORBA::is_nil (bitSubscriptionDataWriter_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil Subscription DataWriter from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n")),
                            1);
        }

      // Publication DataWriter
      datawriter =
        bitPublisher_->create_datawriter(bitPublicationTopic_.in (),
                                         dw_qos,
                                         ::DDS::DataWriterListener::_nil());

      bitPublicationDataWriter_ =
        ::DDS::PublicationBuiltinTopicDataDataWriter::_narrow(datawriter.in ());
      if (CORBA::is_nil (bitPublicationDataWriter_.in()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil Publication DataWriter from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n")),
                            1);
        }

    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception (
        "ERROR: Exception caught in DCPS_IR_Domain::init_built_in_topics_datawriters:");
      return 1;
    }

  return 0;
#else

  return 1;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



int DCPS_IR_Domain::init_built_in_topics_transport ()
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  try
    {
      transportImpl_ =
        TheTransportFactory->obtain(OpenDDS::DCPS::BIT_ALL_TRAFFIC);

      // Create the Publisher
      bitPublisher_ =
        bitParticipant_->create_publisher(PUBLISHER_QOS_DEFAULT,
                                          ::DDS::PublisherListener::_nil());
      if (CORBA::is_nil (bitPublisher_.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) ERROR: ")
                            ACE_TEXT("Nil Publisher from ")
                            ACE_TEXT("DCPS_IR_Domain::init_built_in_topics.\n")),
                            1);
        }

      // Attach the Publisher with the TransportImpl.
      OpenDDS::DCPS::PublisherImpl* pubServant
        = dynamic_cast<OpenDDS::DCPS::PublisherImpl*>(bitPublisher_.in ());

      OpenDDS::DCPS::AttachStatus status
        = pubServant->attach_transport(transportImpl_.in());

      if (status != OpenDDS::DCPS::ATTACH_OK)
        {
          // We failed to attach to the transport for some reason.
          const char* status_str;

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
                            ACE_TEXT("(%P|%t) ERROR: Failed to attach to the transport. ")
                            ACE_TEXT("AttachStatus == %s\n"), status_str),
                            1);
        }
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception (
        "ERROR: Exception caught in DCPS_IR_Domain::init_built_in_topics_transport:");
      return 1;
    }

  return 0;
#else

  return 1;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



int DCPS_IR_Domain::cleanup_built_in_topics()
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      // clean up the Built-in Topic objects

      ::DDS::ReturnCode_t delDataWriterRetCode;
      if (! CORBA::is_nil (bitPublisher_.in ()))
        {
          delDataWriterRetCode =
            bitPublisher_->delete_datawriter(
                bitParticipantDataWriter_.in ());
          delDataWriterRetCode =
            bitPublisher_->delete_datawriter(
                bitTopicDataWriter_.in ());
          delDataWriterRetCode =
            bitPublisher_->delete_datawriter(
                bitSubscriptionDataWriter_.in ());
          delDataWriterRetCode =
            bitPublisher_->delete_datawriter(
                bitPublicationDataWriter_.in ());

          ::DDS::ReturnCode_t delPublisherRetCode =
            bitParticipant_->delete_publisher(
                bitPublisher_.in ());
          ACE_UNUSED_ARG (delPublisherRetCode);

        } // if (0 != bitPublisher_)


      if (! CORBA::is_nil (bitParticipant_.in ()))
        {
          ::DDS::ReturnCode_t delTopicRetCode;
          delTopicRetCode =
            bitParticipant_->delete_topic(bitParticipantTopic_.in ());
          delTopicRetCode =
            bitParticipant_->delete_topic(bitTopicTopic_.in ());
          delTopicRetCode =
            bitParticipant_->delete_topic(bitSubscriptionTopic_.in ());
          delTopicRetCode =
            bitParticipant_->delete_topic(bitPublicationTopic_.in ());

          ::DDS::ReturnCode_t delParticipantRetCode =
            bitParticipantFactory_->delete_participant(bitParticipant_.in ());
          ACE_UNUSED_ARG (delParticipantRetCode);
        }
    }

  return 0;

#else
  return 1;
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



int DCPS_IR_Domain::add_topic_description(DCPS_IR_Topic_Description*& desc)
{
  DCPS_IR_Topic_Description* discard = 0;
  switch( this->find_topic_description(
                               desc->get_name(),
                               desc->get_dataTypeName(),
                               discard
                             )) {
    case -1:
      this->topicDescriptions_.insert( desc);
      if (::OpenDDS::DCPS::DCPS_debug_level > 0) {
        ACE_DEBUG((LM_DEBUG,
          ACE_TEXT("(%P|%t) DCPS_IR_Domain::add_topic_description: ")
          ACE_TEXT("added Topic Description 0x%x in domain %d.\n"),
          desc,
          id_
        ));
      }
      return 0;

    case 0:
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) WARNING: DCPS_IR_Domain::add_topic_description: ")
        ACE_TEXT("attempt to add existing Topic Description 0x%x to domain %d.\n"),
        desc,
        id_
      ));
      return 1;

    case 1:
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) WARNING: DCPS_IR_Domain::add_topic_description: ")
        ACE_TEXT("attempt to add incompatible Topic Description 0x%x to domain %d.\n"),
        desc,
        id_
      ));
      return 2;

    default:
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::add_topic_description: ")
        ACE_TEXT("unknonw error adding Topic Description 0x%x to domain %d.\n"),
        desc,
        id_
      ));
      return 2;
  }
}

int DCPS_IR_Domain::remove_topic_description(DCPS_IR_Topic_Description*& desc)
{
  DCPS_IR_Topic_Description_Set::iterator where
    = this->topicDescriptions_.find( desc);
  if( where != this->topicDescriptions_.end()) {
    /// @TODO: Is this a leak?  Who owns the contained description?
    // delete where->second;
    this->topicDescriptions_.erase( where);
    return 0;

  } else {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::remove_topic_description: ")
      ACE_TEXT("unable to remove Topic Description 0x%x from domain %d.\n"),
      desc,
      id_
    ));
    return -1;
  }
}

void DCPS_IR_Domain::add_dead_participant(DCPS_IR_Participant* participant)
{
  deadParticipants_.insert(participant);
}

void DCPS_IR_Domain::remove_dead_participants ()
{
  if (0 < deadParticipants_.size() )
    {
      DCPS_IR_Participant* dead = 0;
      DCPS_IR_Participant_Set::ITERATOR iter = deadParticipants_.begin ();
      DCPS_IR_Participant_Set::ITERATOR end = deadParticipants_.end();

      while (iter != end)
        {
          dead = *iter;
          ++iter;

          std::stringstream buffer;
          long handle;
          OpenDDS::DCPS::RepoId participantId = dead->get_id();
          handle = ::OpenDDS::DCPS::GuidConverter( participantId);
          buffer << participantId
                 << "(" << std::hex << handle << ")";

          ACE_ERROR((LM_ERROR,
            ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::remove_dead_participants () ")
            ACE_TEXT("Removing dead participant 0x%x id %s\n"),
            dead,
            buffer.str().c_str()
            ));
          deadParticipants_.remove(dead);

          dead->set_alive(0);

          CORBA::Boolean notify_lost = 1;
          remove_participant(dead->get_id(), notify_lost);
        }
    }
}



::DDS::DomainId_t DCPS_IR_Domain::get_id ()
{
  return id_;
}



OpenDDS::DCPS::RepoId
DCPS_IR_Domain::get_next_participant_id()
{
  return this->participantIdGenerator_.next();
}

void
DCPS_IR_Domain::last_participant_key( long key)
{
  this->participantIdGenerator_.last( key);
}

OpenDDS::DCPS::RepoId
DCPS_IR_Domain::participant( const CORBA::Long key)
{
  KeyToIdMap::iterator where
    = this->participantKeyToIdMap_.find( key);
  if( where != this->participantKeyToIdMap_.end()) {
    if (::OpenDDS::DCPS::DCPS_debug_level > 9) {
      std::stringstream buffer;
      buffer << where->second;
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::participant( handle): ")
        ACE_TEXT("Returning RepoId %s for key 0x%x.\n"),
        buffer.str().c_str(),
        key
      ));
    }
    return where->second;

  } else {
    if (::OpenDDS::DCPS::DCPS_debug_level > 4) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::participant( handle): ")
        ACE_TEXT("Did NOT find RepoId for key 0x%x.\n"),
        key
      ));
    }
    return OpenDDS::DCPS::GUID_UNKNOWN;
  }
}

OpenDDS::DCPS::RepoId
DCPS_IR_Domain::topic( const CORBA::Long key)
{
  KeyToIdMap::iterator where
    = this->topicKeyToIdMap_.find( key);
  if( where != this->topicKeyToIdMap_.end()) {
    if (::OpenDDS::DCPS::DCPS_debug_level > 9) {
      std::stringstream buffer;
      buffer << where->second;
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::topic( key): ")
        ACE_TEXT("Returning RepoId %s for key 0x%x.\n"),
        buffer.str().c_str(),
        key
      ));
    }
    return where->second;

  } else {
    if (::OpenDDS::DCPS::DCPS_debug_level > 4) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::topic( key): ")
        ACE_TEXT("Did NOT find RepoId for key 0x%x.\n"),
        key
      ));
    }
    return OpenDDS::DCPS::GUID_UNKNOWN;
  }
}

OpenDDS::DCPS::RepoId
DCPS_IR_Domain::subscription( const CORBA::Long key)
{
  KeyToIdMap::iterator where
    = this->subscriptionKeyToIdMap_.find( key);
  if( where != this->subscriptionKeyToIdMap_.end()) {
    if (::OpenDDS::DCPS::DCPS_debug_level > 9) {
      std::stringstream buffer;
      buffer << where->second;
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::subscription( key): ")
        ACE_TEXT("Returning RepoId %s for key 0x%x.\n"),
        buffer.str().c_str(),
        key
      ));
    }
    return where->second;

  } else {
    if (::OpenDDS::DCPS::DCPS_debug_level > 4) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::subscription( key): ")
        ACE_TEXT("Did NOT find RepoId for key 0x%x.\n"),
        key
      ));
    }
    return OpenDDS::DCPS::GUID_UNKNOWN;
  }
}

OpenDDS::DCPS::RepoId
DCPS_IR_Domain::publication( const CORBA::Long key)
{
  KeyToIdMap::iterator where
    = this->publicationKeyToIdMap_.find( key);
  if( where != this->publicationKeyToIdMap_.end()) {
    if (::OpenDDS::DCPS::DCPS_debug_level > 9) {
      std::stringstream buffer;
      buffer << where->second;
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::publication( key): ")
        ACE_TEXT("Returning RepoId %s for key 0x%x.\n"),
        buffer.str().c_str(),
        key
      ));
    }
    return where->second;

  } else {
    if (::OpenDDS::DCPS::DCPS_debug_level > 4) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) DCPS_IR_Domain::publication( key): ")
        ACE_TEXT("Did NOT find RepoId for key 0x%x.\n"),
        key
      ));
    }
    return OpenDDS::DCPS::GUID_UNKNOWN;
  }
}

void DCPS_IR_Domain::publish_participant_bit (DCPS_IR_Participant* participant)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      /// @TODO: FIXME!

      // I do *not* know what this test is intended to discriminate.  I
      // am going to err on the side of not causing an error for now.
      OpenDDS::DCPS::RepoId id = participant->get_id();
      OpenDDS::DCPS::GuidConverter converter( id);
      if( converter != 1)
        {
          try
          {
            const ::DDS::DomainParticipantQos* participantQos = participant->get_qos ();

            ::DDS::ParticipantBuiltinTopicData data;
            data.key[0] = id_;
            data.key[1] = converter;
            data.key[2]= 0;
            data.user_data = participantQos->user_data;

            ::DDS::InstanceHandle_t handle
              = bitParticipantDataWriter_->_cxx_register (data);

            participant->set_handle(handle);

            if (::OpenDDS::DCPS::DCPS_debug_level > 0)
            {
              ACE_DEBUG ((LM_DEBUG, 
                "(%P|%t) DCPS_IR_Domain::publish_participant_bit: [ %d, 0x%x, 0x%x], handle %d.\n", 
                data.key[0], data.key[1], data.key[2], handle));
            }

            bitParticipantDataWriter_->write(data,
                                            handle);
          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::publish_participant_bit:");
          }
        }
      else
        {
          participant->set_bit_status(1);
        }

    }
#else
  ACE_UNUSED_ARG(participant);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::publish_topic_bit (DCPS_IR_Topic* topic)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      DCPS_IR_Topic_Description* desc =
        topic->get_topic_description();
      const char* dataTypeName = desc->get_dataTypeName ();

      bool isNotBIT =
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC_TYPE);

      if ( isNotBIT )
        {
          try
          {
            const ::DDS::TopicQos* topicQos = topic->get_topic_qos ();

            OpenDDS::DCPS::RepoId participantId = topic->get_participant_id();
            OpenDDS::DCPS::RepoId topicId       = topic->get_id();

            ::DDS::TopicBuiltinTopicData data;
            data.key[0] = id_;
            data.key[1] = OpenDDS::DCPS::GuidConverter( participantId);
            data.key[2] = OpenDDS::DCPS::GuidConverter( topicId);
            data.name = desc->get_name();
            data.type_name = desc->get_dataTypeName();
            data.durability = topicQos->durability;
            data.durability_service = topicQos->durability_service;
            data.deadline = topicQos->deadline;
            data.latency_budget = topicQos->latency_budget;
            data.liveliness = topicQos->liveliness;
            data.reliability = topicQos->reliability;
            data.transport_priority = topicQos->transport_priority;
            data.lifespan = topicQos->lifespan;
            data.destination_order = topicQos->destination_order;
            data.history = topicQos->history;
            data.resource_limits = topicQos->resource_limits;
            data.ownership = topicQos->ownership;
            data.topic_data = topicQos->topic_data;

            ::DDS::InstanceHandle_t handle
              = bitTopicDataWriter_->_cxx_register (data);

            topic->set_handle(handle);

            if (::OpenDDS::DCPS::DCPS_debug_level > 0)
            {
              ACE_DEBUG ((LM_DEBUG, 
                "(%P|%t) DCPS_IR_Domain::publish_topic_bit: [ %d, 0x%x, 0x%x], handle %d.\n", 
                data.key[0], data.key[1], data.key[2], handle));
            }

            bitTopicDataWriter_->write(data,
                                      handle);
          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::publish_topic_bit:");
          }
        }
      else
        {
          topic->set_bit_status(1);
        }
    }
#else
  ACE_UNUSED_ARG(topic);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::publish_subscription_bit (DCPS_IR_Subscription* subscription)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      DCPS_IR_Topic_Description* desc =
        subscription->get_topic_description();

      const char* dataTypeName = desc->get_dataTypeName ();

      bool isNotBIT =
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC_TYPE);

      if ( isNotBIT )
        {
          try
          {
            const ::DDS::DataReaderQos* readerQos = subscription->get_datareader_qos();
            const ::DDS::SubscriberQos* publisherQos = subscription->get_subscriber_qos ();

            DCPS_IR_Topic* topic = subscription->get_topic ();
            const ::DDS::TopicQos* topicQos = topic->get_topic_qos();

            OpenDDS::DCPS::RepoId participantId  = subscription->get_participant_id();
            OpenDDS::DCPS::RepoId subscriptionId = subscription->get_id();

            ::DDS::SubscriptionBuiltinTopicData data;
            data.key[0] = id_;
            data.key[1] = OpenDDS::DCPS::GuidConverter( participantId);
            data.key[2] = OpenDDS::DCPS::GuidConverter( subscriptionId);
            data.participant_key[0] = id_;
            data.participant_key[1] = OpenDDS::DCPS::GuidConverter( participantId);
            data.participant_key[2] = 0;
            data.topic_name = desc->get_name();
            data.type_name = desc->get_dataTypeName();
            data.durability = readerQos->durability;
            data.deadline = readerQos->deadline;
            data.latency_budget = readerQos->latency_budget;
            data.liveliness = readerQos->liveliness;
            data.reliability = readerQos->reliability;
            data.destination_order = readerQos->destination_order;
            data.user_data = readerQos->user_data;
            data.time_based_filter = readerQos->time_based_filter;
            data.presentation = publisherQos->presentation;
            data.partition = publisherQos->partition;
            data.topic_data = topicQos->topic_data;
            data.group_data = publisherQos->group_data;

            KeyToIdMap::iterator where = this->subscriptionKeyToIdMap_.find( data.key[2]);
            if( where != this->subscriptionKeyToIdMap_.end()) {
              if( !(subscriptionId == where->second)) {
                std::stringstream buffer;
                buffer << subscriptionId << "(" << std::hex << data.key[2] << ")";
                buffer << ", existing Id: " << where->second;
                ACE_ERROR((LM_ERROR,
                  ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::publish_subscription_bit: ")
                  ACE_TEXT("Domain %d attempt to add duplicate key 0x%x ")
                  ACE_TEXT("for subscription %s.\n"),
                  id_,
                  data.key[2],
                  buffer.str().c_str()
                ));
                /// @TODO: This only affects the 'ignore_*()' interfaces, so allow
                //         the service to continue at this time.
                // throw OpenDDS::DCPS::Invalid_Subscription();
              }

            } else {
              this->subscriptionKeyToIdMap_.insert(
                where,
                KeyToIdMap::value_type( data.key[2], subscriptionId)
              );
            }

            ::DDS::InstanceHandle_t handle
              = bitSubscriptionDataWriter_->_cxx_register (data);

            subscription->set_handle(handle);

            if (::OpenDDS::DCPS::DCPS_debug_level > 0)
            {
              ACE_DEBUG ((LM_DEBUG, 
                "(%P|%t) DCPS_IR_Domain::publish_subscription_bit: [ %d, 0x%x, 0x%x], handle %d.\n", 
                data.key[0], data.key[1], data.key[2], handle));
            }

            bitSubscriptionDataWriter_->write(data,
                                              handle);
          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::publish_subscription_bit:");
          }
        }
      else
        {
          subscription->set_bit_status(1);
        }
    }
#else
  ACE_UNUSED_ARG(subscription);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::publish_publication_bit (DCPS_IR_Publication* publication)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      DCPS_IR_Topic_Description* desc =
        publication->get_topic_description();

      const char* dataTypeName = desc->get_dataTypeName ();

      bool isNotBIT =
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_PARTICIPANT_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_SUBSCRIPTION_TOPIC_TYPE) &&
        ACE_OS::strcmp(dataTypeName, ::OpenDDS::DCPS::BUILT_IN_PUBLICATION_TOPIC_TYPE);

      if ( isNotBIT )
        {
          try
          {
            const ::DDS::DataWriterQos* writerQos = publication->get_datawriter_qos();
            const ::DDS::PublisherQos* publisherQos = publication->get_publisher_qos ();

            DCPS_IR_Topic* topic = publication->get_topic ();
            const ::DDS::TopicQos* topicQos = topic->get_topic_qos();

            OpenDDS::DCPS::RepoId participantId = publication->get_participant_id();
            OpenDDS::DCPS::RepoId publicationId = publication->get_id();

            ::DDS::PublicationBuiltinTopicData data;
            data.key[0] = id_;
            data.key[1] = OpenDDS::DCPS::GuidConverter( participantId);
            data.key[2] = OpenDDS::DCPS::GuidConverter( publicationId);
            data.participant_key[0] = id_;
            data.participant_key[1] = OpenDDS::DCPS::GuidConverter( participantId);
            data.participant_key[2] = 0;
            data.topic_name = desc->get_name();
            data.type_name = desc->get_dataTypeName();
            data.durability = writerQos->durability;
            data.durability_service = writerQos->durability_service;
            data.deadline = writerQos->deadline;
            data.latency_budget = writerQos->latency_budget;
            data.liveliness = writerQos->liveliness;
            data.reliability = writerQos->reliability;
            data.user_data = writerQos->user_data;
            data.ownership_strength = writerQos->ownership_strength;
            data.presentation = publisherQos->presentation;
            data.partition = publisherQos->partition;
            data.topic_data = topicQos->topic_data;
            data.group_data = publisherQos->group_data;

            KeyToIdMap::iterator where = this->publicationKeyToIdMap_.find( data.key[2]);
            if( where != this->publicationKeyToIdMap_.end()) {
              if( !(publicationId == where->second)) {
                std::stringstream buffer;
                buffer << publicationId << "(" << std::hex << data.key[2] << ")";
                buffer << ", existing Id: " << where->second;
                ACE_ERROR((LM_ERROR,
                  ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::publish_publication_bit: ")
                  ACE_TEXT("Domain %d attempt to add duplicate key 0x%x ")
                  ACE_TEXT("for publication %s\n"),
                  id_,
                  data.key[2],
                  buffer.str().c_str()
                ));
                /// @TODO: This only affects the 'ignore_*()' interfaces, so allow
                //         the service to continue at this time.
                // throw OpenDDS::DCPS::Invalid_Publication();
              }

            } else {
              this->publicationKeyToIdMap_.insert(
                where,
                KeyToIdMap::value_type( data.key[2], publicationId)
              );
            }

            ::DDS::InstanceHandle_t handle
              = bitPublicationDataWriter_->_cxx_register (data);

            publication->set_handle(handle);

            if (::OpenDDS::DCPS::DCPS_debug_level > 0)
            {
              ACE_DEBUG ((LM_DEBUG, 
                "(%P|%t) DCPS_IR_Domain::publish_publication_bit: [ %d, 0x%x, 0x%x], handle %d.\n", 
                data.key[0], data.key[1], data.key[2], handle));
            }

            bitPublicationDataWriter_->write(data,
                                            handle);
          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::publish_publication_bit:");
          }
        }
      else
        {
          publication->set_bit_status(1);
        }
    }
#else
  ACE_UNUSED_ARG(publication);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::dispose_participant_bit (DCPS_IR_Participant* participant)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      if ( ! participant->is_bit())
        {
          try
          {
            ::DDS::ParticipantBuiltinTopicData key_data;
            ::DDS::InstanceHandle_t handle = participant->get_handle();

            ::DDS::ReturnCode_t retGetKey
              = bitParticipantDataWriter_->get_key_value(key_data,
                                                        handle);

            if (::DDS::RETCODE_OK != retGetKey)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_participant_bit ")
                          ACE_TEXT("Unable to get_key_value for participant ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          participant,
                          handle,
                          retGetKey
                          ));
              }

            ::DDS::ReturnCode_t retDispose =
              bitParticipantDataWriter_->dispose(key_data,
                                                handle);
            if (::DDS::RETCODE_OK != retDispose)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_participant_bit ")
                          ACE_TEXT("Unable to dispose for participant ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          participant,
                          handle,
                          retDispose
                          ));
              }

          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "ERROR: Exception caught in DCPS_IR_Domain::dispose_participant_bit:");
          }
        }
    }
#else
  ACE_UNUSED_ARG(participant);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::dispose_topic_bit (DCPS_IR_Topic* topic)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      if ( ! topic->is_bit())
        {
          try
          {
            ::DDS::TopicBuiltinTopicData key_data;
            ::DDS::InstanceHandle_t handle = topic->get_handle();

            ::DDS::ReturnCode_t retGetKey
              = bitTopicDataWriter_->get_key_value(key_data,
                                                  handle);

            if (::DDS::RETCODE_OK != retGetKey)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_topic_bit ")
                          ACE_TEXT("Unable to get_key_value for topic ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          topic,
                          handle,
                          retGetKey
                          ));
              }

            ::DDS::ReturnCode_t retDispose =
              bitTopicDataWriter_->dispose(key_data,
                                          handle);
            if (::DDS::RETCODE_OK != retDispose)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_topic_bit ")
                          ACE_TEXT("Unable to dispose for topic ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          topic,
                          handle,
                          retDispose
                          ));
              }
          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::dispose_topic_bit:");
          }
        }
    }
#else
  ACE_UNUSED_ARG(topic);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::dispose_subscription_bit (DCPS_IR_Subscription* subscription)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      if ( ! subscription->is_bit() )
        {
          try
          {
            ::DDS::SubscriptionBuiltinTopicData key_data;
            ::DDS::InstanceHandle_t handle = subscription->get_handle();

            ::DDS::ReturnCode_t retGetKey
              = bitSubscriptionDataWriter_->get_key_value(key_data,
                                                          handle);

            if (::DDS::RETCODE_OK != retGetKey)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_subscription_bit ")
                          ACE_TEXT("Unable to get_key_value for subscription ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          subscription,
                          handle,
                          retGetKey
                          ));
              }

            ::DDS::ReturnCode_t retDispose =
              bitSubscriptionDataWriter_->dispose(key_data,
                                                  handle);
            if (::DDS::RETCODE_OK != retDispose)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_subscription_bit ")
                          ACE_TEXT("Unable to dispose for subscription ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          subscription,
                          handle,
                          retDispose
                          ));
              }

          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::dispose_subscription_bit:");
          }
        }
    }
#else
  ACE_UNUSED_ARG(subscription);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}



void DCPS_IR_Domain::dispose_publication_bit (DCPS_IR_Publication* publication)
{
#if !defined (DDS_HAS_MINIMUM_BIT)
  if (useBIT_)
    {
      if ( ! publication->is_bit())
        {
          try
          {
            ::DDS::PublicationBuiltinTopicData key_data;
            ::DDS::InstanceHandle_t handle = publication->get_handle();

            ::DDS::ReturnCode_t retGetKey
              = bitPublicationDataWriter_->get_key_value(key_data, handle);

            if (::DDS::RETCODE_OK != retGetKey)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_publication_bit ")
                          ACE_TEXT("Unable to get_key_value for publication ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          publication,
                          handle,
                          retGetKey
                          ));
              }

            ::DDS::ReturnCode_t retDispose =
              bitPublicationDataWriter_->dispose(key_data,
                                                handle);
            if (::DDS::RETCODE_OK != retDispose)
              {
                ACE_ERROR((LM_ERROR,
                          ACE_TEXT("(%P|%t) ERROR: DCPS_IR_Domain::dispose_publication_bit ")
                          ACE_TEXT("Unable to dispose for publication ptr 0x%x handle %d.  ")
                          ACE_TEXT("Call returned %d.\n"),
                          publication,
                          handle,
                          retDispose
                          ));
              }

          }
          catch (const CORBA::Exception& ex)
          {
            ex._tao_print_exception (
              "(%P|%t) ERROR: Exception caught in DCPS_IR_Domain::dispose_publication_bit:");
          }
        }
    }
#else
  ACE_UNUSED_ARG(publication);
#endif // !defined (DDS_HAS_MINIMUM_BIT)
}





#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION)

template class ACE_Node<DCPS_IR_Topic_Description*>;
template class ACE_Unbounded_Set<DCPS_IR_Topic_Description*>;
template class ACE_Unbounded_Set_Iterator<DCPS_IR_Topic_Description*>;

template class ACE_Node<DCPS_IR_Participant*>;
template class ACE_Unbounded_Set<DCPS_IR_Participant*>;
template class ACE_Unbounded_Set_Iterator<DCPS_IR_Participant*>;

template class ACE_Map_Entry<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*>;
template class ACE_Map_Manager<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>;
template class ACE_Map_Iterator_Base<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>;
template class ACE_Map_Iterator<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>;
template class ACE_Map_Reverse_Iterator<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>;

#elif defined (ACE_HAS_TEMPLATE_INSTANTIATION_PRAGMA)

#pragma instantiate ACE_Node<DCPS_IR_Topic_Description*>
#pragma instantiate ACE_Unbounded_Set<DCPS_IR_Topic_Description*>
#pragma instantiate ACE_Unbounded_Set_Iterator<DCPS_IR_Topic_Description*>

#pragma instantiate ACE_Node<DCPS_IR_Participant*>
#pragma instantiate ACE_Unbounded_Set<DCPS_IR_Participant*>
#pragma instantiate ACE_Unbounded_Set_Iterator<DCPS_IR_Participant*>

#pragma instantiate ACE_Map_Entry<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*>
#pragma instantiate ACE_Map_Manager<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>
#pragma instantiate ACE_Map_Iterator_Base<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>
#pragma instantiate ACE_Map_Iterator<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>
#pragma instantiate ACE_Map_Reverse_Iterator<OpenDDS::DCPS::RepoId,DCPS_IR_Participant*,ACE_Null_Mutex>

#endif /* ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */
