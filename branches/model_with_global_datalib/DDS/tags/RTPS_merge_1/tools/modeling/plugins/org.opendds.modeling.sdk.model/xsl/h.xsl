<xsl:stylesheet version='1.0'
     xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
     xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'
     xmlns:xmi='http://www.omg.org/XMI'
     xmlns:opendds='http://www.opendds.org/modeling/schemas/OpenDDS/1.0'>
  <!--
    ** $Id$
    **
    ** Generate C++ header code.
    **
    -->
<xsl:include href="common.xsl"/>

<xsl:output method="text"/>
<xsl:strip-space elements="*"/>

<!-- Global node sets -->
<xsl:variable name="types"        select="//types"/>

<!-- Extract the name of the model once. -->
<xsl:variable name = "modelname" select = "/opendds:OpenDDSModel/@name"/>
<xsl:variable name = "MODELNAME" select = "translate($modelname, $lower, $upper)"/>

<!-- process the entire model document to produce the C++ header. -->
<xsl:template match="/">

  <xsl:text>#ifndef </xsl:text>
  <xsl:value-of select="$MODELNAME"/>
  <xsl:text>_T_H
#define </xsl:text>
  <xsl:value-of select="$MODELNAME"/>
  <xsl:text>_T_H

#include "</xsl:text>
  <xsl:value-of select="$modelname"/>
  <xsl:text>_export.h"

#include "model/Exceptions.h"

#include "dds/DdsDcpsInfrastructureC.h" // For QoS Policy types.
#include "dds/DCPS/transport/framework/TransportDefs.h"

namespace DDS {
  class DomainParticipant; // For type registration
} // End of namespace DDS

namespace OpenDDS { namespace DCPS {
  class TransportConfiguration;
} } // End of namespace OpenDDS::DCPS

namespace OpenDDS { namespace Model {

  using namespace OpenDDS::DCPS;
  using namespace DDS;

</xsl:text>
  <xsl:apply-templates mode="declare"/>
  <xsl:text>
} // End namespace Model
} // End namespace OpenDDS

// The template implementation.
</xsl:text>
  <xsl:apply-templates mode="define"/>
  <xsl:text>
#if defined (ACE_TEMPLATES_REQUIRE_SOURCE)
#include "</xsl:text>
  <xsl:value-of select="$modelname"/>
  <xsl:text>_T.cpp"
#endif /* ACE_TEMPLATES_REQUIRE_SOURCE */

#if defined (ACE_TEMPLATES_REQUIRE_PRAGMA)
#pragma implementation ("</xsl:text>
  <xsl:value-of select="$modelname"/>
  <xsl:text>_T.cpp")
#endif /* ACE_TEMPLATES_REQUIRE_PRAGMA */

// Establish the model interfaces for use by the application.

</xsl:text>
  <xsl:value-of select="concat('#include &quot;', $modelname, 'TypeSupportImpl.h&quot;', $newline)"/>
<xsl:text>
#include "model/DefaultInstanceTraits.h"
#include "model/Service_T.h"

#endif /* </xsl:text>
  <xsl:value-of select="$MODELNAME"/>
  <xsl:text>_T_H */

</xsl:text>
</xsl:template>
<!-- End of main processing template. -->

<!-- For a package containing a DCPSlib, output a namespace -->
<xsl:template match="packages[.//libs[@xsi:type='opendds:DcpsLib']]" mode="declare">
  <xsl:value-of select="concat('namespace ', @name, ' {', $newline)"/>
  <xsl:apply-templates mode="declare"/>
  <xsl:value-of select="concat('} // End namespace ', @name, $newline)"/>
</xsl:template>

