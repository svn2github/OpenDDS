// -*- C++ -*-
// ============================================================================
/**
 *  @file   publisher.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#include "common.h"
#include "Writer.h"
#include "TestException.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/PublisherImpl.h"
#include "tests/DCPS/FooType5/FooTypeSupportImpl.h"
#include "tests/DCPS/FooType5/FooNoKeyTypeSupportImpl.h"
#include "dds/DCPS/transport/framework/EntryExit.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"

#include "ace/Arg_Shifter.h"




#include "common.h"

/// parse the command line arguments
int parse_args (int argc, char *argv[])
{
  u_long mask =  ACE_LOG_MSG->priority_mask(ACE_Log_Msg::PROCESS) ;
  ACE_LOG_MSG->priority_mask(mask | LM_TRACE | LM_DEBUG, ACE_Log_Msg::PROCESS) ;
  ACE_Arg_Shifter arg_shifter (argc, argv);
  
  while (arg_shifter.is_anything_left ()) 
  {
    // options:
    //  -i num_samples_per_instance    defaults to 1 
    //  -w num_datawriters          defaults to 1 
    //  -m num_instances_per_writer defaults to 1
    //  -n max_samples_per_instance defaults to INFINITE
    //  -d history.depth            defaults to 1
    //  -p pub transport address    defaults to localhost:23456
    //  -u using udp flag           defaults to 0 - using TCP
    //  -z length of float sequence in data type   defaults to 10
    //  -y write operation interval                defaults to 0
    //  -b blocking timeout in milliseconds        defaults to 0
    //  -k data type has no key flag               defaults to 0 - has key
    //  -f mixed transport test flag               defaults to 0 - single transport test
    //  -o directory of synch files used to coordinate publisher and subscriber
    //                              defaults to current directory.
    //  -v                          verbose transport debug

    const char *currentArg = 0;
    
    if ((currentArg = arg_shifter.get_the_parameter("-m")) != 0) 
    {
      num_instances_per_writer = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-i")) != 0) 
    {
      num_samples_per_instance = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-w")) != 0) 
    {
      num_datawriters = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-n")) != 0) 
    {
      max_samples_per_instance = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-d")) != 0) 
    {
      history_depth = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-p")) != 0) 
    {
      writer_address_str = currentArg;
      writer_address_given = 1;
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-u")) != 0)
    {
      using_udp = ACE_OS::atoi (currentArg);
      if (using_udp == 1)
      {
        ACE_DEBUG((LM_DEBUG, "Publisher Using UDP transport.\n"));
      }
      arg_shifter.consume_arg();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-z")) != 0) 
    {
      sequence_length = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-y")) != 0) 
    {
      op_interval_ms = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-b")) != 0) 
    {
      blocking_ms = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-k")) != 0) 
    {
      no_key = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-f")) != 0) 
    {
      mixed_trans = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter("-o")) != 0)
    {
      synch_file_dir = currentArg;
      pub_ready_filename = synch_file_dir + pub_ready_filename;
      pub_finished_filename = synch_file_dir + pub_finished_filename;
      sub_ready_filename = synch_file_dir + sub_ready_filename;
      sub_finished_filename = synch_file_dir + sub_finished_filename;

      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-v") == 0)
    {
      TURN_ON_VERBOSE_DEBUG;
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


::DDS::Publisher_ptr
create_publisher (::DDS::DomainParticipant_ptr participant,
                  int                          attach_to_udp)
{  
  ::DDS::Publisher_var pub;

  ACE_TRY_NEW_ENV
    {

      // Create the default publisher
      pub = participant->create_publisher(PUBLISHER_QOS_DEFAULT,
                                    ::DDS::PublisherListener::_nil()
                                    ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
      if (CORBA::is_nil (pub.in ()))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) create_publisher failed.\n")));
          return ::DDS::Publisher::_nil ();
        }

      // Attach the publisher to the transport.
      ::TAO::DCPS::PublisherImpl* pub_impl 
        = reference_to_servant< ::TAO::DCPS::PublisherImpl,
                                ::DDS::Publisher_ptr>
                              (pub ACE_ENV_SINGLE_ARG_PARAMETER);
        ACE_TRY_CHECK;

      if (0 == pub_impl)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) Failed to obtain publisher servant \n")));
          return ::DDS::Publisher::_nil ();
        }

      TAO::DCPS::AttachStatus attach_status;

      if (attach_to_udp)
        {
          ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) attach to udp \n")));
          attach_status = pub_impl->attach_transport(writer_udp_impl.in());
        }
      else
        {
          ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) attach to tcp \n")));
          attach_status = pub_impl->attach_transport(writer_tcp_impl.in());
        }

      if (attach_status != TAO::DCPS::ATTACH_OK)
        {
          // We failed to attach to the transport for some reason.
          std::string status_str;

          switch (attach_status)
            {
              case TAO::DCPS::ATTACH_BAD_TRANSPORT:
                status_str = "ATTACH_BAD_TRANSPORT";
                break;
              case TAO::DCPS::ATTACH_ERROR:
                status_str = "ATTACH_ERROR";
                break;
              case TAO::DCPS::ATTACH_INCOMPATIBLE_QOS:
                status_str = "ATTACH_INCOMPATIBLE_QOS";
                break;
              default:
                status_str = "Unknown Status";
                break;
            }

          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) Failed to attach to the transport. ")
                      ACE_TEXT("AttachStatus == %s\n"),
                      status_str.c_str()));
          return ::DDS::Publisher::_nil ();
        }
    }
  ACE_CATCH (TestException,ex)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in create_publisher(). ")));
      return ::DDS::Publisher::_nil () ;
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                           "Exception caught in create_publisher().");
      return ::DDS::Publisher::_nil () ;
    }
  ACE_ENDTRY;
  return pub._retn ();
}


int main (int argc, char *argv[])
{
  ::DDS::DomainParticipantFactory_var dpf;
  Writer* writers = 0;
  ::DDS::DomainParticipant_var participant;

  int status = 0;

  ACE_TRY_NEW_ENV
    {
      ACE_DEBUG((LM_INFO,"(%P|%t) %T publisher main\n"));

      dpf = TheParticipantFactoryWithArgs(argc, argv);
      ACE_TRY_CHECK;

      // let the Service_Participant (in above line) strip out -DCPSxxx parameters
      // and then get application specific parameters.
      parse_args (argc, argv);

      participant
        = dpf->create_participant(MY_DOMAIN, 
                                  PARTICIPANT_QOS_DEFAULT, 
                                  ::DDS::DomainParticipantListener::_nil() 
                                  ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
      if (CORBA::is_nil (participant.in ()))
        {
          ACE_ERROR ((LM_ERROR,
                    ACE_TEXT("(%P|%t) create_participant failed.\n")));
          ACE_THROW(TestException ());
        }

      if (no_key)
        {
          ::Mine::FooNoKeyTypeSupportImpl* nokey_fts_servant 
            = new ::Mine::FooNoKeyTypeSupportImpl();
          if (::DDS::RETCODE_OK != nokey_fts_servant->register_type(participant.in (), MY_TYPE))
            {
              ACE_ERROR ((LM_ERROR, ACE_TEXT("(%P|%t) register_type failed.\n")));
              ACE_THROW (TestException ());
            }
        }
      else 
        {
          ::Mine::FooTypeSupportImpl* fts_servant 
            = new ::Mine::FooTypeSupportImpl();
          if (::DDS::RETCODE_OK != fts_servant->register_type(participant.in (), MY_TYPE))
            {
              ACE_ERROR ((LM_ERROR, ACE_TEXT("(%P|%t) register_type failed.\n")));
              ACE_THROW (TestException ());
            }
        }

      if (mixed_trans)
        {
          ::Mine::FooTypeSupportImpl* fts_servant 
            = new ::Mine::FooTypeSupportImpl();
          if (::DDS::RETCODE_OK != fts_servant->register_type(participant.in (), MY_TYPE_FOR_UDP))
            {
              ACE_ERROR ((LM_ERROR, ACE_TEXT("(%P|%t) register_type failed.\n")));
              ACE_THROW (TestException ());
            }
        }

      ::DDS::TopicQos topic_qos;
      participant->get_default_topic_qos(topic_qos);
      
      ::DDS::Topic_var topic 
        = participant->create_topic (MY_TOPIC, 
                                     MY_TYPE, 
                                     topic_qos, 
                                     ::DDS::TopicListener::_nil()
                                     ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
      if (CORBA::is_nil (topic.in ()))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) create_topic failed.\n")));
          ACE_THROW (TestException ());
        }

      ::DDS::Topic_var topic1;
      if (mixed_trans)
        {
          topic1 = participant->create_topic (MY_TOPIC_FOR_UDP, 
                                              MY_TYPE_FOR_UDP, 
                                              topic_qos, 
                                              ::DDS::TopicListener::_nil()
                                              ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;
          if (CORBA::is_nil (topic1.in ()))
            {
              ACE_ERROR ((LM_ERROR,
                ACE_TEXT("(%P|%t) create_topic failed.\n")));
              ACE_THROW (TestException ());
            }
        }
      // Initialize the transport
      if (0 != ::init_writer_tranport() )
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) init_writer_tranport failed.\n")));
          ACE_THROW (TestException ());
        }
      
      int attach_to_udp = using_udp;
      // Create the default publisher
      ::DDS::Publisher_var pub = create_publisher(participant, attach_to_udp);

      if (CORBA::is_nil (pub.in ()))
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) create_publisher failed.\n")));
          ACE_THROW (TestException ());
        }

      ::DDS::Publisher_var pub1;
      if (mixed_trans)
        {
          // Create another publisher for a difference transport.
          pub1 = create_publisher(participant, ! attach_to_udp);

          if (CORBA::is_nil (pub1.in ()))
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT("(%P|%t) create_publisher failed.\n")));
              ACE_THROW (TestException ());
            }
        }

      // Create the datawriters
      ::DDS::DataWriterQos dw_qos;
      pub->get_default_datawriter_qos (dw_qos);

      // Make it KEEP_ALL history so we can verify the received  
      // data without dropping.
      dw_qos.history.kind = ::DDS::KEEP_ALL_HISTORY_QOS;
      dw_qos.reliability.kind = ::DDS::RELIABLE_RELIABILITY_QOS;
      dw_qos.resource_limits.max_samples_per_instance =
          max_samples_per_instance ;
      dw_qos.reliability.max_blocking_time.sec = blocking_ms/1000;
      dw_qos.reliability.max_blocking_time.nanosec = blocking_ms%1000 * 1000000;
      // The history depth is only used for KEEP_LAST.
      //dw_qos.history.depth = history_depth  ;  

      ::DDS::DataWriter_var* dw = new ::DDS::DataWriter_var[num_datawriters];
      Writer** writers = new Writer*[num_datawriters];

      for (int i = 0; i < num_datawriters; i ++)
        {
          int attach_to_udp = using_udp;
          ::DDS::Publisher_var the_pub = pub;
          ::DDS::Topic_var the_topic = topic;
          // The first datawriter would be using a different transport
          // from others for the diff trans test case.
          if (mixed_trans && i == 0)
            {
              attach_to_udp = ! attach_to_udp;
              the_pub = pub1;
              the_topic = topic1; 
            }
          dw[i] = the_pub->create_datawriter(the_topic.in (),
                                             dw_qos,
                                             ::DDS::DataWriterListener::_nil()
                                             ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;

          if (CORBA::is_nil (dw[i].in ()))
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT("(%P|%t) create_datawriter failed.\n")));
              ACE_THROW (TestException ());
            }

          writers[i] = new Writer (dw[i].in (), i);
        } 
  
      // Indicate that the publisher is ready
      FILE* writers_ready = ACE_OS::fopen (pub_ready_filename.c_str (), ACE_LIB_TEXT("w"));
      if (writers_ready == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR Unable to create publisher ready file\n")));
        }

      // Wait for the subscriber to be ready.
      FILE* readers_ready = 0;
      do
        {
          ACE_Time_Value small(0,250000);
          ACE_OS::sleep (small);
          readers_ready = ACE_OS::fopen (sub_ready_filename.c_str (), ACE_LIB_TEXT("r"));
        } while (0 == readers_ready);

      ACE_OS::fclose(writers_ready);
      ACE_OS::fclose(readers_ready);

      // ensure the associations are fully established before writing.
      ACE_OS::sleep(3);

      {  // Extra scope for VC6
        for (int i = 0; i < num_datawriters; i ++)
          {
            writers[i]->start ();
          }
      }
  
      int timeout_writes = 0;
      bool writers_finished = false;

      while ( !writers_finished )
        {
          writers_finished = true;
          for (int i = 0; i < num_datawriters; i ++)
            {
              writers_finished = writers_finished && writers[i]->is_finished();
            }
        }

      {  // Extra scope for VC6
        for (int i = 0; i < num_datawriters; i ++)
          {
            timeout_writes += writers[i]->get_timeout_writes();
          }
      }
      // Indicate that the publisher is done
      FILE* writers_completed = ACE_OS::fopen (pub_finished_filename.c_str (), ACE_LIB_TEXT("w"));
      if (writers_completed == 0)
        {
          ACE_ERROR ((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR Unable to i publisher completed file\n")));
        }
      else
        {
          ACE_OS::fprintf (writers_completed, "%d\n", timeout_writes);
        }
      ACE_OS::fclose (writers_completed);

      // Wait for the subscriber to finish.
      FILE* readers_completed = 0;
      do
        {
          ACE_Time_Value small(0,250000);
          ACE_OS::sleep (small);
          readers_completed = ACE_OS::fopen (sub_finished_filename.c_str (), ACE_LIB_TEXT("r"));
        } while (0 == readers_completed);

      ACE_OS::fclose(writers_completed);
      ACE_OS::fclose(readers_completed);

      {  // Extra scope for VC6
        for (int i = 0; i < num_datawriters; i ++)
          {
            writers[i]->end ();
          }
      }
    }
  ACE_CATCH (TestException,ex)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("(%P|%t) TestException caught in main (). ")));
      status = 1;
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                           "Exception caught in main ():");
      status = 1;
    }
  ACE_ENDTRY;

  if (writers != 0)
    {
      delete [] writers;
    }

  ACE_TRY_NEW_ENV
    {
      if (! CORBA::is_nil (participant.in ()))
        {
          participant->delete_contained_entities(ACE_ENV_SINGLE_ARG_PARAMETER);
          ACE_TRY_CHECK;
        }

      if (! CORBA::is_nil (dpf.in ()))
        {
          dpf->delete_participant(participant.in () ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;
        }
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                           "Exception caught in cleanup.");
      status = 1;
    }
  ACE_ENDTRY;

  TheTransportFactory->release();
  TheServiceParticipant->shutdown (); 

  return status;
}




