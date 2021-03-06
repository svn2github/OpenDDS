// -*- C++ -*-
/*
 * $Id$
 */

#ifndef _DCPSINFOREPOSERV_H
#define _DCPSINFOREPOSERV_H

#include <orbsvcs/Shutdown_Utilities.h>

#include <string>

#include "ace/Event_Handler.h"
#include "ace/Condition_Thread_Mutex.h"

#include "tao/ORB_Core.h"

#include "DCPSInfoRepoServ_Export.h"
#include "FederatorConfig.h"
#include "FederatorManagerImpl.h"
#include "ShutdownInterface.h"

class OpenDDS_DCPSInfoRepoServ_Export InfoRepo :
    public ShutdownInterface, public ACE_Event_Handler {
public:
  struct InitError
  {
    InitError (const char* msg)
      : msg_(msg) {};
    std::string msg_;
  };

  InfoRepo (int argc, ACE_TCHAR *argv[]);
  ~InfoRepo (void);
  void run (void);

  /// ShutdownInterface used to schedule a shutdown.
  virtual void shutdown (void);

  /// shutdown() and wait for it to complete: cannot be called from the reactor
  /// thread.
  void sync_shutdown (void);

  /// Handler for the reactor to dispatch finalization activity to.
  virtual int handle_exception( ACE_HANDLE fd = ACE_INVALID_HANDLE);

private:
  void init ();
  void usage (const ACE_TCHAR * cmd);
  void parse_args (int argc, ACE_TCHAR *argv[]);

  /// Actual finalization of service resources.
  void finalize();

  CORBA::ORB_var orb_;
  PortableServer::POA_var root_poa_;
  PortableServer::POA_var info_poa_;
  PortableServer::POAManager_var poa_manager_;

  ACE_TString ior_file_;
  ACE_TString listen_address_str_;
  int listen_address_given_;
  bool use_bits_;
  bool resurrect_;

  /// Flag to indicate that finalization has already occurred.
  bool finalized_;

  /// Repository Federation behaviors
  OpenDDS::Federator::ManagerImpl federator_;
  OpenDDS::Federator::Config      federatorConfig_;

  PortableServer::ServantBase_var info_;
  ACE_Thread_Mutex lock_;
  ACE_Condition_Thread_Mutex cond_;
  bool shutdown_complete_;
};

class OpenDDS_DCPSInfoRepoServ_Export InfoRepo_Shutdown :
    public Shutdown_Functor
{
public:
  InfoRepo_Shutdown(InfoRepo& ir);

  void operator() (int which_signal);
private:
  InfoRepo& ir_;
};

#endif  /* _DCPSINFOREPOSERV_H */
