#include "SubDriver.h"
#include "TestException.h"
#include "ace/Log_Msg.h"

#include "dds/DCPS/transport/simpleTCP/SimpleTcp.h"
#include "dds/DCPS/transport/framework/EntryExit.h"


int
main(int argc, char* argv[])
{
  // Need call the ORB_init to dynamically load the transport libs.
  CORBA::ORB_var orb = CORBA::ORB_init (argc,
                                        argv,
                                        "DDS_DCPS");

  DBG_ENTRY("sub_main.cpp","main");

  ACE_LOG_MSG->priority_mask(LM_TRACE     |
                             LM_DEBUG     |
                             LM_INFO      |
                             LM_NOTICE    |
                             LM_WARNING   |
                             LM_ERROR     |
                             LM_CRITICAL  |
                             LM_ALERT     |
                             LM_EMERGENCY,
                             ACE_Log_Msg::PROCESS);
  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "Create the SubDriver object.\n"));

  SubDriver driver;

  try
  {
    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
               "Tell the SubDriver object to run(argc,argv).\n"));

    driver.run(argc, argv);

    VDBG((LM_DEBUG, "(%P|%t) DBG:   "
               "SubDriver object has completed running.  "
               "Exit process with success code (0).\n"));

    return 0;
  }
  catch (const TestException&)
  {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) SubDriver TestException.\n"));
  }
  catch (...)
  {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) SubDriver unknown (...) exception.\n"));
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "SubDriver object has completed running due to an exception.  "
             "Exit process with failure code (1).\n"));

  return 1;
}
