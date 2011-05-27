#include "test_helper.h" //must be the 1st include
#include "PubDriver.h"
#include "Writer.h"
#include "TestException.h"
#include "tests/DCPS/FooType3Unbounded/FooDefTypeSupportC.h"
#include "tests/DCPS/FooType3Unbounded/FooDefTypeSupportImpl.h"
#include "tests/DCPS/FooType3Unbounded/FooDefC.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include "dds/DCPS/AssociationData.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "tests/DCPS/common/TestSupport.h"

#include <ace/Arg_Shifter.h>

#include <sstream>

const long  MY_DOMAIN   = 411;
const char* MY_TOPIC    = "foo";
const char* MY_TYPE     = "foo";

PubDriver::PubDriver()
: participant_ (::DDS::DomainParticipant::_nil ()),
  topic_ (::DDS::Topic::_nil ()),
  publisher_ (::DDS::Publisher::_nil ()),
  datawriters_ (0),
  writers_ (0),
  pub_id_fname_ ("pub_id.txt"),
  sub_id_ ( OpenDDS::DCPS::GUID_UNKNOWN),
  block_on_write_ (0),
  num_threads_to_write_ (0),
  multiple_instances_ (0),
  num_writes_per_thread_ (1),
  num_datawriters_ (1),
  max_samples_per_instance_(::DDS::LENGTH_UNLIMITED),
  history_depth_ (1),
  has_key_ (1),
  write_delay_msec_ (0),
  check_data_dropped_ (0),
  pub_driver_ior_ ("pubdriver.ior"),
  shutdown_ (false),
  sub_ready_filename_("sub_ready.txt")
{
}


PubDriver::~PubDriver()
{
  delete [] datawriters_;
  for (int i = 0; i < num_datawriters_; i ++)
  {
    delete writers_[i];
  }

  delete [] writers_;
}


void
PubDriver::run(int& argc, char* argv[])
{
  parse_args(argc, argv);
  init(argc, argv);

  run();

  while (shutdown_ == false)
  {
    ACE_OS::sleep (1);
  }

  end();
}


