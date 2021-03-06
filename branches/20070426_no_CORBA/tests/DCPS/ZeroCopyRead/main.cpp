// -*- C++ -*-
// ============================================================================
/**
 *  @file   main.cpp
 *
 *  $Id$
 *
 *  Test of zero-copy read.
 */
// ============================================================================


#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/PublisherImpl.h"
#include "SimpleTypeSupportImpl.h"
#include "dds/DCPS/transport/framework/EntryExit.h"

#include "dds/DCPS/transport/simpleUnreliableDgram/SimpleUdpConfiguration.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"

#include "ace/Arg_Shifter.h"

#include <string>

class TestException
{
  public:

    TestException()  {}
    ~TestException() {}
};


const long  MY_DOMAIN   = 411;
const char* MY_TOPIC    = "foo";
const char* MY_TYPE     = "foo";

const ACE_Time_Value max_blocking_time(::DDS::DURATION_INFINITY_SEC);

int use_take = 0;
int multiple_instances = 0;
int max_samples_per_instance = ::DDS::LENGTH_UNLIMITED;
int history_depth = 1 ;
bool support_client_side_BIT = false;

int test_failed = 0;

TAO::DCPS::TransportImpl_rch reader_transport_impl;
TAO::DCPS::TransportImpl_rch writer_transport_impl;

enum TransportInstanceId
{
  SUB_TRAFFIC,
  PUB_TRAFFIC
};



int init_tranport ()
{
  int status = 0;

      reader_transport_impl
        = TheTransportFactory->create_transport_impl (SUB_TRAFFIC,
                                                      "SimpleTcp",
                                                      TAO::DCPS::DONT_AUTO_CONFIG);

      TAO::DCPS::TransportConfiguration_rch reader_config
        = TheTransportFactory->create_configuration (SUB_TRAFFIC, "SimpleTcp");

      if (reader_transport_impl->configure(reader_config.in()) != 0)
        {
          ACE_ERROR((LM_ERROR,
                    ACE_TEXT("(%P|%t) init_transport: sub TCP ")
                    ACE_TEXT(" Failed to configure the transport.\n")));
          status = 1;
        }

      writer_transport_impl
        = TheTransportFactory->create_transport_impl (PUB_TRAFFIC,
                                                      "SimpleTcp",
                                                      TAO::DCPS::DONT_AUTO_CONFIG);
      TAO::DCPS::TransportConfiguration_rch writer_config
        = TheTransportFactory->create_configuration (PUB_TRAFFIC, "SimpleTcp");

      if (writer_transport_impl->configure(writer_config.in()) != 0)
        {
          ACE_ERROR((LM_ERROR,
                    ACE_TEXT("(%P|%t) init_transport: sub TCP")
                    ACE_TEXT(" Failed to configure the transport.\n")));
          status = 1;
        }

  return status;
}


int wait_for_data (::DDS::Subscriber_ptr sub,
                   int timeout_sec)
{
  const int factor = 10;
  ACE_Time_Value small(0,1000000/factor);
  int timeout_loops = timeout_sec * factor;

  ::DDS::DataReaderSeq_var discard = new ::DDS::DataReaderSeq(10);
  while (timeout_loops-- > 0)
    {
      sub->get_datareaders (
                    discard.out (),
                    ::DDS::NOT_READ_SAMPLE_STATE,
                    ::DDS::ANY_VIEW_STATE,
                    ::DDS::ANY_INSTANCE_STATE );
      if (discard->length () > 0)
        return 1;

      ACE_OS::sleep (small);
    }
  return 0;
}