<!-- For a DCPSlib, output a namespace and the Elements class -->
<xsl:template match="libs[@xsi:type='opendds:DcpsLib']" mode="declare">
  <xsl:call-template name="output-comment"/>
  <xsl:value-of select="concat('namespace ', @name, ' {', $newline)"/>
  <xsl:text> 
  class Elements {
    public:
</xsl:text>
  <!-- Match within the DCPSlib -->
  <xsl:variable name="lib-participants" select=".//participants"/>
  <xsl:variable name="lib-topics"       select=".//topicDescriptions"/>
  <xsl:variable name="lib-cf-topics"    select=".//topicDescriptions[@xsi:type='topics:ContentFilteredTopic']"/>
  <xsl:variable name="lib-multitopics"  select=".//topicDescriptions[@xsi:type='topics:MultiTopic']"/>
  <xsl:variable name="lib-publishers"   select=".//publishers"/>
  <xsl:variable name="lib-subscribers"  select=".//subscribers"/>
  <xsl:variable name="lib-writers"      select=".//writers"/>
  <xsl:variable name="lib-readers"      select=".//readers"/>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'Participants'"/>
    <xsl:with-param name="values" select="$lib-participants"/>
  </xsl:call-template>

  <xsl:value-of select="concat('      class Types {', $newline)"/>
  <xsl:value-of select="concat('        public: enum Values {', $newline)"/>
  <xsl:for-each select="$types[@xmi:id = $lib-topics/@datatype]">
    <xsl:variable name="enum">
      <xsl:call-template name="type-enum"/>
    </xsl:variable>
    <xsl:value-of select="concat('          ', $enum, ',', $newline)"/>
  </xsl:for-each>
  <xsl:value-of select="concat('          LAST_INDEX', $newline)"/>
  <xsl:value-of select="concat('        };', $newline)"/>
  <xsl:value-of select="concat('      };', $newline)"/>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'Topics'"/>
    <xsl:with-param name="values" select="$lib-topics"/>
  </xsl:call-template>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'ContentFilteredTopics'"/>
    <xsl:with-param name="values" select="$lib-cf-topics"/>
  </xsl:call-template>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'MultiTopics'"/>
    <xsl:with-param name="values" select="$lib-multitopics"/>
  </xsl:call-template>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'Publishers'"/>
    <xsl:with-param name="values" select="$lib-publishers"/>
  </xsl:call-template>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'Subscribers'"/>
    <xsl:with-param name="values" select="$lib-subscribers"/>
  </xsl:call-template>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'DataWriters'"/>
    <xsl:with-param name="values" select="$lib-writers"/>
  </xsl:call-template>

  <xsl:call-template name="generate-enum">
    <xsl:with-param name="class" select="'DataReaders'"/>
    <xsl:with-param name="values" select="$lib-readers"/>
  </xsl:call-template>

<xsl:text>      class Data {
        public:
          Data();
          ~Data();

          ///{ @name Qos Policy values
          DDS::DomainParticipantQos qos(Participants::Values which);
          DDS::TopicQos             qos(Topics::Values which);
          DDS::PublisherQos         qos(Publishers::Values which);
          DDS::SubscriberQos        qos(Subscribers::Values which);
          DDS::DataWriterQos        qos(DataWriters::Values which);
          DDS::DataReaderQos        qos(DataReaders::Values which);

          bool copyTopicQos(DataWriters::Values which);
          bool copyTopicQos(DataReaders::Values which);

          void copyPublicationQos(DataWriters::Values which, DDS::DataWriterQos&amp; writerQos);
          void copySubscriptionQos(DataReaders::Values which, DDS::DataReaderQos&amp; readerQos);
          ///}

          ///{ @name Other configuration values
          long        mask(Participants::Values which);
          long        mask(Publishers::Values which);
          long        mask(Subscribers::Values which);
          long        mask(Topics::Values which);
          long        mask(DataWriters::Values which);
          long        mask(DataReaders::Values which);
          long        domain(Participants::Values which);
          const char* typeName(Types::Values which);
          const char* topicName(Topics::Values which);
          ///}

          /// Type registration.
          void registerType(Types::Values type, DDS::DomainParticipant* participant);

          ///{ @name Containment Relationships
          Participants::Values participant(Publishers::Values which);
          Participants::Values participant(Subscribers::Values which);
          Types::Values        type(Topics::Values which);
          Topics::Values       topic(DataWriters::Values which);
          Topics::Values       topic(DataReaders::Values which);
          ContentFilteredTopics::Values contentFilteredTopic(Topics::Values which);
          char*                filterExpression(ContentFilteredTopics::Values which);
          char*                topicExpression(MultiTopics::Values which);
          MultiTopics::Values  multiTopic(Topics::Values which);
          Topics::Values       relatedTopic(ContentFilteredTopics::Values which);
          Publishers::Values   publisher(DataWriters::Values which);
          Subscribers::Values  subscriber(DataReaders::Values which);
          OpenDDS::DCPS::TransportIdType transport(Publishers::Values which);
          OpenDDS::DCPS::TransportIdType transport(Subscribers::Values which);
          ///}

        private:

          // Initialization.
          void loadDomains();
          void loadTopics();
          void loadMaps();

          void buildParticipantsQos();
          void buildTopicsQos();
          void buildPublishersQos();
          void buildSubscribersQos();
          void buildPublicationsQos();
          void buildSubscriptionsQos();

          // Basic array containers since we only allow access using the
          // defined enumeration values.
</xsl:text>
<xsl:if test="$lib-participants">
  <xsl:text>
          unsigned long             domains_[                Participants::LAST_INDEX];
          DDS::DomainParticipantQos participantsQos_[        Participants::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-publishers">
  <xsl:text>
          Participants::Values      publisherParticipants_[   Publishers::LAST_INDEX];
          DDS::PublisherQos         publishersQos_[           Publishers::LAST_INDEX];
          OpenDDS::DCPS::TransportIdType publisherTransports_[Publishers::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-subscribers">
  <xsl:text>
          Participants::Values      subscriberParticipants_[   Subscribers::LAST_INDEX];
          DDS::SubscriberQos        subscribersQos_[           Subscribers::LAST_INDEX];
          OpenDDS::DCPS::TransportIdType subscriberTransports_[Subscribers::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-writers">
  <xsl:text>
          Topics::Values            writerTopics_[           DataWriters::LAST_INDEX];
          Publishers::Values        publishers_[             DataWriters::LAST_INDEX];
          DDS::DataWriterQos        writersQos_[             DataWriters::LAST_INDEX];
          bool                      writerCopyTopicQos_[     DataWriters::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-readers">
<xsl:text>
          Topics::Values            readerTopics_[           DataReaders::LAST_INDEX];
          Subscribers::Values       subscribers_[            DataReaders::LAST_INDEX];
          DDS::DataReaderQos        readersQos_[             DataReaders::LAST_INDEX];
          bool                      readerCopyTopicQos_[     DataReaders::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-topics">
<xsl:text>
          Types::Values                 types_[      Topics::LAST_INDEX];
          const char*                   topicNames_[ Topics::LAST_INDEX];
          DDS::TopicQos                 topicsQos_[  Topics::LAST_INDEX];
          char*                         typeNames_[   Types::LAST_INDEX];
          ContentFilteredTopics::Values cfTopics_[   Topics::LAST_INDEX];
          MultiTopics::Values           multiTopics_[Topics::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-cf-topics">
<xsl:text>
          Topics::Values relatedTopics_    [ContentFilteredTopics::LAST_INDEX];
          char*          filterExpressions_[ContentFilteredTopics::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:if test="$lib-multitopics">
<xsl:text>
          char*          topicExpressions_[MultiTopics::LAST_INDEX];
</xsl:text>
</xsl:if>
<xsl:text>
      }; // End class Data
  }; // End class Elements
</xsl:text>
  <xsl:value-of select="concat('} // End namespace ', @name, $newline)"/>
</xsl:template>

<!-- Output the template implementations for the DCPSlib -->
<xsl:template match="libs[@xsi:type='opendds:DcpsLib']" mode="define">
  <xsl:variable name="lib-participants" select=".//participants"/>
  <xsl:variable name="lib-topics"       select=".//topicDescriptions"/>
  <xsl:variable name="lib-cf-topics"    select=".//topicDescriptions[@xsi:type = 'topics:ContentFilteredTopic']"/>
  <xsl:variable name="lib-multitopics"  select=".//topicDescriptions[@xsi:type = 'topics:MultiTopic']"/>
  <xsl:variable name="lib-publishers"   select=".//publishers"/>
  <xsl:variable name="lib-subscribers"  select=".//subscribers"/>
  <xsl:variable name="lib-writers"      select=".//writers"/>
  <xsl:variable name="lib-readers"      select=".//readers"/>

  <xsl:variable name="elements-qname">
    <xsl:call-template name="elements-qname"/>
  </xsl:variable>
  <xsl:variable name="data-qname">
    <xsl:call-template name="data-qname"/>
  </xsl:variable>

<xsl:text>
inline
DDS::DomainParticipantQos
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::qos(Participants::Values which)
{
  if(which &lt; 0 || which >= Participants::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-participants">
    <xsl:text>  return this->participantsQos_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return DDS::DomainParticipantQos(); // not valid when no domain participants defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
DDS::TopicQos
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::qos(Topics::Values which)
{
  if (which &lt; 0 || which >= Topics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-topics">
    <xsl:text>  return this->topicsQos_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return DDS::TopicQos(); // not valid when no topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
DDS::PublisherQos
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::qos(Publishers::Values which)
{
  if (which &lt; 0 || which >= Publishers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-publishers">
    <xsl:text>  return this->publishersQos_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return PublisherQos();  // not valid when no publishers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
DDS::SubscriberQos
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::qos(Subscribers::Values which)
{
  if (which &lt; 0 || which >= Subscribers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-subscribers">
    <xsl:text>  return this->subscribersQos_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return DDS::SubscriberQos(); // not valid when no subscribers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
DDS::DataWriterQos
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::qos(DataWriters::Values which)
{
  if (which &lt; 0 || which >= DataWriters::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-writers">
    <xsl:text>  return this->writersQos_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return DDS::DataWriterQos(); // not valid when no data writers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
DDS::DataReaderQos
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::qos(DataReaders::Values which)
{
  if (which &lt; 0 || which >= DataReaders::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-readers">
    <xsl:text>  return this->readersQos_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return DDS::DataReaderQos(); // not valid when no data readers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
bool
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::copyTopicQos(DataWriters::Values which)
{
  if (which &lt; 0 || which >= DataWriters::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-writers">
    <xsl:text>  return this->writerCopyTopicQos_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return false; // not valid when no data writers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
bool
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::copyTopicQos(DataReaders::Values which)
{
  if (which &lt; 0 || which >= DataReaders::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-readers">
    <xsl:text>  return this->readerCopyTopicQos_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return false; // not valid when no data readers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::mask(Participants::Values which)
{
  if (which &lt; 0 || which >= Participants::LAST_INDEX) {
    throw OutOfBoundsException();
  }
  return DEFAULT_STATUS_MASK;
}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::mask( Publishers::Values which)
{
  if( which &lt; 0 || which >= Publishers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
  return DEFAULT_STATUS_MASK;
}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::mask( Subscribers::Values which)
{
  if( which &lt; 0 || which >= Subscribers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
  return DEFAULT_STATUS_MASK;
}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::mask( Topics::Values which)
{
  if( which &lt; 0 || which >= Topics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
  return DEFAULT_STATUS_MASK;
}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::mask( DataWriters::Values which)
{
  if( which &lt; 0 || which >= DataWriters::LAST_INDEX) {
    throw OutOfBoundsException();
  }
  return DEFAULT_STATUS_MASK;
}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::mask( DataReaders::Values which)
{
  if( which &lt; 0 || which >= DataReaders::LAST_INDEX) {
    throw OutOfBoundsException();
  }
  return DEFAULT_STATUS_MASK;
}

inline
long
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::domain( Participants::Values which)
{
  if( which &lt; 0 || which >= Participants::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-participants">
    <xsl:text>  return this->domains_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return 0; // not valid when no domain participants defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
const char*
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::typeName( Types::Values which)
{
  if( which &lt; 0 || which >= Types::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-topics">
    <xsl:text>  return this->typeNames_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return (const char*)NULL; // not valid when no topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
const char*
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::topicName( Topics::Values which)
{
  if( which &lt; 0 || which >= Topics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-topics">
    <xsl:text>  return this->topicNames_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return (const char*)NULL;  // not valid when no topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Participants::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::participant(Publishers::Values which)
{
  if(which &lt; 0 || which >= Publishers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-publishers">
    <xsl:text>  return this->publisherParticipants_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Participants::LAST_INDEX; // not valid when no publishers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Participants::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::participant(Subscribers::Values which)
{
  if(which &lt; 0 || which >= Subscribers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-subscribers">
    <xsl:text>  return this->subscriberParticipants_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Participants::LAST_INDEX; // Not valid when no subscribers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Types::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::type(Topics::Values which)
{
  if(which &lt; 0 || which >= Topics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-topics">
    <xsl:text>  return this->types_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Types::LAST_INDEX; // not valid when no types defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Topics::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::topic(DataWriters::Values which)
{
  if(which &lt; 0 || which >= DataWriters::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-writers">
    <xsl:text>  return this->writerTopics_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Topics::LAST_INDEX; // not valid when no data writers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Topics::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::topic(DataReaders::Values which)
{
  if(which &lt; 0 || which >= DataReaders::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-readers">
    <xsl:text>  return this->readerTopics_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Topics::LAST_INDEX; // not valid when no data readers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::ContentFilteredTopics::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::contentFilteredTopic(Topics::Values which)
{
  if(which &lt; 0 || which >= Topics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-topics">
    <xsl:text>  return this->cfTopics_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return ContentFilteredTopics::LAST_INDEX; // not valid when no topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::MultiTopics::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::multiTopic(Topics::Values which)
{
  if(which &lt; 0 || which >= Topics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-topics">
    <xsl:text>  return this->multiTopics_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return MultiTopics::LAST_INDEX; // not valid when no topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Topics::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::relatedTopic(ContentFilteredTopics::Values which)
{
  if(which &lt; 0 || which >= ContentFilteredTopics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-cf-topics">
    <xsl:text>  return this->relatedTopics_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Topics::LAST_INDEX; // not valid when no CF topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
char* 
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::filterExpression(ContentFilteredTopics::Values which)
{
  if(which &lt; 0 || which >= ContentFilteredTopics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-cf-topics">
    <xsl:text>  return this->filterExpressions_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return 0; // not valid when no CF topics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
char* 
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::topicExpression(MultiTopics::Values which)
{
  if(which &lt; 0 || which >= MultiTopics::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-multitopics">
    <xsl:text>  return this->topicExpressions_[which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return 0; // not valid when no multitopics defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Publishers::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::publisher(DataWriters::Values which)
{
  if(which &lt; 0 || which >= DataWriters::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-writers">
    <xsl:text>  return this->publishers_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text> return Publishers::LAST_INDEX; 
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
</xsl:text>
  <xsl:value-of select="$elements-qname"/>
  <xsl:text>::Subscribers::Values
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::subscriber(DataReaders::Values which)
{
  if(which &lt; 0 || which >= DataReaders::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-readers">
    <xsl:text>  return this->subscribers_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return Subscribers::LAST_INDEX; // not valid when no data readers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
OpenDDS::DCPS::TransportIdType
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::transport(Publishers::Values which)
{
  if(which &lt; 0 || which >= Publishers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-publishers">
    <xsl:text>  return this->publisherTransports_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return 0; // not valid when no publishers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
<xsl:text>}

inline
OpenDDS::DCPS::TransportIdType
</xsl:text>
  <xsl:value-of select="$data-qname"/>
  <xsl:text>::transport(Subscribers::Values which)
{
  if(which &lt; 0 || which >= Subscribers::LAST_INDEX) {
    throw OutOfBoundsException();
  }
</xsl:text>
<xsl:choose>
  <xsl:when test="$lib-subscribers">
    <xsl:text>  return this->subscriberTransports_[ which];
</xsl:text>
  </xsl:when>
  <xsl:otherwise>
    <xsl:text>  return 0;  //  not valid when no subscribers defined
</xsl:text>
  </xsl:otherwise>
</xsl:choose>
    <xsl:text>}
</xsl:text>
</xsl:template>

<!-- Output enumeration within Elements class -->
<xsl:template name="generate-enum">
  <xsl:param name="class" />
  <xsl:param name="values" />

  <xsl:value-of select="concat('      class ',$class, ' {')"/>
  <xsl:call-template name="generate-enum-values">
    <xsl:with-param name="values" select="$values"/>
  </xsl:call-template>
</xsl:template>

<!-- Output enumeration values within Elements class -->
<xsl:template name="generate-enum-values">
  <xsl:param name="values" />
    <xsl:text>
        public: enum Values {
</xsl:text>
    <xsl:for-each select="$values">
      <xsl:call-template name="output-comment">
        <xsl:with-param name="indent" select="'          '"/>
      </xsl:call-template>
      <xsl:text>          </xsl:text>
      <xsl:call-template name="normalize-identifier"/>
      <xsl:value-of select="concat(',', $newline)"/>
    </xsl:for-each>
    <xsl:text>          LAST_INDEX
        };
      };

</xsl:text>
</xsl:template>

<!-- Ignore text -->
<xsl:template match="text()" mode="declare"/>
<xsl:template match="text()" mode="define"/>

</xsl:stylesheet>

