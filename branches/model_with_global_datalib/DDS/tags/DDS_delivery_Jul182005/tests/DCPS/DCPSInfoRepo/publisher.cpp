#include  "dds/DdsDcpsInfoC.h"
#include  "DCPSDataWriterI.h"

#include "ace/Arg_Shifter.h"


const char *ior = "file://dcps_ir.ior";
bool ignore_entities = false;
bool qos_tests = false;

int
parse_args (int argc, char *argv[])
{
  ACE_Arg_Shifter arg_shifter (argc, argv);
  
  while (arg_shifter.is_anything_left ()) 
  {
    const char *currentArg = 0;
    
    if ((currentArg = arg_shifter.get_the_parameter("-k")) != 0) 
    {
      ior = currentArg;
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-i") == 0) 
    {
      ignore_entities = true;
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-q") == 0) 
    {
      qos_tests = true;
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-?") == 0) 
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                          "usage:  %s "
                          "-k <ior> "
                          "\n"
                          "        -i (ignore tests)\n"
                          "        -q (incompatible qos test)\n"
                          "        -? (usage message)",
                          argv [0]),
                        -1);
    }
    else 
    {
      arg_shifter.ignore_arg ();
    }
  }
  // Indicates sucessful parsing of the command line
  return 0;
}


int
main (int argc, char *argv[])
{
  if (parse_args (argc, argv) != 0)
    return 1;

  ACE_TRY_NEW_ENV
    {
      CORBA::ORB_var orb =
        CORBA::ORB_init (argc, argv, "" ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      //Get reference to the RootPOA.
      CORBA::Object_var obj = orb->resolve_initial_references( "RootPOA" );
      PortableServer::POA_var poa = PortableServer::POA::_narrow( obj.in() );

      // Activate the POAManager.
      PortableServer::POAManager_var mgr = poa->the_POAManager();
      mgr->activate();

      CORBA::Object_var tmp =
        orb->string_to_object (ior ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      TAO::DCPS::DCPSInfo_var info =
        TAO::DCPS::DCPSInfo::_narrow (tmp.in () ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      if (CORBA::is_nil (info.in ()))
        {
          ACE_ERROR_RETURN ((LM_DEBUG,
                             "Nil TAO::DCPS::DCPSInfo reference <%s>\n",
                             ior),
                            1);
        }


      // check adding a participant
      ::DDS::DomainParticipantQos_var dpQos = new ::DDS::DomainParticipantQos;
      CORBA::Long domainId = 911;

      CORBA::Long dpId = info->add_domain_participant(domainId, dpQos.in());
      ACE_TRY_CHECK;
      if (0 == dpId)
        {
          ACE_ERROR((LM_ERROR, ACE_TEXT("add_domain_participant failed!\n") ));
        }

      // add a topic
      CORBA::Long topicId;
      CORBA::String_var tname = "MYtopic";
      CORBA::String_var dname = "MYdataname";
      ::DDS::TopicQos_var topicQos = new ::DDS::TopicQos;
      TAO::DCPS::TopicStatus topicStatus = info->assert_topic(topicId,
                                                           domainId,
                                                           dpId,
                                                           tname.in(),
                                                           dname.in(),
                                                           topicQos.in()
                                                           ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      if (topicStatus != TAO::DCPS::CREATED)
        {
          ACE_ERROR((LM_ERROR, ACE_TEXT("Topic creation failed and returned %d"), topicStatus));
        }

      // Add publication
      TAO_DDS_DCPSDataWriter_i dwi;
      PortableServer::ObjectId_var oid = poa->activate_object( &dwi );
      obj = poa->id_to_reference( oid.in() );
      TAO::DCPS::DataWriterRemote_var dw = TAO::DCPS::DataWriterRemote::_narrow(obj.in() ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
      if (CORBA::is_nil (dw.in ()))
        {
          ACE_ERROR_RETURN ((LM_DEBUG,
                             "Nil TAO::DCPS::DataWriterRemote reference\n"),
                            1);
        }

      ::DDS::DataWriterQos_var dwq = new ::DDS::DataWriterQos;
      dwq->reliability.kind = ::DDS::BEST_EFFORT_RELIABILITY_QOS;
      TAO::DCPS::TransportInterfaceInfo_var tii = new TAO::DCPS::TransportInterfaceInfo;
      ::DDS::PublisherQos_var pQos = new ::DDS::PublisherQos;

      CORBA::Long pubId = info->add_publication(domainId,
                                                dpId,
                                                topicId,
                                                dw.in(),
                                                dwq.in(),
                                                tii.in(),
                                                pQos.in()
                                                ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
      if (0 == pubId)
        {
          ACE_ERROR((LM_ERROR, ACE_TEXT("add_publication failed!\n") ));
        }

      // add an inconsistent topic
      CORBA::Long topicId2;
      CORBA::String_var tname2 = "MYtopic";
      CORBA::String_var dname2 = "MYnewdataname";
      ::DDS::TopicQos_var topicQos2 = new ::DDS::TopicQos;
      TAO::DCPS::TopicStatus topicStatus2 = info->assert_topic(topicId2,
                                                            domainId,
                                                            dpId,
                                                            tname2.in(),
                                                            dname2.in(),
                                                            topicQos2.in()
                                                            ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      if (topicStatus2 != TAO::DCPS::CONFLICTING_TYPENAME)
        {
          ACE_ERROR((LM_ERROR, 
                     ACE_TEXT("Inconsistent topic creation did not fail with ")
                     ACE_TEXT("CONFLICTING_TYPENAME and returned %d"), topicStatus));
        }



      if (ignore_entities)
        {
          ACE_DEBUG((LM_INFO, 
                     ACE_TEXT("Ignoring all entities with 1 and 2\n") ));
          info->ignore_domain_participant(domainId, dpId, 1);
          ACE_TRY_CHECK;
          info->ignore_topic(domainId, dpId, 1);
          ACE_TRY_CHECK;
          info->ignore_publication(domainId, dpId, 1);
          ACE_TRY_CHECK;
          info->ignore_subscription(domainId, dpId, 1);
          ACE_TRY_CHECK;

          info->ignore_domain_participant(domainId, dpId, 2);
          ACE_TRY_CHECK;
          info->ignore_topic(domainId, dpId, 2);
          ACE_TRY_CHECK;
          info->ignore_publication(domainId, dpId, 2);
          ACE_TRY_CHECK;
          info->ignore_subscription(domainId, dpId, 2);
          ACE_TRY_CHECK;
        }


      // Set up the incompatible qos test
      CORBA::Long dpIdAlmost;
      CORBA::Long topicIdAlmost;
      TAO_DDS_DCPSDataWriter_i* dwiAlmost = new TAO_DDS_DCPSDataWriter_i;
      TAO::DCPS::DataWriterRemote_var dwAlmost;
      ::DDS::DataWriterQos_var dwqAlmost = 0;
      CORBA::Long pubIdAlmost;

      if (qos_tests)
      {

        dpIdAlmost = info->add_domain_participant(domainId, dpQos.in());
        ACE_TRY_CHECK;
        if (0 == dpIdAlmost)
          {
            ACE_ERROR((LM_ERROR, ACE_TEXT("add_domain_participant for qos test failed!\n") ));
          }

        // add a topic
        topicStatus = info->assert_topic(topicIdAlmost,
                                         domainId,
                                         dpIdAlmost,
                                         tname.in(),
                                         dname.in(),
                                         topicQos.in()
                                         ACE_ENV_ARG_PARAMETER);
        ACE_TRY_CHECK;

        if (topicStatus != TAO::DCPS::CREATED)
          {
            ACE_ERROR((LM_ERROR,
                      ACE_TEXT("Topic creation for qos test failed and returned %d"),
                      topicStatus));
          }

        // Add publication
        oid = poa->activate_object( dwiAlmost );
        obj = poa->id_to_reference( oid.in() );
        dwAlmost = TAO::DCPS::DataWriterRemote::_narrow(obj.in() ACE_ENV_ARG_PARAMETER);
        ACE_TRY_CHECK;
        if (CORBA::is_nil (dwAlmost.in ()))
          {
            ACE_ERROR_RETURN ((LM_DEBUG,
                              "Nil TAO::DCPS::DataWriterRemote reference in qos test\n"),
                              1);
          }

        dwqAlmost = new ::DDS::DataWriterQos;
        dwqAlmost->reliability.kind = ::DDS::RELIABLE_RELIABILITY_QOS;

        pubIdAlmost = info->add_publication(domainId,
                                            dpIdAlmost,
                                            topicIdAlmost,
                                            dwAlmost.in(),
                                            dwqAlmost.in(),
                                            tii.in(),
                                            pQos.in()
                                            ACE_ENV_ARG_PARAMETER);
        ACE_TRY_CHECK;
        if (0 == pubId)
          {
            ACE_ERROR((LM_ERROR, ACE_TEXT("add_publication for qos test failed!\n") ));
          }
      }




      // run the orb
      ACE_Time_Value run_time = ACE_Time_Value(15,0);
      orb->run(run_time ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      if (ignore_entities)
        {
          ACE_DEBUG((LM_INFO, 
                     ACE_TEXT("Ignoring all entities with 3\n") ));
          info->ignore_domain_participant(domainId, dpId, 3);
          ACE_TRY_CHECK;
          info->ignore_topic(domainId, dpId, 3);
          ACE_TRY_CHECK;
          info->ignore_publication(domainId, dpId, 3);
          ACE_TRY_CHECK;
          info->ignore_subscription(domainId, dpId, 3);
          ACE_TRY_CHECK;

          run_time = ACE_Time_Value(15,0);
          orb->run(run_time ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;
        }

      if (qos_tests)
        {
          // remove all the qos test entities
          info->remove_publication(domainId, dpIdAlmost, pubIdAlmost ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;

          info->remove_topic(domainId, dpIdAlmost, topicIdAlmost ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;

          info->remove_domain_participant(domainId, dpIdAlmost ACE_ENV_ARG_PARAMETER);
          ACE_TRY_CHECK;
        }


      // remove all the entities
      info->remove_publication(domainId, dpId, pubId ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      info->remove_topic(domainId, dpId, topicId ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;

      info->remove_domain_participant(domainId, dpId ACE_ENV_ARG_PARAMETER);
      ACE_TRY_CHECK;
      // clean up the orb
      orb->destroy (ACE_ENV_SINGLE_ARG_PARAMETER);
      ACE_TRY_CHECK;
    }
  ACE_CATCHANY
    {
      ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                           "Exception caught in publisher.cpp:");
      return 1;
    }
  ACE_ENDTRY;

  return 0;
}