/// parse the command line arguments
int parse_args (int argc, char *argv[])
{

  u_long mask =  ACE_LOG_MSG->priority_mask(ACE_Log_Msg::PROCESS) ;
  ACE_LOG_MSG->priority_mask(mask | LM_TRACE | LM_DEBUG, ACE_Log_Msg::PROCESS) ;
  ACE_Arg_Shifter arg_shifter (argc, argv);

  while (arg_shifter.is_anything_left ())
  {
    // options:
    //  -n max_samples_per_instance defaults to INFINITE
    //  -d history.depth            defaults to 1
    //  -z                          verbose transport debug
    //  -b                          enable client side Built-In topic support

    const char *currentArg = 0;

    if ((currentArg = arg_shifter.get_the_parameter("-n")) != 0)
    {
      max_samples_per_instance = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-d")) != 0)
    {
      history_depth = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-z") == 0)
    {
      TURN_ON_VERBOSE_DEBUG;
      arg_shifter.consume_arg();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-b") == 0)
    {
      support_client_side_BIT = true;
      arg_shifter.consume_arg();
    }
    else
    {
      arg_shifter.ignore_arg ();
    }
  }
  // Indicates sucessful parsing of the command line
  return 0;
}

void check_read_status(DDS::ReturnCode_t status,
                       const Test::SimpleSeq& data,
                       CORBA::ULong expected,
                       const char* where)
{

      if (status == ::DDS::RETCODE_OK)
      {
          if (data.length() != expected)
          {
              ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) %s ERROR: expected %d samples but got %d\n"),
                  where, expected, data.length() ));
              test_failed = 1;
              throw TestException();
          }

      }
      else if (status == ::DDS::RETCODE_NO_DATA)
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) %s ERROR: reader received NO_DATA!\n"), 
          where));
        test_failed = 1;
        throw TestException();
      }
      else if (status == ::DDS::RETCODE_PRECONDITION_NOT_MET)
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) %s ERROR: reader received PRECONDITION_NOT_MET!\n"), 
          where));
        test_failed = 1;
        throw TestException();
      }
      else
      {
        ACE_ERROR((LM_ERROR,
            ACE_TEXT("(%P|%t) %s ERROR: unexpected status %d!\n"),
            where, status ));
        test_failed = 1;
        throw TestException();
      }
}


void check_return_loan_status(DDS::ReturnCode_t status,
                              const Test::SimpleSeq& data,
                       CORBA::ULong expected_len,
                       CORBA::ULong expected_max,
                       const char* where)
{

      if (status == ::DDS::RETCODE_OK)
      {
          if (data.length() != expected_len)
          {
              ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) %s ERROR: expected %d len but got %d\n"),
                  where, expected_len, data.length() ));
              test_failed = 1;
              throw TestException();
          }
          if (data.max_len() != expected_max)
          {
              ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) %s ERROR: expected %d max_len but got %d\n"),
                  where, expected_max, data.max_len() ));
              test_failed = 1;
              throw TestException();
          }

      }
      else if (status == ::DDS::RETCODE_NO_DATA)
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) %s ERROR: reader received NO_DATA!\n"), 
          where));
        test_failed = 1;
        throw TestException();
      }
      else if (status == ::DDS::RETCODE_PRECONDITION_NOT_MET)
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) %s ERROR: reader received PRECONDITION_NOT_MET!\n"), 
          where));
        test_failed = 1;
        throw TestException();
      }
      else
      {
        ACE_ERROR((LM_ERROR,
            ACE_TEXT("(%P|%t) %s ERROR: unexpected status %d!\n"),
            where, status ));
        test_failed = 1;
        throw TestException();
      }
}

