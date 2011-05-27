// -*- C++ -*-
//
// $Id$
#ifndef TAO_DDS_DCPS_SERVICE_PARTICIPANT_H
#define TAO_DDS_DCPS_SERVICE_PARTICIPANT_H

#include  "DomainParticipantFactoryImpl.h"
#include  "dds/DdsDcpsInfrastructureS.h"
#include  "dds/DdsDcpsDomainC.h"
#include  "dds/DdsDcpsInfoC.h"
#include  "DomainParticipantFactoryImpl.h"
#include  "dds/DCPS/transport/framework/TransportImpl_rch.h"
#include  "dds/DCPS/transport/framework/TransportImpl.h"
#include  "tao/TAO_Singleton.h"

#include  "tao/PortableServer/Root_POA.h"

#include  "ace/Task.h"
#include  "ace/Auto_Ptr.h"
#include  "ace/Configuration.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */


namespace TAO
{
  namespace DCPS
  {
    /**
    * @class Service_Participant
    *
    * @brief Service entrypoint.
    *        
    *   This class is a sigleton that allows ::DDS client applications to configure  
    *   the TAO implementation of ::DDS. This includes running the ORB to support 
    *   ::DDS.
    *
    *   @note: The client will either create an ORB and call set_ORB() before 
    *   calling get_domain_particpant_factory() and will run the ORB *or* 
    *   it will not call set_ORB() first and get_domain_particpant_factory() 
    *   will automatically create an ORB to be used by ::DDS and will run that
    *   ORB in a thread it creates.
    *
    *   @note:  this class may read a configuration file that will
    *           configure Transports as well as DCPS (e.g. number of ORB
    *           threads).
    **/
    class TAO_DdsDcps_Export Service_Participant : public ACE_Task_Base
    {
      static int zero_argc;

    public:
      /** Constructor **/
      Service_Participant ();

      /** Lanch a thread to run the orb. **/
      virtual int svc ();

      /** Client provides an ORB for the ::DDS client to use. **/
      int set_ORB (CORBA::ORB_ptr orb);

      /** Get the ORB used by ::DDS.
      *   Only valid after set_ORB() or get_domain_participant_factory()
      *   called. 
      **/
      CORBA::ORB_ptr get_ORB ();

      /** Initialize the ::DDS client environment and get the
      *  DomainParticipantFactory.
      *  This method consumes -DCPS* options and thier arguments
      *  Unless the client/application code calls other methods to define how
      *  the ORB is run, calling this method will initiallize the ORB and then
      *  run it in a separate thread.
      **/
      ::DDS::DomainParticipantFactory_ptr get_domain_participant_factory (
          int &argc = zero_argc, 
          ACE_TCHAR *argv[] = 0
        );

      /** Stop being a participant in the service.
      * Will shutdown the ORB unless it was given via set_ORB().
      * @note Required Precondition: all DomainParticipants have been deleted.
      **/
      void shutdown();

      /** Accessor of the poa that application used.
      *  tbd: Currently this method return the rootpoa, we might create our own poa.
      **/
      PortableServer::POA_ptr the_poa ();

      /** Accessor of the DCPSInfo object reference. **/
      DCPSInfo_ptr get_repository () const;
      
