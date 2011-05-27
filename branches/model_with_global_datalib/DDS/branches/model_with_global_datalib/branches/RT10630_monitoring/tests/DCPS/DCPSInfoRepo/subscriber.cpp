#include  "dds/DdsDcpsInfoC.h"
#include  "DCPSDataReaderI.h"
#include  "dds/DCPS/Service_Participant.h"

#include "ace/Arg_Shifter.h"
#include "ace/INET_Addr.h"


const char *ior = "file://dcps_ir.ior";

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
    else if ((currentArg = arg_shifter.get_the_parameter("-?")) != 0)
    {
      ACE_ERROR_RETURN ((LM_ERROR,
                          "usage:  %s "
                          "-k <ior> "
                          "\n"
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
  try
    {
      CORBA::ORB_var orb =
        CORBA::ORB_init (argc, argv, "");

      if (parse_args (argc, argv) != 0)
        return 1;

      TheServiceParticipant->set_ORB (orb.in ());

      //Get reference to the RootPOA.
      CORBA::Object_var obj = orb->resolve_initial_references( "RootPOA" );
      PortableServer::POA_var poa = PortableServer::POA::_narrow( obj.in() );


      // Activate the POAManager.
      PortableServer::POAManager_var mgr = poa->the_POAManager();
      mgr->activate();

      CORBA::Object_var tmp =
        orb->string_to_object (ior);

      TAO::DCPS::DCPSInfo_var info =
        TAO::DCPS::DCPSInfo::_narrow (tmp.in ());

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
      ACE_INET_Addr addr;
      const char * hostname = addr.get_host_name ();
      CORBA::Long process_id = static_cast <CORBA::Long> (ACE_OS::thr_self ());

      CORBA::Long dpId = info->add_domain_participant(domainId, dpQos.in(),
                                                      hostname, process_id);
      if (0 == dpId)
        {
          ACE_ERROR((LM_ERROR, ACE_TEXT("add_domain_participant failed!\n") ));
        }

      CORBA::Long topicId;
      const char* tname = "MYtopic";
      const char* dname = "MYdataname";
      ::DDS::TopicQos_var topicQos = new ::DDS::TopicQos;
      TAO::DCPS::TopicStatus topicStatus = info->assert_topic(topicId,
                                                           domainId,
                                                           dpId,
                                                           tname,
                                                           dname,
                                                           topicQos.in());

      if (topicStatus != TAO::DCPS::CREATED)
        {
          ACE_ERROR((LM_ERROR, ACE_TEXT("Topic creation failed and returned %d"), topicStatus));
        }


      // Add subscription
      TAO_DDS_DCPSDataReader_i dri;
      PortableServer::ObjectId_var oid = poa->activate_object( &dri );
      obj = poa->id_to_reference( oid.in() );
      TAO::DCPS::DataReaderRemote_var dr = TAO::DCPS::DataReaderRemote::_narrow(obj.in());
      if (CORBA::is_nil (dr.in ()))
        {
          ACE_ERROR_RETURN ((LM_DEBUG,
                             "Nil TAO::DCPS::DataReaderRemote reference\n"),
                            1);
        }

      ::DDS::DataReaderQos_var drq = new ::DDS::DataReaderQos;
      drq->reliability.kind = ::DDS::RELIABLE_RELIABILITY_QOS;
      ::DDS::SubscriberQos_var sQos = new ::DDS::SubscriberQos;
      TAO::DCPS::TransportInterfaceInfo_var tii = new TAO::DCPS::TransportInterfaceInfo;

      CORBA::Long subId = info->add_subscription(domainId,
                                                 dpId,
                                                 topicId,
                                                 dr.in(),
                                                 drq.in(),
                                                 tii.in(),
                                                 sQos.in());
      if (0 == subId)
        {
          ACE_ERROR((LM_ERROR, ACE_TEXT("add_subscription failed!\n") ));
        }

      ACE_Time_Value run_time = ACE_Time_Value(15,0);
      orb->run(run_time);


      info->remove_subscription(domainId, dpId, subId);

      info->remove_topic(domainId, dpId, topicId);

      info->remove_domain_participant(domainId, dpId);

      orb->destroy ();

      TheServiceParticipant->shutdown ();
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in subscriber.cpp:");
      return 1;
    }

  return 0;
}
