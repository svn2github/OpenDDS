// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h"
#include "debug.h"
#include  "Service_Participant.h"
#include  "BuiltInTopicUtils.h"
#include  "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include  "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration_rch.h"
#include  "dds/DCPS/transport/simpleTCP/SimpleTcpFactory.h"
#include  "dds/DCPS/transport/framework/TheTransportFactory.h"
#include  "tao/ORB_Core.h"
#include  "ace/Arg_Shifter.h"
#include  "ace/Reactor.h"


#if ! defined (__ACE_INLINE__)
#include "Service_Participant.inl"
#endif /* __ACE_INLINE__ */


namespace TAO
{
  namespace DCPS
  {
    int Service_Participant::zero_argc = 0;

    const int DEFAULT_BIT_TRANSPORT_PORT = 0xB17 + 2; // = 2841

    const size_t DEFAULT_NUM_CHUNKS = 20;

    const size_t DEFAULT_CHUNK_MULTIPLIER = 10;

    const int BIT_LOOKUP_DURATION_SEC = 2;

    //tbd: Temeporary hardcode the repo ior for DSCPInfo object reference.
    //     Change it to be from configuration file.
    static const char* ior = "file://repo.ior";

    Service_Participant::Service_Participant ()
    : orb_ (CORBA::ORB::_nil ()), 
      orb_from_user_(0),
      n_chunks_ (DEFAULT_NUM_CHUNKS),
      association_chunk_multiplier_(DEFAULT_CHUNK_MULTIPLIER),
      liveliness_factor_ (80),
      bit_transport_port_(DEFAULT_BIT_TRANSPORT_PORT),
      bit_enabled_ (false),
      bit_lookup_duration_ (BIT_LOOKUP_DURATION_SEC)
    {
      initialize();
    }

    int
    Service_Participant::svc ()
    {
      ACE_DECLARE_NEW_CORBA_ENV;
        {
          bool done = false;
          while (! done) 
            {
              ACE_TRY
                {
                  if (orb_->orb_core()->has_shutdown () == false) 
                    {
                      orb_->run (ACE_ENV_ARG_PARAMETER);
                      ACE_TRY_CHECK;
                    }
                  done = true;
                }
              ACE_CATCH (CORBA::SystemException, sysex)
                {
                  ACE_PRINT_EXCEPTION (sysex, "ERROR: Service_Participant::svc");
                }
              ACE_CATCH (CORBA::UserException, userex)
                {
                  ACE_PRINT_EXCEPTION (userex, "ERROR: Service_Participant::svc");
                }
              ACE_CATCHANY
                {
                  ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION, "ERROR: Service_Participant::svc");
                }
              ACE_ENDTRY;
              if (orb_->orb_core()->has_shutdown ()) 
                {
                  done = true;
                }
              else 
                {
                  orb_->orb_core()->reactor()->reset_reactor_event_loop ();
                }
            }
        }
      
      return 0;
    }

    int 
    Service_Participant::set_ORB (CORBA::ORB_ptr orb)
    {
      // The orb is already created by the get_domain_participant_factory() call.
      ACE_ASSERT (CORBA::is_nil (orb_.in ()));
      // The provided orb should not be nil.
      ACE_ASSERT (! CORBA::is_nil (orb));

      orb_ = CORBA::ORB::_duplicate (orb);
      orb_from_user_ = 1;
      return 0;
    }

    CORBA::ORB_ptr 
    Service_Participant::get_ORB ()
    {
      // This method should be called after either set_ORB is called 
      // or get_domain_participant_factory is called.
      ACE_ASSERT (! CORBA::is_nil (orb_.in ()));

      return CORBA::ORB::_duplicate (orb_.in ());
    }

    PortableServer::POA_ptr 
    Service_Participant::the_poa() 
    {
      if (CORBA::is_nil (root_poa_.in ()))
        {
          CORBA::Object_var obj = orb_->resolve_initial_references( "RootPOA" );
          root_poa_ = PortableServer::POA::_narrow( obj.in() );
        }
      return PortableServer::POA::_duplicate (root_poa_.in ());
    }