      /** Accessors of the qos policy initial values. **/
      ::DDS::UserDataQosPolicy               initial_UserDataQosPolicy () const;
      ::DDS::TopicDataQosPolicy              initial_TopicDataQosPolicy () const;
      ::DDS::GroupDataQosPolicy              initial_GroupDataQosPolicy () const;
      ::DDS::TransportPriorityQosPolicy      initial_TransportPriorityQosPolicy () const;
      ::DDS::LifespanQosPolicy               initial_LifespanQosPolicy () const;
      ::DDS::DurabilityQosPolicy             initial_DurabilityQosPolicy () const;
      ::DDS::PresentationQosPolicy           initial_PresentationQosPolicy () const;
      ::DDS::DeadlineQosPolicy               initial_DeadlineQosPolicy () const;
      ::DDS::LatencyBudgetQosPolicy          initial_LatencyBudgetQosPolicy () const;
      ::DDS::OwnershipQosPolicy              initial_OwnershipQosPolicy () const;
      ::DDS::OwnershipStrengthQosPolicy      initial_OwnershipStrengthQosPolicy () const;
      ::DDS::LivelinessQosPolicy             initial_LivelinessQosPolicy () const;
      ::DDS::TimeBasedFilterQosPolicy        initial_TimeBasedFilterQosPolicy () const;
      ::DDS::PartitionQosPolicy              initial_PartitionQosPolicy () const;
      ::DDS::ReliabilityQosPolicy            initial_ReliabilityQosPolicy () const;
      ::DDS::DestinationOrderQosPolicy       initial_DestinationOrderQosPolicy () const;
      ::DDS::HistoryQosPolicy                initial_HistoryQosPolicy () const;
      ::DDS::ResourceLimitsQosPolicy         initial_ResourceLimitsQosPolicy () const;
      ::DDS::EntityFactoryQosPolicy          initial_EntityFactoryQosPolicy () const;
      ::DDS::WriterDataLifecycleQosPolicy    initial_WriterDataLifecycleQosPolicy () const;
      ::DDS::ReaderDataLifecycleQosPolicy    initial_ReaderDataLifecycleQosPolicy () const;

      ::DDS::DomainParticipantQos            initial_DomainParticipantQos () const;
      ::DDS::TopicQos                        initial_TopicQos () const;
      ::DDS::DataWriterQos                   initial_DataWriterQos () const;
      ::DDS::PublisherQos                    initial_PublisherQos () const;
      ::DDS::DataReaderQos                   initial_DataReaderQos () const;
      ::DDS::SubscriberQos                   initial_SubscriberQos () const;

      /// This accessor is to provide the configurable number of chunks that
      /// a datawriter's cached allocator need to allocate when the resource 
      ///  limits are infinite.  Has a default, can be set by the -DCPSChunks 
      /// option, or by n_chunks() setter.
      size_t   n_chunks () const;
      
      /// set the value returned by n_chunks() accessor.
      /// See accessor description.
      void     n_chunks (size_t chunks);

      /// This accessor is to provide the multiplier for allocators
      /// that have resources used on a per association basis. 
      /// Has a default, can be set by the -DCPSChunkAssociationMutltiplier 
      /// option, or by n_association_chunk_multiplier() setter.
      size_t   association_chunk_multiplier () const;
      
      /// set the value returned by n_association_chunk_multiplier() accessor.
      /// See accessor description.
      void     association_chunk_multiplier (size_t multiplier);

      /// Set the Liveliness propagation delay factor.
      /// @param factor % of lease period before sending a liveliness message.
      void liveliness_factor (int factor);

      /// Accessor of the Liveliness propagation delay factor.
      /// @return % of lease period before sending a liveliness message.
      int liveliness_factor () const;
      
      /** Set the InfoRepo's ior manually.
      * NOTE: This has to be called before the get_domain_participant_factory
      *       method.
      */
      void set_repo_ior(const char* repo_ior);

      /** 
      * Accessors for bit_transport_port_.
      * The accessor is used for client application to configure
      * the local transport listening port number.
      *
      * Note: The default port is INVALID. The user needs call 
      *       this function to setup the desired port number. 
      */
      int bit_transport_port () const;
      void bit_transport_port (int port);
    
      /// Accessor of the TransportImpl used by the builtin topics.
      TransportImpl_rch bit_transport_impl ();

      /**
      * Accessor for bit_lookup_duration_msec_.
      * The accessor is used for client application to configure
      * the timeout for lookup data from the builtin topic 
      * datareader.  Value is in milliseconds.
      */
      int bit_lookup_duration_msec () const;
      void bit_lookup_duration_msec (int msec);

      ///TBD: Should be removed finally.
      ///     Added temparary to turn on and off the builtin topic stuff.
      ///     It defaults to turned off. The BIT test needs enable BIT by
      ///     calling this function.
      void set_BIT (bool flag)
        {
          bit_enabled_ = flag;
        }
       