int main (int argc, char *argv[])
{


  u_long mask =  ACE_LOG_MSG->priority_mask(ACE_Log_Msg::PROCESS) ;
  ACE_LOG_MSG->priority_mask(mask | LM_TRACE | LM_DEBUG, ACE_Log_Msg::PROCESS) ;

  ACE_DEBUG((LM_DEBUG,"(%P|%t) zero-copy read test main\n"));
  try
    {
      ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);
      if (CORBA::is_nil (dpf.in ()))
      {
        ACE_ERROR ((LM_ERROR,
                   ACE_TEXT("(%P|%t) creating the DomainParticipantFactory failed.\n")));
        return 1 ;
      }

      // let the Service_Participant (in above line) strip out -DCPSxxx parameters
      // and then get application specific parameters.
      parse_args (argc, argv);

      Test::SimpleTypeSupport_var fts = new Test::SimpleTypeSupportImpl();

      ::DDS::DomainParticipant_var dp =
        dpf->create_participant(MY_DOMAIN,
                                PARTICIPANT_QOS_DEFAULT,
                                ::DDS::DomainParticipantListener::_nil());
      if (CORBA::is_nil (dp.in ()))
      {
        ACE_ERROR ((LM_ERROR,
                   ACE_TEXT("(%P|%t) create_participant failed.\n")));
        return 1 ;
      }

      if (::DDS::RETCODE_OK != fts->register_type(dp.in (), MY_TYPE))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to register the SimpleTypeSupport.")));
          return 1;
        }


      ::DDS::TopicQos topic_qos;
      dp->get_default_topic_qos(topic_qos);

      topic_qos.history.kind = ::DDS::KEEP_LAST_HISTORY_QOS;
      topic_qos.history.depth = history_depth;
      topic_qos.resource_limits.max_samples_per_instance =
            max_samples_per_instance ;


      ::DDS::Topic_var topic =
        dp->create_topic (MY_TOPIC,
                          MY_TYPE,
                          topic_qos,
                          ::DDS::TopicListener::_nil());
      if (CORBA::is_nil (topic.in ()))
      {
        return 1 ;
      }

      ::DDS::TopicDescription_var description =
        dp->lookup_topicdescription(MY_TOPIC);
      if (CORBA::is_nil (description.in ()))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT("(%P|%t) lookup_topicdescription failed.\n")),
                           1);
      }



      // Create the subscriber
      ::DDS::Subscriber_var sub =
        dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                             ::DDS::SubscriberListener::_nil());
      if (CORBA::is_nil (sub.in ()))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT("(%P|%t) create_subscriber failed.\n")),
                           1);
      }

      // Create the publisher
      ::DDS::Publisher_var pub =
        dp->create_publisher(PUBLISHER_QOS_DEFAULT,
                             ::DDS::PublisherListener::_nil());
      if (CORBA::is_nil (pub.in ()))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT("(%P|%t) create_publisher failed.\n")),
                          1);
      }

      // Initialize the transport
      if (0 != ::init_tranport() )
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT("(%P|%t) init_transport failed!\n")),
                           1);
      }

      // Attach the subscriber to the transport.
      TAO::DCPS::SubscriberImpl* sub_impl
        = TAO::DCPS::reference_to_servant<TAO::DCPS::SubscriberImpl>(sub.in());

      if (0 == sub_impl)
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT("(%P|%t) Failed to obtain servant ::TAO::DCPS::SubscriberImpl\n")),
                          1);
      }

      sub_impl->attach_transport(reader_transport_impl.in());


      // Attach the publisher to the transport.
      TAO::DCPS::PublisherImpl* pub_impl
        = TAO::DCPS::reference_to_servant<TAO::DCPS::PublisherImpl> (pub.in ());

      if (0 == pub_impl)
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT("(%P|%t) Failed to obtain servant ::TAO::DCPS::PublisherImpl\n")),
                          1);
      }

      pub_impl->attach_transport(writer_transport_impl.in());

      // Create the datawriter
      ::DDS::DataWriterQos dw_qos;
      pub->get_default_datawriter_qos (dw_qos);

      dw_qos.history.kind = ::DDS::KEEP_LAST_HISTORY_QOS;
      dw_qos.history.depth = history_depth  ;
      dw_qos.resource_limits.max_samples_per_instance =
            max_samples_per_instance ;

      ::DDS::DataWriter_var dw = pub->create_datawriter(topic.in (),
                                        dw_qos,
                                        ::DDS::DataWriterListener::_nil());

      if (CORBA::is_nil (dw.in ()))
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) create_datawriter failed.\n")));
        return 1 ;
      }

      // Create the Datareader
      ::DDS::DataReaderQos dr_qos;
      sub->get_default_datareader_qos (dr_qos);

      dr_qos.history.kind = ::DDS::KEEP_LAST_HISTORY_QOS;
      dr_qos.history.depth = history_depth  ;
      dr_qos.resource_limits.max_samples_per_instance =
            max_samples_per_instance ;

      ::DDS::DataReader_var dr
        = sub->create_datareader(description.in (),
                                 dr_qos,
                                 ::DDS::DataReaderListener::_nil());

      if (CORBA::is_nil (dr.in ()))
        {
          ACE_ERROR_RETURN ((LM_ERROR,
            ACE_TEXT("(%P|%t) create_datareader failed.\n")),
            1);
        }

        Test::SimpleDataWriter_var foo_dw
           = Test::SimpleDataWriter::_narrow(dw.in ());
      if (CORBA::is_nil (foo_dw.in ()))
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) Test::SimpleDataWriter::_narrow failed.\n")));
        return 1; // failure
      }

      Test::SimpleDataWriterImpl* fast_dw =
        TAO::DCPS::reference_to_servant<Test::SimpleDataWriterImpl>
        (foo_dw.in ());

      Test::SimpleDataReader_var foo_dr
        = Test::SimpleDataReader::_narrow(dr.in ());
      if (CORBA::is_nil (foo_dr.in ()))
      {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("(%P|%t) Test::SimpleDataReader::_narrow failed.\n")));
        return 1; // failure
      }

      Test::SimpleDataReaderImpl* fast_dr =
        TAO::DCPS::reference_to_servant<Test::SimpleDataReaderImpl>
        (foo_dr.in ());


      // wait for association establishement before writing.
      // -- replaced this sleep with the while loop below; 
      //    waiting on the one association we expect.
      //  ACE_OS::sleep(5); //REMOVE if not needed
      ::DDS::InstanceHandleSeq handles;
      while (1)
      {
          fast_dw->get_matched_subscriptions(handles);
          if (handles.length() > 0)
              break;
          else
              ACE_OS::sleep(ACE_Time_Value(0,200000));
      }

      // =============== do the test ====


      ::DDS::OfferedIncompatibleQosStatus * incomp =
          foo_dw->get_offered_incompatible_qos_status ();

      int incompatible_transport_found = 0;
      for (CORBA::ULong ii =0; ii < incomp->policies.length (); ii++)
        {
          if (incomp->policies[ii].policy_id
                        == ::DDS::TRANSPORTTYPE_QOS_POLICY_ID)
            incompatible_transport_found = 1;
        }

      ::DDS::SubscriptionMatchStatus matched =
        foo_dr->get_subscription_match_status ();

      ::DDS::InstanceHandle_t handle;

          if (matched.total_count != 1)
            ACE_ERROR_RETURN((LM_ERROR,
              "TEST ERROR: expected subscription_match"
              " with count 1 but got %d\n",
              matched.total_count),
              9);

    try { // the real testing.
      ::Test::Simple foo;
      ::Test::MyLongSeq ls;
      //::Test::Simple::_ls_seq ls;
      ls.length(1);
      ls[0] = 5;
      foo.key  = 1;
      foo.count = 1;
      foo.text = CORBA::string_dup("t1");
      foo.ls = ls;
      

      handle
          = fast_dw->_cxx_register (foo);

      fast_dw->write(foo,
                     handle);


      // wait for new data for upto 5 seconds
      if (!wait_for_data(sub.in (), 5))
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT("(%P|%t) ERROR: timeout waiting for data.\n")),
                           1);

      
      TAO::DCPS::ReceivedDataElement *item;
      {
        //=====================================================
        // 1) show that zero-copy is zero-copy
        //=====================================================
        ACE_DEBUG((LM_INFO,"==== TEST 1 : show that zero-copy is zero-copy\n"));
        const CORBA::Long max_samples = 2;
        // 0 means zero-copy
        Test::SimpleSeq            data1 (0, max_samples);
        ::DDS::SampleInfoSeq info1 (0,max_samples);

          
        DDS::ReturnCode_t status  ;
        status = fast_dr->read(  data1 
                                , info1
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data1, 1, "t1 read2");

        // this should change the value returned by the next read
        data1[0].count = 999;

        Test::SimpleSeq                  data2 (0, max_samples);
        ::DDS::SampleInfoSeq info2 (0,max_samples);
        status = fast_dr->read(  data2 
                                , info2
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data2, 1, "t1 read2");

        if (data1[0].count != data2[0].count)
        {
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t1 ERROR: zero-copy failed.\n") ));
            test_failed = 1;

        }

        item = data2.getPtr(0);

        // test the assignment operator.
        Test::SimpleSeq copy;
        ::DDS::SampleInfoSeq copyInfo;
        copy     = data2;
        copyInfo = info2;
        if (   copy.length() != data2.length() 
            || copy[0].count != data2[0].count
            || item->ref_count_ != 4)
          {
            ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) t1 ERROR: assignment operator failed\n") ));
            test_failed = 1;
          }
            
        status = fast_dr->return_loan(copy, copyInfo );

        check_return_loan_status(status, copy, 0, 0, "t1 return_loan copy");

        if (item->ref_count_ != 3)
        {
            ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) t1 ERROR: bad ref count %d expecting 3\n"), item->ref_count_ ));
            test_failed = 1;
        }


        status = fast_dr->return_loan(data2, info2 );

        check_return_loan_status(status, data2, 0, 0, "t1 return_loan2");

        if (item->ref_count_ != 2)
        {
            ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) t4 ERROR: bad ref count %d expecting 2\n"), item->ref_count_ ));
            test_failed = 1;
        }
        status = fast_dr->return_loan(data1, info1 );

        check_return_loan_status(status, data1, 0, 0, "t1 return_loan1");

        // just the instance container should have a reference.
        if (item->ref_count_ != 1)
        {
            ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) t4 ERROR: bad ref count %d expecting 1\n"), item->ref_count_ ));
            test_failed = 1;
        }
      } // t1

      {
        //=====================================================
        // 2) show that single-copy is makes copies
        //=====================================================
        ACE_DEBUG((LM_INFO,"==== TEST 2 : show that single-copy is makes copies\n"));
          
        const CORBA::Long max_samples = 2;
        // types supporting zero-copy read 
        Test::SimpleSeq                  data1 (max_samples);
        ::DDS::SampleInfoSeq info1 (max_samples);

        DDS::ReturnCode_t status  ;
        status = fast_dr->read(  data1 
                                , info1
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data1, 1, "t1 read2");

        // this should change the value returned by the next read
        data1[0].count = 888;
        data1[0].text = CORBA::string_dup("t2");

        Test::SimpleSeq                  data2 (max_samples);
        ::DDS::SampleInfoSeq info2 (max_samples);
        status = fast_dr->read(  data2 
                                , info2
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data2, 1, "t2 read2");

        if (data1[0].count == data2[0].count)
        {
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t2 ERROR: single-copy test failed for scalar.\n") ));
            test_failed = 1;

        }

        //ACE_DEBUG((LM_DEBUG,"%s != %s\n", data1[0].text.in(), data2[0].text.in() ));

        if (0 == strcmp(data1[0].text.in(), data2[0].text.in()))
        {
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t2 ERROR: single-copy test failed for string.\n") ));
            test_failed = 1;

        }

        // test the assignment operator.
        Test::SimpleSeq      copy (max_samples+1);
        ::DDS::SampleInfoSeq copyInfo (max_samples+1);
        copy     = data2;
        copyInfo = info2;
        if (   copy.length() != data2.length() 
            || copy[0].count != data2[0].count )
          {
            ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) t2 ERROR: assignment operator failed\n") ));
            test_failed = 1;
          }
            
        status = fast_dr->return_loan(copy, copyInfo );

        check_return_loan_status(status, copy, 1, max_samples, "t2 return_loan copy");

        status = fast_dr->return_loan(  data2 
                                      , info2 );

        check_return_loan_status(status, data2, 1, max_samples, "t2 return_loan2");

        status = fast_dr->return_loan(  data1 
                                      , info1 );

        check_return_loan_status(status, data1, 1, max_samples, "t2 return_loan1");

        // END OF BLOCK destruction.
        // 4/24/07 Note: breakpoint in the ls sequence destructor 
        // (part of Simple sample type) showed it was called when this 
        // block went out of scope (because the data1 and data2 are destroyed).
        // Good!
        // 4/24/07 Note: breakpoint in ACE_Array destructor showed 
        // it was called for the ZeroCopyInfoSeq. Good!

      } // t2

      {
        //=====================================================
        // 3) Show that zero-copy reference counting works.
        //    The zero-copy sequence will hold a reference
        //    while the instance container loses its
        //    reference because of history.depth.
        //=====================================================
        ACE_DEBUG((LM_INFO,"==== TEST 3 : Show that zero-copy reference counting works\n"));

        const CORBA::Long max_samples = 2;
        // 0 means zero-copy
        Test::SimpleSeq                  data1 (0, max_samples);
        ::DDS::SampleInfoSeq info1 (0,max_samples);

              
        foo.key  = 1;
        foo.count = 1;

        // since depth=1 the previous sample will be "lost"
        // from the instance container.
        fast_dw->write(foo,
                        handle);

        // wait for write to propogate
        if (!wait_for_data(sub.in (), 5))
            ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) t3 ERROR: timeout waiting for data.\n")),
                            1);

        DDS::ReturnCode_t status  ;
        status = fast_dr->read(  data1 
                                , info1
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data1, 1, "t3 read2");

        if (data1[0].count != 1)
        {
            // test to see the accessing the "lost" (because of history.depth)
            // but still held by zero-copy sequence value works.
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t3 ERROR: unexpected value for data1-pre.\n") ));
            test_failed = 1;

        }

        foo.key  = 1;
        foo.count = 2;

        // since depth=1 the previous sample will be "lost"
        // from the instance container.
        fast_dw->write(foo,
                        handle);

        // wait for write to propogate
        if (!wait_for_data(sub.in (), 5))
            ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) t3 ERROR: timeout waiting for data.\n")),
                            1);

        Test::SimpleSeq                  data2 (0, max_samples);
        ::DDS::SampleInfoSeq info2 (0,max_samples);

        status = fast_dr->read(  data2 
                                , info2
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data2, 1, "t3 read2");

        if (data1[0].count != 1)
        {
            // test to see the accessing the "lost" (because of history.depth)
            // but still held by zero-copy sequence value works.
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t3 ERROR: unexpected value for data1-post.\n") ));
            test_failed = 1;

        }

        if (data2[0].count != 2)
        {
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t3 ERROR: unexpected value for data2.\n") ));
            test_failed = 1;

        }
        status = fast_dr->return_loan(  data2 
                                        , info2 );

        check_return_loan_status(status, data2, 0, 0, "t3 return_loan2");

        // This return_loan will free the memory because the sample
        // has already been "lost" from the instance container.

        // 4/24/07 Note: breakpoint in the ls sequence destructor 
        // (part of Simple sample type) showed it was called when this 
        // block went out of scope. Good!
        // Note: the info sequence data is not destroyed/freed until
        //       the info1 object goes out of scope -- so it can 
        //       be reused without alloc & free.
        status = fast_dr->return_loan(  data1 
                                        , info1 );

        check_return_loan_status(status, data1, 0, 0, "t3 return_loan1");

      } // t3
      {
        //=====================================================
        // 4) show that the default is zero-copy read
        //    and automatic loan_return works.
        //=====================================================
        ACE_DEBUG((LM_INFO,"==== TEST 4 : show that the default is zero-copy read\n"));
        const CORBA::Long max_samples = 2;
        Test::SimpleSeq                  data1;
        ::DDS::SampleInfoSeq info1;

          
        DDS::ReturnCode_t status  ;
        status = fast_dr->read(  data1 
                                , info1
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data1, 1, "t4 read2");

        // this should change the value returned by the next read
        data1[0].count = 999;

        {
            Test::SimpleSeq                  data2;
            ::DDS::SampleInfoSeq info2;
            status = fast_dr->read(  data2 
                                    , info2
                                    , max_samples
                                    , ::DDS::ANY_SAMPLE_STATE
                                    , ::DDS::ANY_VIEW_STATE
                                    , ::DDS::ANY_INSTANCE_STATE );

              
            check_read_status(status, data2, 1, "t4 read2");

            if (data1[0].count != data2[0].count)
            {
                ACE_ERROR ((LM_ERROR,
                        ACE_TEXT("(%P|%t) t4 ERROR: zero-copy failed.\n") ));
                test_failed = 1;

            }

            item = data2.getPtr(0);
            if (item->ref_count_ != 3)
            {
                ACE_ERROR ((LM_ERROR,
                        ACE_TEXT("(%P|%t) t4 ERROR: bad ref count %d expecting 3\n"), item->ref_count_ ));
                test_failed = 1;
            }

        } // data2 goes out of scope here and automatically return_loan'd 
            if (item->ref_count_ != 2)
            {
                ACE_ERROR ((LM_ERROR,
                        ACE_TEXT("(%P|%t) t4 ERROR: bad ref count %d expecting 2\n"), item->ref_count_ ));
                test_failed = 1;
            }
      } // t4
      // just the instance container should have a reference.
        if (item->ref_count_ != 1)
        {
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t4 ERROR: bad ref count %d expecting 1\n"), item->ref_count_ ));
            test_failed = 1;
        }

      {
        //=====================================================
        // 5) show that return_loan and then read with same sequence works.
        //=====================================================
        ACE_DEBUG((LM_INFO,"==== TEST 5 : show that return_loan and then read with same sequence works.\n"));

        const CORBA::Long max_samples = 2;
        // 0 means zero-copy
        Test::SimpleSeq                  data1 (0, max_samples);
        ::DDS::SampleInfoSeq info1 (0,max_samples);
         
        foo.key  = 1;
        foo.count = 1;

        // since depth=1 the previous sample will be "lost"
        // from the instance container.
        fast_dw->write(foo, handle);

        // wait for write to propogate
        if (!wait_for_data(sub.in (), 5))
            ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) t5 ERROR: timeout waiting for data.\n")),
                            1);

        DDS::ReturnCode_t status  ;
        status = fast_dr->read(  data1 
                                , info1
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data1, 1, "t5 read2");

        if (data1[0].count != 1)
        {
            // test to see the accessing the "lost" (because of history.depth)
            // but still held by zero-copy sequence value works.
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t5 ERROR: unexpected value for data1-pre.\n") ));
            test_failed = 1;

        }

        foo.key  = 1;
        foo.count = 2;

        // since depth=1 the previous sample will be "lost"
        // from the instance container.
        fast_dw->write(foo, handle);

        // wait for write to propogate
        if (!wait_for_data(sub.in (), 5))
            ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT("(%P|%t) t5 ERROR: timeout waiting for data.\n")),
                            1);

        status = fast_dr->return_loan(  data1 
                                        , info1 );

        check_return_loan_status(status, data1, 0, 0, "t5 return_loan1");

        status = fast_dr->read(  data1 
                                , info1
                                , max_samples
                                , ::DDS::ANY_SAMPLE_STATE
                                , ::DDS::ANY_VIEW_STATE
                                , ::DDS::ANY_INSTANCE_STATE );

          
        check_read_status(status, data1, 1, "t5 read2");

        if (data1[0].count != 2)
        {
            ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) t5 ERROR: unexpected value for data2.\n") ));
            test_failed = 1;

        }

        status = fast_dr->return_loan(  data1 
                                        , info1 );

        check_return_loan_status(status, data1, 0, 0, "t5 return_loan1");

      } // t5
    }
  catch (const TestException&)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in main.cpp. ")));
      return 1;
    }
      //======== clean up ============
      // Clean up publisher objects
//      pub->delete_contained_entities() ;

      pub->delete_datawriter(dw.in ());
      dp->delete_publisher(pub.in ());


      //clean up subscriber objects
//      sub->delete_contained_entities() ;

      sub->delete_datareader(dr.in ());
      dp->delete_subscriber(sub.in ());

      // clean up common objects
      dp->delete_topic(topic.in ());
      dpf->delete_participant(dp.in ());

      TheTransportFactory->release();
      TheServiceParticipant->shutdown ();

    }
  catch (const TestException&)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in main.cpp. ")));
      return 1;
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in main.cpp:");
      return 1;
    }

  // Note: The TransportImpl reference SHOULD be deleted before exit from
  //       main if the concrete transport libraries are loaded dynamically.
  //       Otherwise cleanup after main() will encount access vilation.
  reader_transport_impl = 0;
  writer_transport_impl = 0;
  return test_failed;
}
