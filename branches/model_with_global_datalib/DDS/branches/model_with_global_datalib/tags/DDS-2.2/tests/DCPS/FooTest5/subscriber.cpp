// -*- C++ -*-
// ============================================================================
/**
 *  @file   subscriber.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#include "DataReaderListener.h"
#include "TestException.h"
// Include the Udp.h to make sure Initializer is created before the Service
// Configurator open service configure file.
#include "dds/DCPS/transport/udp/Udp.h"
#include "dds/DCPS/transport/multicast/Multicast.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "tests/DCPS/FooType5/FooDefTypeSupportImpl.h"
#include "dds/DCPS/transport/framework/EntryExit.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"

#include "dds/DCPS/BuiltInTopicUtils.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DdsDcpsSubscriptionC.h"

#include "ace/Arg_Shifter.h"
#include "ace/Default_Constants.h"

#include "common.h"
#include <sstream>


/// parse the command line arguments
int parse_args (int argc, ACE_TCHAR *argv[])
{
  u_long mask =  ACE_LOG_MSG->priority_mask(ACE_Log_Msg::PROCESS) ;
  ACE_LOG_MSG->priority_mask(mask | LM_TRACE | LM_DEBUG, ACE_Log_Msg::PROCESS) ;
  ACE_Arg_Shifter arg_shifter (argc, argv);

  while (arg_shifter.is_anything_left ())
  {
    // options:
    //  -t use_take?1:0             defaults to 0
    //  -r num_datareaders          defaults to 1
    //  -n max_samples_per_instance defaults to INFINITE
    //  -d history.depth            defaults to 1
    //  -s sub transport address    defaults to localhost:23456
    //  -u using_udp                defaults to 0 - using TCP
    //  -c using_multicast          defaults to 0 - using TCP
    //  -m num_instances_per_writer defaults to 1
    //  -i num_samples_per_instance defaults to 1
    //  -w num_datawriters          defaults to 1
    //  -z length of float sequence in data type   defaults to 10
    //  -y read operation interval                 defaults to 0
    //  -k data type has no key flag               defaults to 0 - has key
    //  -f mixed transport test flag               defaults to 0 - single transport test
    //  -o directory of synch files used to coordinate publisher and subscriber
    //                              defaults to current directory.
    //  -v                          verbose transport debug
    //  -b test_bit                 defaults to 0 - not testing BIT

    const ACE_TCHAR *currentArg = 0;

    if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-t"))) != 0)
    {
      use_take = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-r"))) != 0)
    {
      num_datareaders = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-n"))) != 0)
    {
      max_samples_per_instance = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-d"))) != 0)
    {
      history_depth = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-s"))) != 0)
    {
      reader_address_str = currentArg;
      reader_address_given = 1;
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-u"))) != 0)
    {
      using_udp = ACE_OS::atoi (currentArg);
      if (using_udp == 1)
      {
        ACE_DEBUG((LM_DEBUG, "Subscriber Using UDP transport.\n"));
      }
      arg_shifter.consume_arg();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-c"))) != 0)
    {
      using_multicast = ACE_OS::atoi (currentArg);
      if (using_multicast == 1)
      {
        ACE_DEBUG((LM_DEBUG, "Subscriber Using MULTICAST transport.\n"));
      }
      arg_shifter.consume_arg();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-m"))) != 0)
    {
      num_instances_per_writer = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-i"))) != 0)
    {
      num_samples_per_instance = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-w"))) != 0)
    {
      num_datawriters = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-z"))) != 0)
    {
      sequence_length = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-k"))) != 0)
    {
      no_key = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-y"))) != 0)
    {
      op_interval_ms = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-f"))) != 0)
    {
      mixed_trans = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-o"))) != 0)
    {
      synch_file_dir = currentArg;
      pub_ready_filename = synch_file_dir + pub_ready_filename;
      pub_finished_filename = synch_file_dir + pub_finished_filename;
      sub_ready_filename = synch_file_dir + sub_ready_filename;
      sub_finished_filename = synch_file_dir + sub_finished_filename;

      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp(ACE_TEXT("-v")) == 0)
    {
      TURN_ON_VERBOSE_DEBUG;
      arg_shifter.consume_arg();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-b"))) != 0)
    {
      test_bit = ACE_OS::atoi (currentArg);
      if (test_bit == 1)
      {
        ACE_DEBUG((LM_DEBUG, "Subscriber testing BIT transport.\n"));
      }
      arg_shifter.consume_arg();
    }
    else
    {
      arg_shifter.ignore_arg ();
    }
  }

  if ((using_udp != 0 || mixed_trans != 0) && using_multicast != 0)
  {
    using_multicast = 0;
    ACE_DEBUG((LM_DEBUG, "Subscriber NOT using MULTICAST transport.\n"));
  }

  // Indicates sucessful parsing of the command line
  return 0;
}


::DDS::Subscriber_ptr
create_subscriber (::DDS::DomainParticipant_ptr participant,
                   int                          attach_to_udp,
                   int                          attach_to_multicast)
{

  // Create the subscriber
  ::DDS::Subscriber_var sub;

  try
    {
      sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                             ::DDS::SubscriberListener::_nil(),
                             ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (sub.in ()))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to create_subscriber.")));
          return ::DDS::Subscriber::_nil ();
        }

      // Attach the subscriber to the transport.
      OpenDDS::DCPS::SubscriberImpl* sub_impl
        = dynamic_cast<OpenDDS::DCPS::SubscriberImpl*> (sub.in ());

      if (0 == sub_impl)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) Failed to obtain subscriber servant\n")));
          return ::DDS::Subscriber::_nil ();
        }

      OpenDDS::DCPS::AttachStatus attach_status;

      if (attach_to_udp)
        {
          ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) attach to udp \n")));
          attach_status = sub_impl->attach_transport(reader_udp_impl.in());
        }
      else if (attach_to_multicast)
        {
          ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) attach to multicast \n")));
          attach_status = sub_impl->attach_transport(reader_multicast_impl.in());
        }
      else
        {
          ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) attach to tcp \n")));
          attach_status = sub_impl->attach_transport(reader_tcp_impl.in());
        }

      if (attach_status != OpenDDS::DCPS::ATTACH_OK)
        {
          // We failed to attach to the transport for some reason.
          ACE_TString status_str;

          switch (attach_status)
            {
              case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
                status_str = ACE_TEXT("ATTACH_BAD_TRANSPORT");
                break;
              case OpenDDS::DCPS::ATTACH_ERROR:
                status_str = ACE_TEXT("ATTACH_ERROR");
                break;
              case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
                status_str = ACE_TEXT("ATTACH_INCOMPATIBLE_QOS");
                break;
              default:
                status_str = ACE_TEXT("Unknown Status");
                break;
            }

          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) Failed to attach to the transport. ")
                      ACE_TEXT("AttachStatus == %s\n"),
                      status_str.c_str()));
          return ::DDS::Subscriber::_nil ();
        }
    }
  catch (const TestException&)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in create_subscriber. ")));
      return ::DDS::Subscriber::_nil ();
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in create_subscriber.");
      return ::DDS::Subscriber::_nil ();
    }

  return sub._retn ();
}


int ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{

  ::DDS::DomainParticipantFactory_var dpf;
  ::DDS::DomainParticipant_var participant;

  int status = 0;

  try
    {
      ACE_DEBUG((LM_INFO,"(%P|%t) %T subscriber main\n"));

      dpf = TheParticipantFactoryWithArgs(argc, argv);

      // let the Service_Participant (in above line) strip out -DCPSxxx parameters
      // and then get application specific parameters.
      parse_args (argc, argv);

      results.init ();

      participant =
        dpf->create_participant(MY_DOMAIN,
                                PARTICIPANT_QOS_DEFAULT,
                                ::DDS::DomainParticipantListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (participant.in ()))
        {
          ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) create_participant failed.\n")));
          return 1 ;
        }

      if (no_key)
        {
          ::Xyz::FooNoKeyTypeSupportImpl* nokey_fts_servant
            = new ::Xyz::FooNoKeyTypeSupportImpl();
          OpenDDS::DCPS::LocalObject_var safe_servant = nokey_fts_servant;

          if (::DDS::RETCODE_OK != nokey_fts_servant->register_type(participant.in (), MY_TYPE))
            {
              ACE_ERROR ((LM_ERROR,
                ACE_TEXT ("Failed to register the FooTypeSupport.")));
              throw TestException ();
            }
        }
      else
        {
          ::Xyz::FooTypeSupportImpl* fts_servant
            = new ::Xyz::FooTypeSupportImpl();
          //x ortableServer::ServantBase_var safe_servant = fts_servant;

          if (::DDS::RETCODE_OK != fts_servant->register_type(participant.in (), MY_TYPE))
            {
              ACE_ERROR ((LM_ERROR,
                ACE_TEXT ("Failed to register the FooNoTypeTypeSupport.")));
              throw TestException ();
            }
        }

      if (mixed_trans)
        {
          ::Xyz::FooTypeSupportImpl* fts_servant
            = new ::Xyz::FooTypeSupportImpl();
          OpenDDS::DCPS::LocalObject_var safe_servant = fts_servant;

          if (::DDS::RETCODE_OK != fts_servant->register_type(participant.in (), MY_TYPE_FOR_UDP))
            {
              ACE_ERROR ((LM_ERROR, ACE_TEXT("(%P|%t) register_type failed.\n")));
              throw TestException ();
            }
        }

      ::DDS::TopicQos topic_qos;
      participant->get_default_topic_qos(topic_qos);

      ::DDS::Topic_var topic
        = participant->create_topic(MY_TOPIC,
                                    MY_TYPE,
                                    topic_qos,
                                    ::DDS::TopicListener::_nil(),
                                    ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (topic.in ()))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to create_topic.")));
          throw TestException ();
        }

      ::DDS::TopicDescription_var description
        = participant->lookup_topicdescription(MY_TOPIC);
      if (CORBA::is_nil (description.in ()))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to lookup_topicdescription.")));
          throw TestException ();
        }

      ::DDS::Topic_var topic1;
      ::DDS::TopicDescription_var description1;

      if (mixed_trans)
        {
          topic1 = participant->create_topic (MY_TOPIC_FOR_UDP,
                                              MY_TYPE_FOR_UDP,
                                              topic_qos,
                                              ::DDS::TopicListener::_nil(),
                                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
          if (CORBA::is_nil (topic1.in ()))
            {
              ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) create_topic failed.\n")));
              throw TestException ();
            }

          description1
            = participant->lookup_topicdescription(MY_TOPIC_FOR_UDP);
          if (CORBA::is_nil (description1.in ()))
            {
              ACE_ERROR ((LM_ERROR,
                ACE_TEXT ("Failed to lookup_topicdescription.")));
              throw TestException ();
            }
        }

      // Initialize the transport
      if (0 != ::init_reader_transport() )
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to init_reader_transport.")));
          throw TestException ();
        }

      int attach_to_udp = using_udp;
      int attach_to_multicast = using_multicast;

      // Create the subscriber and attach to the corresponding
      // transport.
      ::DDS::Subscriber_var sub
        = create_subscriber(participant.in (), attach_to_udp, attach_to_multicast);
      if (CORBA::is_nil (sub.in ()))
        {
          ACE_ERROR ((LM_ERROR,
            ACE_TEXT ("Failed to create_subscriber.")));
          throw TestException ();
        }

      ::DDS::Subscriber_var sub1;
      if (mixed_trans)
        {
          // Create the subscriber with a different transport from previous
          // subscriber.
          sub1 = create_subscriber(participant.in (), ! attach_to_udp, attach_to_multicast);
          if (CORBA::is_nil (sub1.in ()))
            {
              ACE_ERROR ((LM_ERROR,
                ACE_TEXT ("Failed to create_subscriber.")));
              throw TestException ();
            }
        }

      // Create the Datareaders
      ::DDS::DataReaderQos dr_qos;
      sub->get_default_datareader_qos (dr_qos);

      // Make it KEEP_ALL history so we can verify the received
      // data without dropping.
      dr_qos.history.kind = ::DDS::KEEP_ALL_HISTORY_QOS;
      dr_qos.reliability.kind = ::DDS::RELIABLE_RELIABILITY_QOS;
      dr_qos.resource_limits.max_samples_per_instance =
        max_samples_per_instance ;
      // The history depth is only used for KEEP_LAST.
      //dr_qos.history.depth = history_depth  ;

      // activate the listener
      ::DDS::DataReaderListener_var listener (new DataReaderListenerImpl);

      if (CORBA::is_nil (listener.in ()))
        {
          ACE_ERROR ((LM_ERROR, ACE_TEXT ("(%P|%t) listener is nil.")));
          throw TestException ();
        }

#if !defined (DDS_HAS_MINIMUM_BIT)
      // Test that the BIT objects can be obtained on the tcp transport
      if (test_bit != 0)
        {
          ::DDS::Subscriber_var bit_subscriber = participant->get_builtin_subscriber () ;
          if (CORBA::is_nil (bit_subscriber.in ()))
            ACE_ERROR((LM_ERROR, ACE_TEXT("Error: Failed to get BIT subscriber\n")));
          else
            {
              ::DDS::DataReader_var bit_dr = bit_subscriber->lookup_datareader (OpenDDS::DCPS::BUILT_IN_TOPIC_TOPIC);
              if(CORBA::is_nil (bit_dr.in ()))
                  ACE_ERROR((LM_ERROR, ACE_TEXT("Error: Failed to lookup BIT Topic Data Reader\n")));
              else
                {
                  ::DDS::TopicBuiltinTopicDataDataReader_var bit_topic_dr
                      = ::DDS::TopicBuiltinTopicDataDataReader::_narrow (bit_dr.in ());
                  if (CORBA::is_nil (bit_topic_dr.in ()))
                      ACE_ERROR((LM_ERROR, ACE_TEXT("Error: Failed to narrow BIT Topic Data Reader\n")));
                }
            }
        }
#endif // !defined (DDS_HAS_MINIMUM_BIT)

      ::DDS::DataReader_var * drs = new ::DDS::DataReader_var[num_datareaders];

      // Create one datareader or multiple datareaders belonging to the same
      // subscriber.
      for (int i = 0; i < num_datareaders; i ++)
        {
          int attach_to_udp = using_udp;
          ::DDS::Subscriber_var the_sub = sub;
          ::DDS::TopicDescription_var the_description = description;
          // The first datawriter would be using a different transport
          // from others for the diff trans test case.
          if (mixed_trans && i == 0)
            {
              attach_to_udp = ! attach_to_udp;
              the_sub = sub1;
              the_description = description1;
            }

          // create the datareader.
          drs[i] = the_sub->create_datareader(the_description.in (),
                                              dr_qos,
                                              listener.in (),
                                              ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

          if (CORBA::is_nil (drs[i].in ()))
            {
              ACE_ERROR_RETURN ((LM_ERROR,
                              ACE_TEXT("(%P|%t) create_datareader failed.\n")),
                              1);
            }
        }

      // Indicate that the subscriber is ready
      FILE* readers_ready = ACE_OS::fopen (sub_ready_filename.c_str (), ACE_TEXT("w"));
      if (readers_ready == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR Unable to create subscriber completed file\n")));
        }

      // Wait for the publisher to be ready
      FILE* writers_ready = 0;
      do
        {
          ACE_Time_Value small_time(0,250000);
          ACE_OS::sleep (small_time);
          writers_ready = ACE_OS::fopen (pub_ready_filename.c_str (), ACE_TEXT("r"));
        } while (0 == writers_ready);

      ACE_OS::fclose(readers_ready);
      ACE_OS::fclose(writers_ready);

      int expected
        = num_datawriters * num_instances_per_writer * num_samples_per_instance;

      FILE* writers_completed = 0;
      int timeout_writes = 0;

      while ( num_reads < expected)
        {
          // Get the number of the timed out writes from publisher so we
          // can re-calculate the number of expected messages. Otherwise,
          // the blocking timeout test will never exit from this loop.
          if (writers_completed == 0)
            {
              writers_completed = ACE_OS::fopen (pub_finished_filename.c_str (), ACE_TEXT("r"));
              if (writers_completed != 0)
                {
                  //writers_completed = ACE_OS::fopen (pub_finished_filename.c_str (), ACE_TEXT("r"));
                  fscanf (writers_completed, "%d\n", &timeout_writes);
                  // After we got the number of timed out writes, we should speed the
                  // receiving.
                  op_interval_ms = 0;
                  expected -= timeout_writes;
                  ACE_DEBUG((LM_DEBUG,
                             ACE_TEXT ("(%P|%t) timed out writes %d, we expect %d\n"),
                             timeout_writes, expected));
                }

            }
          ACE_OS::sleep (1);
        }

      // Indicate that the subscriber is done
      FILE* readers_completed = ACE_OS::fopen (sub_finished_filename.c_str (), ACE_TEXT("w"));
      if (readers_completed == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR Unable to create subscriber completed file\n")));
        }

      // Wait for the publisher to finish
      while (writers_completed == 0)
        {
          ACE_Time_Value small_time(0,250000);
          ACE_OS::sleep (small_time);
          writers_completed = ACE_OS::fopen (pub_finished_filename.c_str (), ACE_TEXT("r"));
        }

      ACE_OS::fclose(readers_completed);
      ACE_OS::fclose(writers_completed);

      if (results.test_passed (expected) == false)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) Verify received samples - not passed \n")));
          status = 1;
        }

      delete [] drs;
    }
  catch (const TestException&)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in main (). ")));
      status = 1;
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in main ():");
      status = 1;
    }

  try
    {
      if (! CORBA::is_nil (participant.in ()))
        {
          participant->delete_contained_entities();
        }
      if (! CORBA::is_nil (dpf.in ()))
        {
          dpf->delete_participant(participant.in ());
        }
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in cleanup.");
      status = 1;
    }

  TheTransportFactory->release();
  TheServiceParticipant->shutdown ();
  // Note: The TransportImpl reference SHOULD be deleted before exit from
  //       main if the concrete transport libraries are loaded dynamically.
  //       Otherwise cleanup after main() will encount access vilation.
  reader_tcp_impl = 0;
  reader_udp_impl = 0;
  reader_multicast_impl = 0;
  return status;
}
