#include "SubDriver.h"
#include "TestException.h"
// Add the TransportImpl.h before TransportImpl_rch.h is included to
// resolve the build problem that the class is not defined when
// RcHandle<T> template is instantiated.
#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include "dds/DCPS/AssociationData.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/RepoIdBuilder.h"
#include "SimpleSubscriber.h"
#include "tests/DCPS/common/TestSupport.h"
#include <ace/Arg_Shifter.h>
#include <ace/OS.h>
#include <ace/Argv_Type_Converter.h>
#include <string>

#include <sstream>

SubDriver::SubDriver()
  : pub_id_fname_ (ACE_TEXT("pub_id.txt")),
    sub_id_ (OpenDDS::DCPS::GUID_UNKNOWN),
    num_writes_ (0),
    pub_driver_ior_ ("file://pubdriver.ior"),
    shutdown_pub_ (1),
    add_new_subscription_ (0),
    shutdown_delay_secs_ (10),
    sub_ready_filename_(ACE_TEXT("sub_ready.txt"))
{
}


SubDriver::~SubDriver()
{
}


void
SubDriver::run(int& argc, ACE_TCHAR* argv[])
{
  parse_args(argc, argv);
  init(argc, argv);
  run();
}


void
SubDriver::parse_args(int& argc, ACE_TCHAR* argv[])
{
  // Command-line arguments:
  //
  // -p <pub_id_fname:pub_host:pub_port>
  // -s <sub_id:sub_host:sub_port>
  // -d <delay before shutdown>
  ACE_Arg_Shifter arg_shifter(argc, argv);

  bool got_p = false;
  bool got_s = false;

  const ACE_TCHAR* current_arg = 0;

  while (arg_shifter.is_anything_left())
    {
      // The '-p' option
      if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-p"))))
        {
          if (got_p)
            {
              ACE_ERROR((LM_ERROR, "(%P|%t) Only one -p allowed on command-line.\n"));
              throw TestException();
            }

          int result = parse_pub_arg(current_arg);
          arg_shifter.consume_arg();

          if (result != 0)
            {
              ACE_ERROR((LM_ERROR, "(%P|%t) Failed to parse -p command-line arg.\n"));
              throw TestException();
            }

          got_p = true;
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-n"))) != 0)
        {
          num_writes_ = ACE_OS::atoi (current_arg);
          arg_shifter.consume_arg ();
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-v"))) != 0)
        {
          pub_driver_ior_ = ACE_TEXT_ALWAYS_CHAR(current_arg);
          arg_shifter.consume_arg ();
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-s"))))
        {
          if (got_s)
            {
              ACE_ERROR((LM_ERROR, "(%P|%t) Only one -s allowed on command-line.\n"));
              throw TestException();
            }

          int result = parse_sub_arg(current_arg);
          arg_shifter.consume_arg();

          if (result != 0)
            {
              ACE_ERROR((LM_ERROR,"(%P|%t) Failed to parse -s command-line arg.\n"));
              throw TestException();
            }

          got_s = true;
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-a"))) != 0)
        {
          add_new_subscription_ = ACE_OS::atoi (current_arg);
          arg_shifter.consume_arg ();
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-x"))) != 0)
        {
          shutdown_pub_ = ACE_OS::atoi (current_arg);
          arg_shifter.consume_arg ();
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-d"))) != 0)
        {
          shutdown_delay_secs_ = ACE_OS::atoi (current_arg);
          arg_shifter.consume_arg ();
        }
      else if ((current_arg = arg_shifter.get_the_parameter(ACE_TEXT("-f"))) != 0)
        {
          sub_ready_filename_ = current_arg;
          arg_shifter.consume_arg ();
        }
      else if (arg_shifter.cur_arg_strncasecmp(ACE_TEXT("-?")) == 0)
        {
          ACE_DEBUG((LM_DEBUG,
                 "usage: %s "
                 "-p pub_id:pub_host:pub_port\n"
                 "-s sub_id:sub_host:sub_port\n"
                 "-n Expected initial messages\n"
                 , argv[0]));

          arg_shifter.consume_arg();
          throw TestException();
        }
      // Anything else we just skip
      else
        {
          arg_shifter.ignore_arg();
        }
    }

  // Make sure we got the required arguments:
  if (!got_p)
    {
      ACE_ERROR((LM_ERROR, "(%P|%t) -p command-line option not specified (required).\n"));
      throw TestException();
    }

  if (!got_s)
    {
      ACE_ERROR((LM_ERROR, "(%P|%t) -s command-line option not specified (required).\n"));
      throw TestException();
    }
}


void
SubDriver::init(int& argc, ACE_TCHAR* argv[])
{
  // initialize the orb
  ACE_Argv_Type_Converter conv (argc, argv);
  orb_ = CORBA::ORB_init (conv.get_argc (),
                          conv.get_ASCII_argv (),
                          OpenDDS::DCPS::DEFAULT_ORB_NAME);

  TheServiceParticipant->set_ORB (orb_.in());
  // Now we can ask TheTransportFactory to create a TransportImpl object
  // using the SIMPLE_TCP type_id.  We also supply an identifier for this
  // particular TransportImpl object that will be created.  This is known
  // as the "impl_id", or "the TransportImpl's instance id".  The point is
  // that we assign the impl_id, and TheTransportFactory caches a reference
  // to the newly created TransportImpl object using the impl_id (ALL_TRAFFIC
  // in our case) as a key to the cache map.  Other parts of this client
  // application code will be able use the obtain() method on
  // TheTransportFactory, provide the impl_id (ALL_TRAFFIC in our case), and
  // a reference to the cached TransportImpl will be returned.
  OpenDDS::DCPS::TransportImpl_rch transport_impl
    = TheTransportFactory->create_transport_impl (ALL_TRAFFIC,
                                                  ACE_TEXT("SimpleTcp"),
                                                  OpenDDS::DCPS::DONT_AUTO_CONFIG);

  // Get or create SimpleTcpConfiguration object.  It just has one field
  // to set - the local_address_ field.  This is the address that will be
  // used to open an acceptor object to listen for passive connection
  // requests.  This is the TransportImpl object's (local) "endpoint" address.
  OpenDDS::DCPS::TransportConfiguration_rch config
    = TheTransportFactory->create_configuration (ALL_TRAFFIC, ACE_TEXT("SimpleTcp"));

  OpenDDS::DCPS::SimpleTcpConfiguration* tcp_config
    = static_cast <OpenDDS::DCPS::SimpleTcpConfiguration*> (config.in ());

  tcp_config->local_address_ = ACE_INET_Addr (this->sub_addr_.c_str ());
  tcp_config->local_address_str_ = this->sub_addr_.c_str ();

  // Supply the config object to the TranportImpl object via its configure()
  // method.
  if (transport_impl->configure(config.in ()) != 0)
    {
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to configure the transport impl\n"));
      throw TestException();
    }

  // Indicate that the subscriber is ready to accept connection
  FILE* readers_ready = ACE_OS::fopen (sub_ready_filename_.c_str (), ACE_LIB_TEXT("w"));
  if (readers_ready == 0)
  {
    ACE_ERROR ((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR Unable to create subscriber ready file\n")));
  }
  else
    ACE_OS::fclose(readers_ready);
  // And we are done with the init().
}

/**
   Class: Pub_Invoke_Task
   Author: Ciju John

   Purpose: The primary purpose of this task class is to
   make a parallel invocation(add_new_subscription) on the
   PubDriver interface. On the pub side this starts up an active
   connection establishment. Previously the pub side had an
   explicit delay put in to compensate for the subscriber
   to initialize itself. Depending upon a time delay is just
   asking for trouble. Now this task does the PubDriver
   invocation, while the parent thread initializes the
   subscriber.
********/
#include "ace/Task.h"
class Pub_Invoke_Task : public ACE_Task_Base
{
public:

  Pub_Invoke_Task ()
    : shutdown_ (false)
    , delay_ (-1)
  { };

  ~Pub_Invoke_Task ()
  {};

  virtual int svc (void)
  {
    ::Test::TestPubDriver_var pub_driver;
    OpenDDS::DCPS::RepoId sub_id = OpenDDS::DCPS::GUID_UNKNOWN;
    ACE_CString       sub_addr;
    int delay = -1;

    while (!shutdown_)
      {
	delay = -1;

	{ // guard scope
	  GuardType guard (lock_);

	  if (this->delay_ >= 0)
	    {
	      // make local copy of protected data.
	      pub_driver = this->pub_driver_;
	      sub_id = this->sub_id_;
	      sub_addr = this->sub_addr_;
	      delay = this->delay_;

	      // reset delay
	      this->delay_ = -1;
	    }
	}

	if (delay >= 0)
	  {
	    ACE_OS::sleep (delay);
	    pub_driver->add_new_subscription (sub_id, sub_addr.c_str ());
	    continue;
	  }

	ACE_OS::sleep (1);
      }

    return 0;
  };

  void register_invocation (const ::Test::TestPubDriver_var pub_driver
                            , OpenDDS::DCPS::RepoId sub_id, const char *sub_addr
                            , int delay = 0)
  {
    GuardType guard (lock_);

    pub_driver_ = pub_driver;
    sub_id_ = sub_id;
    sub_addr_ = CORBA::string_dup (sub_addr);
    delay_ = delay;
  };

  bool shutdown_;

  typedef ACE_SYNCH_MUTEX     LockType;
  typedef ACE_Guard<LockType> GuardType;

private:
  ::Test::TestPubDriver_var pub_driver_;
  OpenDDS::DCPS::RepoId sub_id_;
  ACE_CString       sub_addr_;
  int delay_;

  LockType lock_;
};

void
SubDriver::run()
{
  ACE_DEBUG ((LM_DEBUG,
              ACE_TEXT("(%P|%t) SubDriver::run, ")
              ACE_TEXT(" Wait for %s. \n"),
              pub_id_fname_.c_str ()));


  PublicationIds ids;

  // Wait for the publication id file created by the publisher.
  while (1)
    {
      FILE* fp
	= ACE_OS::fopen (pub_id_fname_.c_str (), ACE_LIB_TEXT("r"));
      if (fp == 0)
	{
	  ACE_OS::sleep (1);
	}
      else
	{
          // this could be made cleaner by losing the old C-style I/O.
	  ::OpenDDS::DCPS::PublicationId pub_id = OpenDDS::DCPS::GUID_UNKNOWN;
          char charBuffer[64];
	  while (fscanf (fp, "%s\n", &charBuffer[0]) != EOF)
	    {
              std::stringstream buffer( charBuffer);
              buffer >> pub_id;
	      ids.push_back(pub_id);

              std::stringstream idbuffer;
              idbuffer << pub_id;
	      ACE_DEBUG ((LM_DEBUG,
			  ACE_TEXT("(%P|%t) SubDriver::run, ")
			  ACE_TEXT(" Got from %s: pub_id=%C.\n"),
			  pub_id_fname_.c_str (),
			  idbuffer.str().c_str()));
	    }
	  ACE_OS::fclose (fp);
	  break;
	}
    }

  CORBA::Object_var object =
    orb_->string_to_object (pub_driver_ior_.c_str ());

  pub_driver_ = ::Test::TestPubDriver::_narrow (object.in ());

  TEST_CHECK (!CORBA::is_nil (pub_driver_.in ()));
  /*
    if (add_new_subscription_ == 1)
    {
    pub_driver_->add_new_subscription (this->sub_id_, this->sub_addr_.c_str ());
    }
  */
  size_t num_publications = ids.size ();

  // Set up the publications.
  OpenDDS::DCPS::AssociationData* publications
    = new OpenDDS::DCPS::AssociationData[num_publications];


  for (size_t i = 0; i < num_publications; i ++)
    {
      publications[i].remote_id_                = ids[i];
      publications[i].remote_data_.transport_id = ALL_TRAFFIC; // TBD later - wrong
      publications[i].remote_data_.publication_transport_priority = 0;

      OpenDDS::DCPS::NetworkAddress network_order_address(this->pub_addr_str_);

      ACE_OutputCDR cdr;
      cdr << network_order_address;
      size_t len = cdr.total_length ();

      publications[i].remote_data_.data
        = OpenDDS::DCPS::TransportInterfaceBLOB
        (len,
        len,
        (CORBA::Octet*)(cdr.buffer ()));

    }

  Pub_Invoke_Task pub_invoke_task;
  pub_invoke_task.activate (THR_NEW_LWP | THR_JOINABLE, 1);

  this->subscriber_.init (ALL_TRAFFIC,
                          this->sub_id_,
                          num_publications,
                          publications);

  if (add_new_subscription_ == 1)
    {
      // Since the Subscriber is ready, no need for Publisher to
      // wait for Subscriber initialization.
      // We still noticed failures on some machines. Since we don't
      // have any means of controlling the delay on the re-enque
      // messages, we will do a best effort and put a 5 sec sleep
      // here for the subscriber to initialize all the way.
      pub_invoke_task.register_invocation (this->pub_driver_, this->sub_id_
        , ACE_TEXT_ALWAYS_CHAR(this->sub_addr_.c_str ()), 5);
    }

  this->subscriber_.associate ();

  delete [] publications;

  while (this->subscriber_.received_test_message() != num_writes_)
    {
      ACE_OS::sleep(1);
    }

  if (shutdown_pub_ == 1)
    {
      pub_driver_->shutdown ();
    }

  pub_invoke_task.shutdown_ = true;
  pub_invoke_task.wait ();

  // Sleep before release transport so the connection will not go away.
  // This would avoid the problem of publisher sendv failure due to lost
  // connection during the shutdown period.
  // ciju: Don't see the need for this. We know before hand how many
  // messages to expect. No need for a general delay(sleep) here.
  // ACE_OS::sleep (shutdown_delay_secs_);

  // Tear-down the entire Transport Framework.
  TheTransportFactory->release();
  TheServiceParticipant->shutdown();
}

int
SubDriver::parse_pub_arg(const ACE_TString& arg)
{
  ACE_TString::size_type pos;

  // Find the first ':' character, and make sure it is in a legal spot.
  if ((pos = arg.find(ACE_TEXT(':'))) == ACE_TString::npos) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) Bad -p command-line value (%s). Missing ':' char.\n",
               arg.c_str()));
    return -1;
  }

  if (pos == 0) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) Bad -p command-line value (%s). "
               "':' char cannot be first char.\n",
               arg.c_str()));
    return -1;
  }

  if (pos == (arg.length() - 1)) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) Bad -p command-line value  (%s) - "
               "':' char cannot be last char.\n",
               arg.c_str()));
    return -1;
  }

  // Parse the pub_id from left of ':' char, and remainder to right of ':'.
  ACE_TString pub_id_str(arg.c_str(), pos);
  this->pub_addr_str_ = arg.c_str() + pos + 1;

  this->pub_id_fname_ = pub_id_str.c_str();
  this->pub_addr_ = ACE_INET_Addr(this->pub_addr_str_.c_str());

  return 0;
}