    void 
    Service_Participant::shutdown ()
    {
      ACE_TRY_NEW_ENV
        {
          ACE_GUARD (TAO_SYNCH_MUTEX, guard, this->factory_lock_);
          ACE_ASSERT (! CORBA::is_nil (orb_.in ()));
          if (! orb_from_user_) 
            {
              orb_->shutdown (0 ACE_ENV_ARG_PARAMETER); 
              this->wait ();
            }
        // Don't delete the participants - require the client code to delete participants
        #if 0
          //TBD return error code from this call 
          // -- non-empty entity will make this call return failure
          if (dp_factory_impl_->delete_contained_participants () != ::DDS::RETCODE_OK)
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT ("(%P|%t) ERROR: Service_Participant::shutdown, ")
                          ACE_TEXT ("delete_contained_participants failed.\n")));
            }
        #endif

          if (! orb_from_user_) 
            {
              root_poa_->destroy (1, 1 ACE_ENV_ARG_PARAMETER);
              orb_->destroy (ACE_ENV_SINGLE_ARG_PARAMETER);
            }
          orb_ = CORBA::ORB::_nil ();
          dp_factory_ = ::DDS::DomainParticipantFactory::_nil ();
        }
      ACE_CATCHANY
        {
          ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION,
                              "ERROR: Service_Participant::shutdown");
          return;
        }
      ACE_ENDTRY;
    }


    ::DDS::DomainParticipantFactory_ptr 
    Service_Participant::get_domain_participant_factory (int &argc, 
                                                         ACE_TCHAR *argv[])
    {
      if (CORBA::is_nil (dp_factory_.in ())) 
        {
          ACE_GUARD_RETURN (TAO_SYNCH_MUTEX, 
                            guard, 
                            this->factory_lock_, 
                            ::DDS::DomainParticipantFactory::_nil ());

          if (CORBA::is_nil (dp_factory_.in ())) 
            {
              ACE_DECLARE_NEW_CORBA_ENV;
              ACE_TRY
                {
                  if (CORBA::is_nil (orb_.in ())) 
                    {
                      //TBD: allow user to specify the ORB id

                      // Use a unique ORB for the ::DDS Service
                      // to avoid conflicts with other CORBA code
                      orb_ = CORBA::ORB_init (argc, 
                                              argv, 
                                              "TAO_DDS_DCPS" 
                                              ACE_ENV_ARG_PARAMETER);
                      ACE_TRY_CHECK;
                    }

                  if (parse_args (argc, argv) != 0)
                    {
                      return ::DDS::DomainParticipantFactory::_nil ();
                    }

                  ACE_ASSERT ( ! CORBA::is_nil (orb_.in ()));

                  CORBA::Object_var poa_object =
                    orb_->resolve_initial_references("RootPOA" ACE_ENV_ARG_PARAMETER);
                  ACE_TRY_CHECK;

                  root_poa_ = PortableServer::POA::_narrow (poa_object.in () 
                                                            ACE_ENV_ARG_PARAMETER);
                  ACE_TRY_CHECK;

                  if (CORBA::is_nil (root_poa_.in ()))
                    {
                      ACE_ERROR ((LM_ERROR, 
                                  ACE_TEXT ("(%P|%t) ERROR: ")
                                  ACE_TEXT ("Service_Participant::get_domain_participant_factory, ")
                                  ACE_TEXT ("nil RootPOA\n")));
                      return ::DDS::DomainParticipantFactory::_nil ();
                    }

                  ACE_NEW_RETURN (dp_factory_servant_,
                                  DomainParticipantFactoryImpl (),
                                  ::DDS::DomainParticipantFactory::_nil ());

                  dp_factory_ = servant_to_reference<DDS::DomainParticipantFactory, 
                                                     DomainParticipantFactoryImpl, 
                                                     DDS::DomainParticipantFactory_ptr> 
                                (dp_factory_servant_ ACE_ENV_ARG_PARAMETER);
                  ACE_TRY_CHECK;
                  
                  // Give ownership to poa.
                  dp_factory_servant_->_remove_ref ();

                  if (CORBA::is_nil (dp_factory_.in ()))
                    {
                      ACE_ERROR ((LM_ERROR, 
                                  ACE_TEXT ("(%P|%t) ERROR: ")
                                  ACE_TEXT ("Service_Participant::get_domain_participant_factory, ")
                                  ACE_TEXT ("nil DomainParticipantFactory. \n")));
                      return ::DDS::DomainParticipantFactory::_nil ();
                    }


                  CORBA::Object_var obj = orb_->string_to_object (ior ACE_ENV_ARG_PARAMETER);
                  ACE_TRY_CHECK;

                  repo_ = DCPSInfo::_narrow (obj.in () ACE_ENV_ARG_PARAMETER);
                  ACE_TRY_CHECK;

                  if (CORBA::is_nil (repo_.in ()))
                    {
                      ACE_ERROR ((LM_ERROR, 
                                  ACE_TEXT ("(%P|%t) ERROR: ")
                                  ACE_TEXT ("Service_Participant::get_domain_participant_factory, ")
                                  ACE_TEXT ("nil DCPSInfo. \n"))); 
                      return ::DDS::DomainParticipantFactory::_nil ();
                    }

                  if (! this->orb_from_user_)
                    {
                      PortableServer::POAManager_var poa_manager =
                        root_poa_->the_POAManager (ACE_ENV_SINGLE_ARG_PARAMETER);
                      ACE_TRY_CHECK;

                      poa_manager->activate (ACE_ENV_SINGLE_ARG_PARAMETER);
                      ACE_TRY_CHECK;

                      if (activate (THR_NEW_LWP | THR_JOINABLE, 1) == -1) 
                        {
                          ACE_ERROR ((LM_ERROR, 
                                      ACE_TEXT ("ERROR: Service_Participant::get_domain_participant_factory, ")
                                      ACE_TEXT ("Failed to activate the orb task."))); 
                          return ::DDS::DomainParticipantFactory::_nil ();
                        }
                    }
                }
              ACE_CATCHANY
                {
                  ACE_PRINT_EXCEPTION (ACE_ANY_EXCEPTION, 
                                      "ERROR: Service_Participant::get_domain_participant_factory");
                  return ::DDS::DomainParticipantFactory::_nil ();
                }
              ACE_ENDTRY;
              ACE_CHECK_RETURN (::DDS::DomainParticipantFactory::_nil ());
            }
        }

      return ::DDS::DomainParticipantFactory::_duplicate (dp_factory_.in ());
    }


    int
    Service_Participant::parse_args (int &argc, ACE_TCHAR *argv[])
    {
      ACE_Arg_Shifter arg_shifter (argc, argv);

      while (arg_shifter.is_anything_left ()) 
        {
          const char *currentArg = 0;

          if ((currentArg = arg_shifter.get_the_parameter("-DCPSDebugLevel")) != 0) 
            {
              DCPS_debug_level = ACE_OS::atoi (currentArg);
              arg_shifter.consume_arg ();
            }
          else if ((currentArg = arg_shifter.get_the_parameter("-DCPSInfo")) != 0) 
            {
              ior = currentArg;
              arg_shifter.consume_arg ();
            }
          else if ((currentArg = arg_shifter.get_the_parameter("-DCPSChunks")) != 0) 
            {
              n_chunks_ = ACE_OS::atoi (currentArg);
              arg_shifter.consume_arg ();
            }
          else if ((currentArg = arg_shifter.get_the_parameter("-DCPSChunkAssociationMutltiplier")) != 0) 
            {
              association_chunk_multiplier_ = ACE_OS::atoi (currentArg);
              arg_shifter.consume_arg ();
            }
          else 
            {
              arg_shifter.ignore_arg ();
            }
        }

      // Indicates sucessful parsing of the command line
      return 0;
    }

    void 
    Service_Participant::initialize ()
    {
      //NOTE: in the future these initial values may be configurable
      //      (to override the Specification's default values
      //       hmm - I guess that would be OK since the user
      //       is overriding them.)
      initial_TransportPriorityQosPolicy_.value = 0;
      initial_LifespanQosPolicy_.duration.sec = ::DDS::DURATION_INFINITY_SEC;
      initial_LifespanQosPolicy_.duration.nanosec = ::DDS::DURATION_INFINITY_NSEC;

      initial_DurabilityQosPolicy_.kind = ::DDS::VOLATILE_DURABILITY_QOS;
      initial_DurabilityQosPolicy_.service_cleanup_delay.sec = ::DDS::DURATION_ZERO_SEC;
      initial_DurabilityQosPolicy_.service_cleanup_delay.nanosec = ::DDS::DURATION_ZERO_NSEC;

      initial_PresentationQosPolicy_.access_scope = ::DDS::INSTANCE_PRESENTATION_QOS;
      initial_PresentationQosPolicy_.coherent_access = 0;
      initial_PresentationQosPolicy_.ordered_access = 0;

      initial_DeadlineQosPolicy_.period.sec = ::DDS::DURATION_INFINITY_SEC;
      initial_DeadlineQosPolicy_.period.nanosec = ::DDS::DURATION_INFINITY_NSEC;

      initial_LatencyBudgetQosPolicy_.duration.sec = ::DDS::DURATION_ZERO_SEC;
      initial_LatencyBudgetQosPolicy_.duration.nanosec = ::DDS::DURATION_ZERO_NSEC;

      initial_OwnershipQosPolicy_.kind = ::DDS::SHARED_OWNERSHIP_QOS;
      initial_OwnershipStrengthQosPolicy_.value = 0;

      initial_LivelinessQosPolicy_.kind = ::DDS::AUTOMATIC_LIVELINESS_QOS;
      initial_LivelinessQosPolicy_.lease_duration.sec = ::DDS::DURATION_INFINITY_SEC;
      initial_LivelinessQosPolicy_.lease_duration.nanosec = ::DDS::DURATION_INFINITY_NSEC;

      initial_TimeBasedFilterQosPolicy_.minimum_separation.sec = ::DDS::DURATION_ZERO_SEC;
      initial_TimeBasedFilterQosPolicy_.minimum_separation.nanosec = ::DDS::DURATION_ZERO_NSEC;

      initial_ReliabilityQosPolicy_.kind = ::DDS::BEST_EFFORT_RELIABILITY_QOS;
      // The spec does not provide the default max_blocking_time.
      initial_ReliabilityQosPolicy_.max_blocking_time.sec = ::DDS::DURATION_INFINITY_SEC;
      initial_ReliabilityQosPolicy_.max_blocking_time.nanosec = ::DDS::DURATION_INFINITY_NSEC;

      initial_DestinationOrderQosPolicy_.kind = ::DDS::BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS;

      initial_HistoryQosPolicy_.kind = ::DDS::KEEP_LAST_HISTORY_QOS;
      initial_HistoryQosPolicy_.depth = 1;

      initial_ResourceLimitsQosPolicy_.max_samples = ::DDS::LENGTH_UNLIMITED;
      initial_ResourceLimitsQosPolicy_.max_instances = ::DDS::LENGTH_UNLIMITED;
      initial_ResourceLimitsQosPolicy_.max_samples_per_instance = ::DDS::LENGTH_UNLIMITED;

      initial_EntityFactoryQosPolicy_.autoenable_created_entities = 1;

      initial_WriterDataLifecycleQosPolicy_.autodispose_unregistered_instances = 1;

      initial_ReaderDataLifecycleQosPolicy_.autopurge_nowriter_samples_delay.sec = ::DDS::DURATION_ZERO_SEC;
      initial_ReaderDataLifecycleQosPolicy_.autopurge_nowriter_samples_delay.nanosec = ::DDS::DURATION_ZERO_NSEC;

      initial_DomainParticipantQos_.user_data = initial_UserDataQosPolicy_;
      initial_DomainParticipantQos_.entity_factory = initial_EntityFactoryQosPolicy_;

      initial_TopicQos_.topic_data = initial_TopicDataQosPolicy_;
      initial_TopicQos_.durability = initial_DurabilityQosPolicy_;
      initial_TopicQos_.deadline = initial_DeadlineQosPolicy_;
      initial_TopicQos_.latency_budget = initial_LatencyBudgetQosPolicy_;
      initial_TopicQos_.liveliness = initial_LivelinessQosPolicy_;
      initial_TopicQos_.reliability = initial_ReliabilityQosPolicy_;
      initial_TopicQos_.destination_order = initial_DestinationOrderQosPolicy_;
      initial_TopicQos_.history = initial_HistoryQosPolicy_;
      initial_TopicQos_.resource_limits = initial_ResourceLimitsQosPolicy_;
      initial_TopicQos_.transport_priority = initial_TransportPriorityQosPolicy_;
      initial_TopicQos_.lifespan = initial_LifespanQosPolicy_;
      initial_TopicQos_.ownership = initial_OwnershipQosPolicy_;

      initial_DataWriterQos_.durability = initial_DurabilityQosPolicy_;
      initial_DataWriterQos_.deadline = initial_DeadlineQosPolicy_;
      initial_DataWriterQos_.latency_budget = initial_LatencyBudgetQosPolicy_;
      initial_DataWriterQos_.liveliness = initial_LivelinessQosPolicy_;
      initial_DataWriterQos_.reliability = initial_ReliabilityQosPolicy_;
      initial_DataWriterQos_.destination_order = initial_DestinationOrderQosPolicy_;
      initial_DataWriterQos_.history = initial_HistoryQosPolicy_;
      initial_DataWriterQos_.resource_limits = initial_ResourceLimitsQosPolicy_;
      initial_DataWriterQos_.transport_priority = initial_TransportPriorityQosPolicy_;
      initial_DataWriterQos_.lifespan = initial_LifespanQosPolicy_;
      initial_DataWriterQos_.user_data = initial_UserDataQosPolicy_;
      initial_DataWriterQos_.ownership_strength = initial_OwnershipStrengthQosPolicy_;
      initial_DataWriterQos_.writer_data_lifecycle = initial_WriterDataLifecycleQosPolicy_;

      initial_PublisherQos_.presentation = initial_PresentationQosPolicy_;
      initial_PublisherQos_.partition = initial_PartitionQosPolicy_;
      initial_PublisherQos_.group_data = initial_GroupDataQosPolicy_;
      initial_PublisherQos_.entity_factory = initial_EntityFactoryQosPolicy_;

      initial_DataReaderQos_.durability = initial_DurabilityQosPolicy_;
      initial_DataReaderQos_.deadline = initial_DeadlineQosPolicy_;
      initial_DataReaderQos_.latency_budget = initial_LatencyBudgetQosPolicy_;
      initial_DataReaderQos_.liveliness = initial_LivelinessQosPolicy_;
      initial_DataReaderQos_.reliability = initial_ReliabilityQosPolicy_;
      initial_DataReaderQos_.destination_order = initial_DestinationOrderQosPolicy_;
      initial_DataReaderQos_.history = initial_HistoryQosPolicy_;
      initial_DataReaderQos_.resource_limits = initial_ResourceLimitsQosPolicy_;
      initial_DataReaderQos_.user_data = initial_UserDataQosPolicy_;
      initial_DataReaderQos_.time_based_filter = initial_TimeBasedFilterQosPolicy_;
      initial_DataReaderQos_.reader_data_lifecycle = initial_ReaderDataLifecycleQosPolicy_;

      initial_SubscriberQos_.presentation = initial_PresentationQosPolicy_;
      initial_SubscriberQos_.partition = initial_PartitionQosPolicy_;
      initial_SubscriberQos_.group_data = initial_GroupDataQosPolicy_;
      initial_SubscriberQos_.entity_factory = initial_EntityFactoryQosPolicy_;
    }

    void
    Service_Participant::set_repo_ior(const char* repo_ior)
    {
      ior = repo_ior;
    }

    int 
    Service_Participant::bit_transport_port () const
    {
       return bit_transport_port_; 
    }

    void 
    Service_Participant::bit_transport_port (int port)
    {
       bit_transport_port_ = port;
    }

    int
    Service_Participant::init_bit_transport_impl ()
    {
#if !defined (DDS_HAS_MINIMUM_BIT)
      TheTransportFactory->register_type(BIT_SIMPLE_TCP,
        new SimpleTcpFactory());

      SimpleTcpConfiguration_rch config =
        new SimpleTcpConfiguration();

      // localhost will only work for DCPSInfo on the same machine.
      // Don't specify an address to an OS picked address is used.
      //config->local_address_ 
      //  = ACE_INET_Addr (bit_transport_port_, "localhost");

      bit_transport_impl_ =
        TheTransportFactory->create(BIT_ALL_TRAFFIC,
                                    BIT_SIMPLE_TCP);

      if (bit_transport_impl_->configure(config.in()) != 0)
        {
          ACE_ERROR((LM_ERROR,
                     ACE_TEXT("(%P|%t) ERROR: TAO_DDS_DCPSInfo_i::init_bit_transport_impl: ")
                     ACE_TEXT("Failed to configure the transport.\n")));
          return -1;
        }
      else
        {
          return 0;
        }
#else
      return -1;
#endif // DDS_HAS_MINIMUM_BIT
    }


    TransportImpl_rch 
    Service_Participant::bit_transport_impl ()
    {
      if (bit_transport_impl_.is_nil ())
        {
          init_bit_transport_impl ();
        }

      return bit_transport_impl_;
    }

    int 
    Service_Participant::bit_lookup_duration_sec () const
    {
      return bit_lookup_duration_;
    }

    void 
    Service_Participant::bit_lookup_duration_sec (int sec)
    {
      bit_lookup_duration_ = sec;
    }

  } // namespace DCPS
} // namespace TAO


// gcc on AIX needs explicit instantiation of the singleton templates
#if defined (ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION) || (defined (__GNUC__) && defined (_AIX))

template class TAO_Singleton<Service_Participant, TAO_SYNCH_MUTEX>;

#elif defined (ACE_HAS_TEMPLATENSTANTIATION_PRAGMA)

#pragma instantiate TAO_Singleton<Service_Participant, TAO_SYNCH_MUTEX>

#endif /*ACE_HAS_EXPLICIT_TEMPLATE_INSTANTIATION */

