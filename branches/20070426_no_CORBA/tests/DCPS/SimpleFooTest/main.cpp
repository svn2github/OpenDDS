// -*- C++ -*-
// ============================================================================
/**
 *  @file   main.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================



#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"

#include "tests/DCPS/FooType/FooTypeSupportImpl.h"

#include "ace/Get_Opt.h"

#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/Service_Participant.h"

// if COND fails then log error and abort with -1.
#define TEST_CHECK(COND) \
  if (!( COND )) \
      ACE_ERROR((LM_ERROR,"(%N:%l) FAILED on TEST_CHECK(%s)%a\n",\
        #COND , -1));

const long  MY_DOMAIN   = 411;
const char* MY_TOPIC    = "foo";
const char* MY_TYPE     = "foo";

/// parse the command line arguments
int parse_args (int argc, char *argv[])
{
  ACE_Get_Opt get_opts (argc, argv, "");
  int c;

  while ((c = get_opts ()) != -1)
    switch (c)
      {

      case '?':
      default:
        ACE_ERROR_RETURN ((LM_ERROR,
                           "usage:  %s "
                           "-k <DCPSInfo ior> "
                           "\n",
                           argv [0]),
                          -1);
      }
  // Indicates sucessful parsing of the command line
  return 0;
}


int main (int argc, char *argv[])
{
  try
    {
      ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);


      FooTypeSupportImpl* fts_servant = new FooTypeSupportImpl();
      TAO::DCPS::LocalObject_var safe_servant = fts_servant;

      FooTypeSupport_var fts =
        TAO::DCPS::servant_to_reference (fts_servant);


      ::DDS::DomainParticipant_var dp =
        dpf->create_participant(MY_DOMAIN,
                                PARTICIPANT_QOS_DEFAULT,
                                ::DDS::DomainParticipantListener::_nil());
      TEST_CHECK (! CORBA::is_nil (dp.in ()));

      if (::DDS::RETCODE_OK != fts->register_type(dp.in (), MY_TYPE))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to register the FooTypeSupport.")));
          return 1;
        }


      ::DDS::Topic_var topic =
        dp->create_topic (MY_TOPIC,
                          MY_TYPE,
                          TOPIC_QOS_DEFAULT,
                          ::DDS::TopicListener::_nil());
      TEST_CHECK (! CORBA::is_nil (topic.in ()));

      ::DDS::Publisher_var pub =
        dp->create_publisher(PUBLISHER_QOS_DEFAULT,
                             ::DDS::PublisherListener::_nil());
      TEST_CHECK (! CORBA::is_nil (pub.in ()));

      ::DDS::DataWriter_var dw =
        pub->create_datawriter(topic.in (),
                               DATAWRITER_QOS_DEFAULT,
                               ::DDS::DataWriterListener::_nil());
      TEST_CHECK (! CORBA::is_nil (dw.in ()));

      FooDataWriter_var foo_dw = FooDataWriter::_narrow(dw.in ());
      TEST_CHECK (! CORBA::is_nil (foo_dw.in ()));

      Foo foo_write_data;
      foo_write_data.key = 101010;
      foo_write_data.x = (float) 123.456;
      foo_write_data.y = (float) 987.654;

      foo_dw->write(foo_write_data,
                    0);


      ::DDS::Subscriber_var sub =
        dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                              ::DDS::SubscriberListener::_nil());
      TEST_CHECK (! CORBA::is_nil (sub.in ()));

      ::DDS::TopicDescription_var description =
        dp->lookup_topicdescription(MY_TOPIC);
      TEST_CHECK (! CORBA::is_nil (description.in ()));

      TAO::DCPS::TopicDescriptionImpl* ti =
        TAO::DCPS::reference_to_servant<TAO::DCPS::TopicDescriptionImpl>
        (description.in ());
      TEST_CHECK (ti != 0);

      ::DDS::DataReader_var dr =
        sub->create_datareader(description.in (),
                               DATAREADER_QOS_DEFAULT,
                               ::DDS::DataReaderListener::_nil());
      TEST_CHECK (! CORBA::is_nil (dr.in ()));

      FooDataReader_var foo_dr = FooDataReader::_narrow(dr.in ());
      TEST_CHECK (! CORBA::is_nil (foo_dr.in ()));

      Foo foo_read_data;
      foo_read_data.key = 22222;
      foo_read_data.x = (float) -795.295;
      foo_read_data.y = (float) 56987.2;

      ::DDS::SampleInfo si;
      foo_dr->read_next_sample(foo_read_data, si);

      // check for equality
      TEST_CHECK (foo_write_data.key == foo_read_data.key);
      TEST_CHECK (foo_write_data.x == foo_read_data.x);
      TEST_CHECK (foo_write_data.y == foo_read_data.y);


      // clean up the objects
      ::DDS::ReturnCode_t retcode;
      retcode = pub->delete_datawriter(dw.in ());
      TEST_CHECK(::DDS::RETCODE_OK == retcode);
      retcode = dp->delete_publisher(pub.in ());
      TEST_CHECK(::DDS::RETCODE_OK == retcode);

      retcode = sub->delete_datareader(dr.in ());
      TEST_CHECK(::DDS::RETCODE_OK == retcode);
      retcode = dp->delete_subscriber(sub.in());
      TEST_CHECK(::DDS::RETCODE_OK == retcode);

      retcode = dp->delete_topic(topic.in ());
      TEST_CHECK(::DDS::RETCODE_OK == retcode);
      retcode = dpf->delete_participant(dp.in ());
      TEST_CHECK(::DDS::RETCODE_OK == retcode);

      TheServiceParticipant->shutdown ();

    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in main.cpp:");
      return 1;
    }

  return 0;
}