int
SubDriver::parse_sub_arg(const ACE_TString& arg)
{
  ACE_TString::size_type pos;

  // Find the first ':' character, and make sure it is in a legal spot.
  if ((pos = arg.find(ACE_TEXT(':'))) == ACE_TString::npos) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) Bad -s command-line value (%s). Missing ':' char.\n",
               arg.c_str()));
    return -1;
  }

  if (pos == 0) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) Bad -s command-line value (%s). "
               "':' char cannot be first char.\n",
               arg.c_str()));
    return -1;
  }

  if (pos == (arg.length() - 1)) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) Bad -s command-line value  (%s) - "
               "':' char cannot be last char.\n",
               arg.c_str()));
    return -1;
  }

  // Parse the sub_id from left of ':' char, and remainder to right of ':'.
  ACE_TString sub_id_str(arg.c_str(), pos);
  ACE_TString sub_addr_str(arg.c_str() + pos + 1);

  // RepoIds are conventionally created and managed by the DCPSInfoRepo. Those
  // generated here are for the sole purpose of verifying internal behavior.
  OpenDDS::DCPS::RepoIdBuilder builder(sub_id_);

  builder.participantId(1);
  builder.entityKey(ACE_OS::atoi(sub_id_str.c_str()));
  builder.entityKind(OpenDDS::DCPS::ENTITYKIND_USER_WRITER_WITH_KEY);

  this->sub_addr_ = sub_addr_str.c_str();

  return 0;
}