      bool get_BIT ()
        {
          return bit_enabled_;
        }

      /** Create the TransportImpl for all builtin topics.
       */
      int init_bit_transport_impl ();

    private:
      
      /** Initalize default qos **/
      void initialize ();

      /** Parse the command line for user options. e.g. "-DCPSInfo <iorfile>". 
       *  It consumes -DCPS* options and thier arguments
       */
      int parse_args (int &argc, ACE_TCHAR *argv[]);

      /** Import the configuration file to the ACE_Configuration_Heap object and load 
       *  common section configuration to the Service_Participant singleton and load
       *  the factory and transport section configuration to the TransportFactory
       *  singleton.
       */
      int load_configuration ();

      /** Load the common configuration to the Service_Participant singleton.
       *  Note: The values from command line can overwrite the values in configuration file.
       */
      int load_common_configuration ();

      /// The orb object reference which can be provided by client or initialized  
      /// by this sigleton.
      CORBA::ORB_var orb_;

      /// true if set_ORB() was called.
      int orb_from_user_;

      /// The root poa object reference.
      PortableServer::POA_var root_poa_;

      /// The domain participant factory servant.
      /// Allocate the factory on the heap to avoid the circular dependency
      /// since the TAO::DCPS::DomainParticipantFactoryImpl constructor calls the
      /// TAO::DCPS::Service_Participant singleton. 
      DomainParticipantFactoryImpl*        dp_factory_servant_;

      /// The domain participant factory object reference.
      ::DDS::DomainParticipantFactory_var  dp_factory_;
      
      /// The DCPSInfo/repository object reference.
      DCPSInfo_var repo_;

      /// The lock to serialize DomainParticipantFactory singleton creation 
      /// and shutdown.
      TAO_SYNCH_MUTEX      factory_lock_;

      /// The initial values of qos policies.
      ::DDS::UserDataQosPolicy               initial_UserDataQosPolicy_;
      ::DDS::TopicDataQosPolicy              initial_TopicDataQosPolicy_;
      ::DDS::GroupDataQosPolicy              initial_GroupDataQosPolicy_;
      ::DDS::TransportPriorityQosPolicy      initial_TransportPriorityQosPolicy_;
      ::DDS::LifespanQosPolicy               initial_LifespanQosPolicy_;
      ::DDS::DurabilityQosPolicy             initial_DurabilityQosPolicy_;
      ::DDS::PresentationQosPolicy           initial_PresentationQosPolicy_;
      ::DDS::DeadlineQosPolicy               initial_DeadlineQosPolicy_;
      ::DDS::LatencyBudgetQosPolicy          initial_LatencyBudgetQosPolicy_;
      ::DDS::OwnershipQosPolicy              initial_OwnershipQosPolicy_;
      ::DDS::OwnershipStrengthQosPolicy      initial_OwnershipStrengthQosPolicy_;
      ::DDS::LivelinessQosPolicy             initial_LivelinessQosPolicy_;
      ::DDS::TimeBasedFilterQosPolicy        initial_TimeBasedFilterQosPolicy_;
      ::DDS::PartitionQosPolicy              initial_PartitionQosPolicy_;
      ::DDS::ReliabilityQosPolicy            initial_ReliabilityQosPolicy_;
      ::DDS::DestinationOrderQosPolicy       initial_DestinationOrderQosPolicy_;
      ::DDS::HistoryQosPolicy                initial_HistoryQosPolicy_;
      ::DDS::ResourceLimitsQosPolicy         initial_ResourceLimitsQosPolicy_;
      ::DDS::EntityFactoryQosPolicy          initial_EntityFactoryQosPolicy_;
      ::DDS::WriterDataLifecycleQosPolicy    initial_WriterDataLifecycleQosPolicy_;
      ::DDS::ReaderDataLifecycleQosPolicy    initial_ReaderDataLifecycleQosPolicy_;

