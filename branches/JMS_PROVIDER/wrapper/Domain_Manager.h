// -*- C++ -*-

//=============================================================================
/**
 *  @file    Domain_Manager.h
 *
 *  $Id$
 *
 *  @author  Friedhelm Wolf (fwolf@dre.vanderbilt.edu)
 */
//=============================================================================

#ifndef _DOMAIN_MANAGER_H_
#define _DOMAIN_MANAGER_H_

#include "DDSWrapper_export.h"
#include "Domain_Manager_Impl.h"

/**
 * @class ManagerException
 * @author Friedhelm Wolf (fwolf@dre.vanderbilt.edu)
 * @brief general purpose exception class for the manager classes
 */
class DDSWrapper_Export Manager_Exception {
 public:
  /// constructor
  Manager_Exception (const std::string& reason);

  /// getter method for the reason_ class member
  std::string reason () const;

 private:
  std::string reason_; /// description of the reason for the exception
};

/**
 * @class Domain_Manager
 * @author Friedhelm Wolf (fwolf@dre.vanderbilt.edu)
 * @brief class for memory management of Domain_Manager_Impl classes
 *
 * This class plays the role of an Abstraction in the Bridge pattern.
 */
class DDSWrapper_Export Domain_Manager 
{
 public:
  /// default ctor
  Domain_Manager ();

  /// ctor
  /// @param argc number of command line arguments
  /// @param argv commandline arguments used for initialization
  /// @param domain_id in which domain participant should be registered
  Domain_Manager (int & argc, 
		  char *argv[],
		  DDS::DomainId_t domain_id);

  /// ctor that takes ownership of the passed in impl pointer
  Domain_Manager (Domain_Manager_Impl * impl);

  /// copy constructor
  Domain_Manager (const Domain_Manager & copy);

  /// assignment operator
  void operator= (const Domain_Manager& copy);  

  /// destructor
  ~Domain_Manager ();

  /// this call blocks the thread until a SIGINT signal for the process is received
  void run ();

  /// this call causes the run method to terminate
  void shutdown ();

  /// factory method for subscription managers
  Subscription_Manager subscription_manager ();

  /// returns a subscription manager for built-in topics
  Subscription_Manager builtin_topic_subscriber ();

  /// getter method for publication managers
  Publication_Manager publication_manager ();

  /// getter method for the internal domain participant
  /// the memory is managed by the Domain_Manager
  DDS::DomainParticipant_ptr participant ();

 private:
  Reference_Counter_T <Domain_Manager_Impl> manager_impl_;  
};

#if defined (__ACE_INLINE__)
#include "Domain_Manager.inl"
#endif

#endif /* _DOMAIN_MANAGER_H_ */
