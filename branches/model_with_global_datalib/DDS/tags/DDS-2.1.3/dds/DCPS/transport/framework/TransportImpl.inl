/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "TransportInterface.h"
#include "TransportConfiguration.h"
#include "TransportReactorTask.h"
#include "DataLink_rch.h"
#include "DataLink.h"
#include "EntryExit.h"

ACE_INLINE
OpenDDS::DCPS::TransportConfiguration*
OpenDDS::DCPS::TransportImpl::config() const
{
  return this->config_.in();
}

/// NOTE: Should only be called if this->lock_ has already been acquired.
//MJM: I am not convinced that this needs to be guarded by the caller.
//MJM: He gets a current snapshot of the value.  If it changes, his copy
//MJM: is stale.  Or do you mean that his stale copy may be stopped if
//MJM: his _use_ of the reactor task is not guarded?
ACE_INLINE OpenDDS::DCPS::TransportReactorTask*
OpenDDS::DCPS::TransportImpl::reactor_task()
{
  DBG_ENTRY_LVL("TransportImpl","reactor_task",6);
  TransportReactorTask_rch task = this->reactor_task_;
  return task._retn();
}

ACE_INLINE int
OpenDDS::DCPS::TransportImpl::set_reactor(TransportReactorTask* task)
{
  DBG_ENTRY_LVL("TransportImpl","set_reactor",6);

  GuardType guard(this->lock_);

  if (!this->reactor_task_.is_nil()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl already has a reactor.\n"),
                     -1);
  }

  // Keep a copy for ourselves.
  task->_add_ref();
  this->reactor_task_ = task;

  return 0;
}

/// The DataLink itself calls this when it has determined that, due
/// to some remove_associations() call being handled by a TransportInterface
/// object, the DataLink has lost all of its associations, and is not needed
/// any longer.
ACE_INLINE void
OpenDDS::DCPS::TransportImpl::release_datalink(DataLink* link, bool release_pending)
{
  DBG_ENTRY_LVL("TransportImpl","release_datalink",6);

  // Delegate to our subclass.
  this->release_datalink_i(link, release_pending);
}

/// This is called by a TransportInterface object when it is handling
/// its own request to detach_transport(), and this TransportImpl object
/// is the one to which it is currently attached.
ACE_INLINE void
OpenDDS::DCPS::TransportImpl::detach_interface(TransportInterface* transport_interface)
{
  DBG_ENTRY_LVL("TransportImpl","detach_interface",6);

  GuardType guard(this->lock_);

  // We really don't care if this unbind "works" or not.  As long as we
  // don't have the interface pointer in our interfaces_ collection, then
  // we are happy.
  unbind(interfaces_, transport_interface);
}

ACE_INLINE OpenDDS::DCPS::TransportImpl::ReservationLockType&
OpenDDS::DCPS::TransportImpl::reservation_lock()
{
  DBG_ENTRY_LVL("TransportImpl","reservation_lock",6);
  return this->reservation_lock_;
}

ACE_INLINE const OpenDDS::DCPS::TransportImpl::ReservationLockType&
OpenDDS::DCPS::TransportImpl::reservation_lock() const
{
  DBG_ENTRY_LVL("TransportImpl","reservation_lock",6);
  return this->reservation_lock_;
}

/// Note that this will return -1 if the TransportImpl has not been
/// configure()'d yet.
ACE_INLINE int
OpenDDS::DCPS::TransportImpl::connection_info
(TransportInterfaceInfo& local_info) const
{
  DBG_ENTRY_LVL("TransportImpl","connection_info",6);

  GuardType guard(this->lock_);

  if (this->config_.is_nil()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl cannot populate "
                      "connection_info - TransportImpl has not "
                      "been configure()'d.\n"),
                     -1);
  }

  // Delegate to our subclass.
  return this->connection_info_i(local_info);
}

/// Note that this will return -1 if the TransportImpl has not been
/// configure()'d yet.
ACE_INLINE int
OpenDDS::DCPS::TransportImpl::swap_bytes() const
{
  DBG_ENTRY_LVL("TransportImpl","swap_bytes",6);

  GuardType guard(this->lock_);

  if (this->config_.is_nil()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: TransportImpl cannot return swap_bytes "
                      "value - TransportImpl has not been configure()'d.\n"),
                     -1);
  }

  return this->config_->swap_bytes_;
}

ACE_INLINE void
OpenDDS::DCPS::TransportImpl::pre_shutdown_i()
{
  //noop
}