void
PubDriver::parse_args(int& argc, char* argv[])
{
  // Command-line arguments:
  //
  //  -p <pub_id_fname:pub_host:pub_port>
  //  -s <sub_id:sub_host:sub_port>
  //
  //  -b <block/non-block waiting>
  //  -t num_threads_to_write    defaults to 1
  //  -i num_writes_per_thread   defaults to 1
  //  -w num_datawriters_        defaults to 1
  //  -b block_on_write?1:0      defaults to 0
  //  -m multiple_instances?1:0  defaults to 0
  //  -n max_samples_per_instance defaults to INFINITE
  //  -d history.depth           defaults to 1
  //  -y has_key_flag            defaults to 1
  //  -r data_dropped            defaults to 0
  ACE_Arg_Shifter arg_shifter(argc, argv);

  bool got_p = false;
  bool got_s = false;

  const char* current_arg = 0;

  while (arg_shifter.is_anything_left())
  {
    // The '-p' option
    if ((current_arg = arg_shifter.get_the_parameter("-p"))) {
      if (got_p) {
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) Only one -p allowed on command-line.\n"));
        throw TestException();
      }

      int result = parse_pub_arg(current_arg);
      arg_shifter.consume_arg();

      if (result != 0) {
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) Failed to parse -p command-line arg.\n"));
        throw TestException();
      }

      got_p = true;
    }
    // A '-s' option
    else if ((current_arg = arg_shifter.get_the_parameter("-s"))) {
      if (got_s) {
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) Only one -s allowed on command-line.\n"));
        throw TestException();
      }

      int result = parse_sub_arg(current_arg);
      arg_shifter.consume_arg();

      if (result != 0) {
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) Failed to parse -s command-line arg.\n"));
        throw TestException();
      }

      got_s = true;
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-b")) != 0)
    {
      block_on_write_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-t")) != 0)
    {
      num_threads_to_write_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-m")) != 0)
    {
      multiple_instances_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-i")) != 0)
    {
      num_writes_per_thread_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-w")) != 0)
    {
      num_datawriters_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-n")) != 0)
    {
      max_samples_per_instance_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp("-DCPS") != -1)
    {
      // ignore -DCPSxxx options that will be handled by Service_Participant
      arg_shifter.ignore_arg();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-d")) != 0)
    {
      history_depth_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-y")) != 0)
    {
      has_key_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-v")) != 0)
    {
      pub_driver_ior_ = current_arg;
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-l")) != 0)
    {
      write_delay_msec_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-r")) != 0)
    {
      check_data_dropped_ = ACE_OS::atoi (current_arg);
      arg_shifter.consume_arg ();
    }
    else if ((current_arg = arg_shifter.get_the_parameter("-f")) != 0)
    {
      sub_ready_filename_ = current_arg;
      arg_shifter.consume_arg ();
    }
    // The '-?' option
    else if (arg_shifter.cur_arg_strncasecmp("-?") == 0) {
      ACE_DEBUG((LM_DEBUG,
                 "usage: %s "
                 "-p pub_id:pub_host:pub_port -s sub_id:sub_host:sub_port\n",
                 argv[0]));

      arg_shifter.consume_arg();
      throw TestException();
    }
    // Anything else we just skip
    else {
      arg_shifter.ignore_arg();
    }
  }

  // Make sure we got the required arguments:
  if (!got_p) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) -p command-line option not specified (required).\n"));
    throw TestException();
  }

  if (!got_s) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) -s command-line option not specified (required).\n"));
    throw TestException();
  }
}


void
PubDriver::init(int& argc, char *argv[])
{
  // Create DomainParticipant and then publisher, topic and datawriter.
  ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);

  // Activate the PubDriver servant and write its ior to a file.
  PortableServer::POA_var poa = TheServiceParticipant->the_poa ();
  CORBA::ORB_var orb = TheServiceParticipant->get_ORB ();

  PortableServer::ObjectId_var id = poa->activate_object(this);

  CORBA::Object_var object = poa->id_to_reference(id.in());

  CORBA::String_var ior_string = orb->object_to_string (object.in ());

  //
  // Write the IOR to a file.
  //
  FILE *output_file= ACE_OS::fopen (pub_driver_ior_.c_str (), "w");
  if (output_file == 0)
  {
    ACE_ERROR ((LM_ERROR,
                ACE_TEXT("Cannot open output file for writing IOR\n")));
  }
  ACE_OS::fprintf (output_file, "%s", ior_string.in ());
  ACE_OS::fclose (output_file);

  datawriters_ = new ::DDS::DataWriter_var[num_datawriters_];
  writers_ = new Writer* [num_datawriters_];

  ::Xyz::FooTypeSupport_var fts (new ::Xyz::FooTypeSupportImpl);

  participant_ =
    dpf->create_participant(MY_DOMAIN,
                            PARTICIPANT_QOS_DEFAULT,
                            ::DDS::DomainParticipantListener::_nil());
  TEST_CHECK (! CORBA::is_nil (participant_.in ()));

  if (::DDS::RETCODE_OK != fts->register_type(participant_.in (), MY_TYPE))
    {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT ("Failed to register the FooTypeSupport.")));
    }


  ::DDS::TopicQos topic_qos;
  participant_->get_default_topic_qos(topic_qos);

  if (block_on_write_)
  {
    topic_qos.reliability.kind  = ::DDS::RELIABLE_RELIABILITY_QOS;
    topic_qos.resource_limits.max_samples_per_instance = max_samples_per_instance_;
    topic_qos.history.kind  = ::DDS::KEEP_ALL_HISTORY_QOS;
  }
  else
  {
    topic_qos.history.depth = history_depth_;
  }

  topic_ = participant_->create_topic (MY_TOPIC,
                                       MY_TYPE,
                                       topic_qos,
                                       ::DDS::TopicListener::_nil());
  TEST_CHECK (! CORBA::is_nil (topic_.in ()));


  publisher_ =
    participant_->create_publisher(PUBLISHER_QOS_DEFAULT,
                          ::DDS::PublisherListener::_nil());
  TEST_CHECK (! CORBA::is_nil (publisher_.in ()));

  attach_to_transport ();

  ::DDS::DataWriterQos datawriter_qos;
  publisher_->get_default_datawriter_qos (datawriter_qos);

  if (block_on_write_)
  {
    datawriter_qos.reliability.kind  = ::DDS::RELIABLE_RELIABILITY_QOS;
    datawriter_qos.resource_limits.max_samples_per_instance = max_samples_per_instance_;
    datawriter_qos.history.kind  = ::DDS::KEEP_ALL_HISTORY_QOS;
  }
  else
  {
    datawriter_qos.history.depth = history_depth_;
  }

  // Create one datawriter or multiple datawriters belong to the same
  // publisher.
  for (int i = 0; i < num_datawriters_; i ++)
  {
    datawriters_[i]
    = publisher_->create_datawriter(topic_.in (),
                                    datawriter_qos,
                                    ::DDS::DataWriterListener::_nil());
    TEST_CHECK (! CORBA::is_nil (datawriters_[i].in ()));
  }
}

void
PubDriver::end()
{
  ACE_DEBUG((LM_DEBUG, "(%P|%t)PubDriver::end \n"));

  // Record samples been written in the Writer's data map.
  // Verify the number of instances and the number of samples
  // written to the datawriter.
  for (int i = 0; i < num_datawriters_; i ++)
  {
    writers_[i]->end ();
    InstanceDataMap& map = writers_[i]->data_map ();
    if (multiple_instances_ == 0 || has_key_ == 0)
    {
      // One instance when data type has a key value and all instances
      // have the same key or has no key value.
      TEST_CHECK (map.num_instances() == 1);
    }
    else
    {
      // multiple instances test - an instance per thread
      TEST_CHECK (map.num_instances() == num_threads_to_write_);
    }
    TEST_CHECK (map.num_samples() == num_threads_to_write_ * num_writes_per_thread_);

    publisher_->delete_datawriter(datawriters_[i].in ());
  }

  // clean up the service objects
  participant_->delete_publisher(publisher_.in ());

  participant_->delete_topic(topic_.in ());

  ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactory;
  dpf->delete_participant(participant_.in ());

  // Tear-down the entire Transport Framework.
  TheTransportFactory->release();

  TheServiceParticipant->shutdown ();
}

void
PubDriver::run()
{
  FILE* fp = ACE_OS::fopen (pub_id_fname_.c_str (), ACE_LIB_TEXT("w"));
  if (fp == 0)
  {
    ACE_ERROR ((LM_ERROR,
                ACE_LIB_TEXT("Unable to open %s for writing:(%u) %p\n"),
                pub_id_fname_.c_str (),
                ACE_LIB_TEXT("PubDriver::run")));
    return;
  }

  for (int i = 0; i < num_datawriters_; i ++)
  {
    ::Xyz::FooDataWriterImpl* datawriter_servant
      = dynamic_cast< ::Xyz::FooDataWriterImpl*>
      (datawriters_[i].in ());
    OpenDDS::DCPS::PublicationId pub_id = datawriter_servant->get_publication_id ();
    std::stringstream buffer;
    buffer << pub_id;

    // Write the publication id to a file.
    ACE_DEBUG ((LM_DEBUG,
                ACE_TEXT("(%P|%t) PubDriver::run, ")
                ACE_TEXT(" Write to %s: pub_id=%s. \n"),
                pub_id_fname_.c_str (),
                buffer.str().c_str()));

    ACE_OS::fprintf (fp, "%s\n", buffer.str().c_str());
  }

  fclose (fp);

  // Wait for the subscriber to be ready to accept connection.
  FILE* readers_ready = 0;
  do
    {
      ACE_Time_Value small(0,250000);
      ACE_OS::sleep (small);
      readers_ready = ACE_OS::fopen (sub_ready_filename_.c_str (), ACE_LIB_TEXT("r"));
    } while (0 == readers_ready);

  ACE_OS::fclose(readers_ready);

  // Set up the subscriptions.
  ::OpenDDS::DCPS::ReaderAssociationSeq associations;
  associations.length (1);
  associations[0].readerTransInfo.transport_id = 1; // TBD - not right
  associations[0].readerTransInfo.publication_transport_priority = 0;

  OpenDDS::DCPS::NetworkAddress network_order_address(this->sub_addr_str_);

  ACE_OutputCDR cdr;
  cdr << network_order_address;
  size_t len = cdr.total_length ();

  associations[0].readerTransInfo.data
    = OpenDDS::DCPS::TransportInterfaceBLOB
    (len,
    len,
    (CORBA::Octet*)(cdr.buffer ()));

  associations[0].readerId = this->sub_id_;
  associations[0].subQos = TheServiceParticipant->initial_SubscriberQos ();
  associations[0].readerQos = TheServiceParticipant->initial_DataReaderQos ();

  { // make VC6 buid - avoid error C2374: 'i' : redefinition; multiple initialization
  for (int i = 0; i < num_datawriters_; i ++)
  {

    ::Xyz::FooDataWriterImpl* datawriter_servant
      = dynamic_cast< ::Xyz::FooDataWriterImpl*>
      (datawriters_[i].in ());
    OpenDDS::DCPS::PublicationId pub_id = datawriter_servant->get_publication_id ();
    ::OpenDDS::DCPS::DataWriterRemote_var dw_remote =
        DDS_TEST::getRemoteInterface(*datawriter_servant);

    dw_remote->add_associations (pub_id, associations);
  }
  }

  // Let the subscriber catch up before we broadcast.
  ACE_OS::sleep (2);

  { // make VC6 buid - avoid error C2374: 'i' : redefinition; multiple initialization

  // Each Writer/DataWriter launch threads to write samples
  // to the same instance or multiple instances.
  // When writing to multiple instances, the instance key
  // identifies instances is the thread id.
  for (int i = 0; i < num_datawriters_; i ++)
  {
    writers_[i] = new Writer(this,
                             datawriters_[i].in (),
                             num_threads_to_write_,
                             num_writes_per_thread_,
                             multiple_instances_,
                             i,
                             has_key_,
                             write_delay_msec_,
                             check_data_dropped_);
    writers_[i]->start ();
  }
  }
}

int
PubDriver::parse_pub_arg(const std::string& arg)
{
  std::string::size_type pos;

  // Find the first ':' character, and make sure it is in a legal spot.
  if ((pos = arg.find_first_of(':')) == std::string::npos) {
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
  std::string pub_id_str(arg,0,pos);
  this->pub_addr_str_ = std::string (arg,pos+1,std::string::npos); //use 3-arg constructor to build with VC6

  this->pub_id_fname_ = pub_id_str.c_str();
  this->pub_addr_ = ACE_INET_Addr(this->pub_addr_str_.c_str());

  return 0;
}

int
PubDriver::parse_sub_arg(const std::string& arg)
{
  std::string::size_type pos;

  // Find the first ':' character, and make sure it is in a legal spot.
  if ((pos = arg.find_first_of(':')) == std::string::npos) {
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
  std::string sub_id_str(arg,0,pos);
  this->sub_addr_str_ = std::string (arg,pos+1,std::string::npos); //use 3-arg constructor to build with VC6

  OpenDDS::DCPS::GuidConverter converter( 0, 1); // Federation == 0, Participant == 1
  converter.kind()   = OpenDDS::DCPS::ENTITYKIND_USER_WRITER_WITH_KEY;
  converter.key()[2] = ACE_OS::atoi(sub_id_str.c_str());
  this->sub_id_ = converter;

  // Use the remainder as the "stringified" ACE_INET_Addr.
  this->sub_addr_ = ACE_INET_Addr(this->sub_addr_str_.c_str());

  return 0;
}

void PubDriver::shutdown (
  )
  ACE_THROW_SPEC ((
    CORBA::SystemException
  ))
{
  shutdown_ = true;
}


void PubDriver::attach_to_transport ()
{
  // create TransportImpl.
  OpenDDS::DCPS::TransportImpl_rch transport_impl
    = TheTransportFactory->create_transport_impl (ALL_TRAFFIC, "SimpleTcp", OpenDDS::DCPS::DONT_AUTO_CONFIG);

  OpenDDS::DCPS::TransportConfiguration_rch config
    = TheTransportFactory->create_configuration (ALL_TRAFFIC, "SimpleTcp");

  OpenDDS::DCPS::SimpleTcpConfiguration* tcp_config
    = static_cast <OpenDDS::DCPS::SimpleTcpConfiguration*> (config.in ());

  tcp_config->local_address_ = this->pub_addr_;
  tcp_config->local_address_str_ = this->pub_addr_str_;

  if (transport_impl->configure(config.in ()) != 0)
    {
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to configure the transport impl\n"));
      throw TestException();
    }

  // Attach the Publisher with the TransportImpl.
  ::OpenDDS::DCPS::PublisherImpl* pub_servant
    = dynamic_cast < ::OpenDDS::DCPS::PublisherImpl*>(publisher_.in());

  TEST_CHECK (pub_servant != 0);

  OpenDDS::DCPS::AttachStatus status
    = pub_servant->attach_transport(transport_impl.in ());

  if (status != OpenDDS::DCPS::ATTACH_OK)
  {
    // We failed to attach to the transport for some reason.
    std::string status_str;

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

    ACE_ERROR((LM_ERROR,
                "(%P|%t) Failed to attach to the transport. "
                "AttachStatus == %s\n", status_str.c_str()));
    throw TestException();
  }
}