      ::DDS::DomainParticipantQos            initial_DomainParticipantQos_;
      ::DDS::TopicQos                        initial_TopicQos_;
      ::DDS::DataWriterQos                   initial_DataWriterQos_;
      ::DDS::PublisherQos                    initial_PublisherQos_;
      ::DDS::DataReaderQos                   initial_DataReaderQos_;
      ::DDS::SubscriberQos                   initial_SubscriberQos_;

      ::DDS::LivelinessLostStatus            initial_liveliness_lost_status_ ;
      ::DDS::OfferedDeadlineMissedStatus     initial_offered_deadline_missed_status_ ;
      ::DDS::OfferedIncompatibleQosStatus    initial_offered_incompatible_qos_status_ ;
      ::DDS::PublicationMatchStatus          initial_publication_match_status_ ;

      /// The configurable value of the number chunks that the DataWriter's
      /// cached allocator can allocate.
      size_t                                 n_chunks_;
      /// The configurable value of maximum number of expected associations
      /// for publishers and subscribers.  This is used to pre allocate enough
      /// memory and reduce heap allocations.
      size_t                                 association_chunk_multiplier_;
      /// The propagation delay factor.
      int                                    liveliness_factor_;

      /// The local transport port number.
      int                                    bit_transport_port_;

      /// The transport impl for builtin topics.
      TransportImpl_rch                      bit_transport_impl_;

      
      bool bit_enabled_;

      /// The timeout for lookup data from the builtin topic datareader.
      int                                    bit_lookup_duration_msec_;

      /// The configuration object that imports the configuration file.
      ACE_Configuration_Heap cf_;
    };

   
    typedef TAO_Singleton<Service_Participant, TAO_SYNCH_MUTEX> TAO_SERVICE_PARTICIPANT;

    TAO_DDSDCPS_SINGLETON_DECLARE (::TAO_Singleton, Service_Participant, TAO_SYNCH_MUTEX)  

    #define TheServiceParticipant                     TAO::DCPS::TAO_SERVICE_PARTICIPANT::instance()

    #define TheParticipantFactory                     TheServiceParticipant->get_domain_participant_factory()

    #define TheParticipantFactoryWithArgs(argc, argv) TheServiceParticipant->get_domain_participant_factory(argc, argv)

    /// Get a servant pointer given an object reference.
    /// @throws PortableServer::POA::OjbectNotActive, 
    ///         PortableServer::POA::WrongAdapter
    ///         PortableServer::POA::WongPolicy
    template <class T_impl, class T_ptr>
    T_impl* reference_to_servant (
      T_ptr p
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ))
    {
      if (CORBA::is_nil (p))
        {
          return 0;
        }

      PortableServer::POA_var poa = TheServiceParticipant->the_poa ();

      T_impl* the_servant = ACE_dynamic_cast (T_impl*, 
           poa->reference_to_servant (
              p ACE_ENV_ARG_PARAMETER) );
      ACE_CHECK_RETURN (0);

      // Use the ServantBase_var so that the servant's reference 
      // count will not be changed by this operation.
      PortableServer::ServantBase_var servant = the_servant;

      return the_servant;
    }

    /// Get an object reference given the servant pointer.
    /// @throws PortableServer::POA::ServantNotActive, 
    ///         PortableServer::POA::WongPolicy
    template <class T, class T_impl, class T_ptr>
    T_ptr servant_to_reference (
      T_impl* servant
      ACE_ENV_ARG_DECL
    )
    ACE_THROW_SPEC ((
      CORBA::SystemException
    ))
    {
      PortableServer::POA_var poa = TheServiceParticipant->the_poa ();
      CORBA::Object_var obj = poa->servant_to_reference (servant ACE_ENV_ARG_PARAMETER);
      ACE_CHECK_RETURN(T::_nil ());

      T_ptr the_obj = T::_narrow (obj.in () ACE_ENV_ARG_PARAMETER); 
      ACE_CHECK_RETURN(T::_nil ());
      return the_obj;
    }	

  } // namespace DCPS
} // namespace TAO


#if defined(__ACE_INLINE__)
#include "Service_Participant.inl"
#endif /* __ACE_INLINE__ */


#endif /* TAO_DDS_DCPS_SERVICE_PARTICIPANT_H  */
