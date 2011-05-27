// -*- C++ -*-
// ============================================================================
/**
 *  @file   system.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================

#include "TestSystem.h"
#include "TestException.h"
#include "tao/Exception.h"
#include "dds/DCPS/debug.h"

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  OpenDDS::DCPS::set_DCPS_debug_level( 0);
  int status = 0;
  try {
    TestSystem system( argc, argv, 0);
    system.run();

  } catch (const CORBA::Exception& ex) {
    ex._tao_print_exception ("%T (%P|%t) ABORT: CORBA problem detected.\n");
    status = -1;

  } catch (const std::exception& ex) {
    ACE_ERROR ((LM_ERROR,
                ACE_TEXT("%T (%P|%t) ABORT: %C exception caught in main().\n"), ex.what()));
    status = -2;

  } catch(...) {
    ACE_ERROR ((LM_ERROR,
                ACE_TEXT("%T (%P|%t) ABORT: Unspecified exception caught in main() - panic.\n")));
    status = -3;

  }

  return status;
}

