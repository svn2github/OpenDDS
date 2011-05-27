// -*- C++ -*-
// ============================================================================
/**
 *  @file   DataBlockLockPool.h
 *
 *  $Id$
 *
 *
 */
// ============================================================================

#ifndef DATABLOCKLOCKPOOL_H
#define DATABLOCKLOCKPOOL_H

#include "ace/Lock_Adapter_T.h"
#include "ace/Thread_Mutex.h"
#include "ace/Containers_T.h"
#include "dds/DCPS/dcps_export.h"

/**
 * @class DataBlockLockPool
 *
 * @brief Holds and distributes locks to be used for locking 
 * ACE_Data_Blocks.  Currently it does not require returning the
 * locks.
 *
 * @NOTE: The lock returned is not guaranteed to be unique.
 *
 * @NOTE: This class is NOT thread safe.
 */
class TAO_DdsDcps_Export DataBlockLockPool
{
public:
  typedef ACE_Lock_Adapter<ACE_Thread_Mutex> DataBlockLock;

  DataBlockLockPool(size_t size);
  virtual ~DataBlockLockPool();

  DataBlockLock * get_lock ();
  void return_lock (DataBlockLock * lock);

private:
  typedef ACE_Array<DataBlockLock> Pool;

  Pool   pool_;
  size_t size_;
  /// Used to track which lock to give out next.
  size_t iterator_;
};

#endif /* DATABLOCKLOCKPOOL_H  */
