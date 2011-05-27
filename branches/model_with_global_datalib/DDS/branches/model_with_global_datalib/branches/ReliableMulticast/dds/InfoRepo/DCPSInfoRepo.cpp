#include "DcpsInfo_pch.h"
#include "DCPSInfo_i.h"

#include "dds/DCPS/Service_Participant.h"

#include "dds/DCPS/transport/framework/EntryExit.h"

#include "tao/ORB_Core.h"
#include "tao/IORTable/IORTable.h"

#include "ace/Get_Opt.h"
#include "ace/Arg_Shifter.h"
#include "ace/Service_Config.h"

static ACE_TString ior_file (ACE_TEXT("repo.ior"));
static ACE_TString domain_file (ACE_TEXT("domain_ids"));
static ACE_TString listen_address_str; //(ACE_TEXT("localhost:2839")); // = 0xB17

//static const char * ior_file = "repo.ior";
//static const char * domain_file = "domain_ids";
//static const char * listen_address_str = "localhost:2839"; // = 0xB17
static int listen_address_given = 0;
static bool use_bits = true;

void
usage (const ACE_TCHAR * cmd)
{
  ACE_DEBUG ((LM_INFO,
              ACE_TEXT ("Usage:\n")
              ACE_TEXT ("  %s\n")
              ACE_TEXT ("    -a <address> listening address for Built-In Topics\n")
              ACE_TEXT ("    -o <file> write ior to file\n")
              ACE_TEXT ("    -d <file> load domain ids from file\n")
              ACE_TEXT ("    -NOBITS disable the Built-In Topics\n")
              ACE_TEXT ("    -z turn on verbose Transport logging\n")
              ACE_TEXT ("    -?\n")
              ACE_TEXT ("\n"),
              cmd));
}

void
parse_args (int argc,
            ACE_TCHAR *argv[])
{
  listen_address_str = ACE_LOCALHOST;
  listen_address_str += ":2839";

  ACE_Arg_Shifter arg_shifter(argc, argv);

  const char* current_arg = 0;

  while (arg_shifter.is_anything_left())
    {
      if ( (current_arg = arg_shifter.get_the_parameter("-a")) != 0)
        {
          ::listen_address_str = current_arg;
          listen_address_given = 1;
          arg_shifter.consume_arg();
        }
      else if ((current_arg = arg_shifter.get_the_parameter("-d")) != 0)
        {
          ::domain_file = current_arg;
          arg_shifter.consume_arg ();
        }
      else if ((current_arg = arg_shifter.get_the_parameter("-o")) != 0)
        {
          ::ior_file = current_arg;
          arg_shifter.consume_arg ();
        }
      else if (arg_shifter.cur_arg_strncasecmp("-NOBITS") == 0)
        {
          ::use_bits = false;
          arg_shifter.consume_arg ();
        }
      else if (arg_shifter.cur_arg_strncasecmp("-z") == 0)
        {
          TURN_ON_VERBOSE_DEBUG;
          arg_shifter.consume_arg();
        }

      // The '-?' option
      else if (arg_shifter.cur_arg_strncasecmp("-?") == 0)
        {
          ::usage (argv[0]);
          ACE_OS::exit (0);

        }
      // Anything else we just skip

      else
        {
          arg_shifter.ignore_arg();
        }
    }
}


int
ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{

  ACE_DEBUG((LM_DEBUG,"(%P|%t) %T Repo main\n")); //REMOVE

  try
    {
      // The usual server side boilerplate code.

      CORBA::ORB_var orb = CORBA::ORB_init (argc,
                                            argv,
                                            "");

      // ciju: Hard-code the 'RW' wait strategy directive.
      // Deadlocks have been observed to occur otherwise under stress conditions.
      ACE_Service_Config::process_directive
	(ACE_TEXT("static Client_Strategy_Factory \"-ORBWaitStrategy rw ")
	 ACE_TEXT("-ORBTransportMuxStrategy exclusive -ORBConnectStrategy blocked\""));
      ACE_Service_Config::process_directive
	(ACE_TEXT("static Resource_Factory \"-ORBFlushingStrategy blocking\""));

      CORBA::Object_var obj =
        orb->resolve_initial_references ("RootPOA");

      PortableServer::POA_var root_poa =
        PortableServer::POA::_narrow (obj.in ());

      PortableServer::POAManager_var poa_manager =
        root_poa->the_POAManager ();

      poa_manager->activate ();

      TAO_DDS_DCPSInfo_i info;

      // Use persistent and user id POA policies so the Info Repo's
      // object references are consistent.
      CORBA::PolicyList policies (2);
      policies.length (2);
      policies[0] =
        root_poa->create_id_assignment_policy (PortableServer::USER_ID);
      policies[1] =
        root_poa->create_lifespan_policy (PortableServer::PERSISTENT);
      PortableServer::POA_var poa = root_poa->create_POA ("InfoRepo",
                                                          poa_manager.in (),
                                                          policies);

      // Creation of the new POAs over, so destroy the Policy_ptr's.
      for (CORBA::ULong i = 0; i < policies.length (); ++i)
        {
          policies[i]->destroy ();
        }

      PortableServer::ObjectId_var oid =
        PortableServer::string_to_ObjectId ("InfoRepo");
      poa->activate_object_with_id (oid.in (),
                                    &info);
      obj = poa->id_to_reference(oid.in());
      TAO::DCPS::DCPSInfo_var info_repo = TAO::DCPS::DCPSInfo::_narrow(
                      obj.in ());

      CORBA::String_var objref_str =
        orb->object_to_string (info_repo.in ());

      TheServiceParticipant->set_ORB(orb.in());
      TheServiceParticipant->set_repo_ior(objref_str.in());

      // Initialize the DomainParticipantFactory
      ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);

      // We need parse the command line options for DCPSInfoRepo after parsing DCPS specific
      // command line options.

      // Check the non-ORB arguments.
      ::parse_args (argc,
                    argv);

      if (::use_bits)
        {
          ACE_INET_Addr address (::listen_address_str.c_str());
          if (0 != info.init_transport(listen_address_given, address))
            {
              ACE_ERROR_RETURN((LM_ERROR,
                                ACE_TEXT("ERROR: Failed to initialize the transport!\n")),
                               -1);
            }
        }

      // Load the domains _after_ initializing the participant factory and initializing
      // the transport
      if (0 >= info.load_domains(::domain_file.c_str(), ::use_bits))
        {
          //ACE_ERROR_RETURN((LM_ERROR, "ERROR: Failed to load any domains!\n"), -1);
        }

      CORBA::Object_var table_object =
        orb->resolve_initial_references ("IORTable");

      IORTable::Table_var adapter =
        IORTable::Table::_narrow (table_object.in ());
      if (CORBA::is_nil (adapter.in ()))
        {
          ACE_ERROR ((LM_ERROR, "Nil IORTable\n"));
        }
      else
        {
          adapter->bind ("DCPSInfoRepo", objref_str.in ());
        }

      FILE * file = ACE_OS::fopen (::ior_file.c_str(), "w");
      if (file != NULL)
  {
    ACE_OS::fprintf (file, "%s", objref_str.in ());
    ACE_OS::fclose (file);

    orb->run ();
  }
      else {
  ACE_ERROR((LM_ERROR, "ERROR: Unable to open IOR file: %s\n", ::ior_file.c_str()));
      }

      TheServiceParticipant->shutdown ();

      orb->destroy ();
    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception (
        "ERROR: ::DDS DCPS Info Repo caught exception");

      return -1;
    }

  return 0;
}
