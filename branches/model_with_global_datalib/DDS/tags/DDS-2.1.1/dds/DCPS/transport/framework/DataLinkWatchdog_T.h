/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_DATALINKWATCHDOG_T_H
#define DCPS_DATALINKWATCHDOG_T_H

#include "ace/Global_Macros.h"
#include "ace/Event_Handler.h"
#include "ace/Log_Msg.h"
#include "ace/Mutex.h"
#include "ace/Reactor.h"
#include "ace/Time_Value.h"

#include "ace/OS_NS_time.h"

namespace OpenDDS {
namespace DCPS {

template<typename ACE_LOCK>
class DataLinkWatchdog : public ACE_Event_Handler {
public:
  virtual ~DataLinkWatchdog() {
    cancel();
  }

  bool schedule(ACE_Reactor* reactor, const void* arg = 0) {
    ACE_GUARD_RETURN(ACE_LOCK,
                     guard,
                     this->lock_,
                     false);

    return schedule_i(reactor, arg, false);
  }

  bool schedule_now(ACE_Reactor* reactor, const void* arg = 0) {
    ACE_GUARD_RETURN(ACE_LOCK,
                     guard,
                     this->lock_,
                     false);

    return schedule_i(reactor, arg, true);
  }

  void cancel() {
    ACE_GUARD(ACE_LOCK,
              guard,
              this->lock_);

    if (this->timer_id_ == -1) return;

    this->reactor_->cancel_timer(this->timer_id_);

    this->timer_id_ = -1;
    this->reactor_ = 0;

    this->cancelled_ = true;
  }

  int handle_timeout(const ACE_Time_Value& now, const void* arg) {
    ACE_Time_Value timeout = next_timeout();

    if (timeout != ACE_Time_Value::zero) {
      timeout += this->epoch_;
      if (now > timeout) {
        on_timeout(arg);
        cancel();
        return 0;
      }
    }

    on_interval(arg);

    if (!schedule(reactor(), arg)) {
      ACE_ERROR((LM_WARNING,
                 ACE_TEXT("(%P|%t) WARNING: ")
                 ACE_TEXT("DataLinkWatchdog::handle_timeout: ")
                 ACE_TEXT("unable to reschedule watchdog timer!\n")));
    }

    return 0;
  }

protected:
  DataLinkWatchdog()
    : reactor_(0),
      timer_id_(-1),
      cancelled_(false)
  {}

  virtual ACE_Time_Value next_interval() = 0;
  virtual void on_interval(const void* arg) = 0;

  virtual ACE_Time_Value next_timeout() { return ACE_Time_Value::zero; }
  virtual void on_timeout(const void* /*arg*/) {}

private:
  ACE_LOCK lock_;

  ACE_Reactor* reactor_;
  long timer_id_;

  ACE_Time_Value epoch_;
  bool cancelled_;

  bool schedule_i(ACE_Reactor* reactor, const void* arg, bool nodelay) {
    if (this->cancelled_) return true;

    ACE_Time_Value delay;
    if (!nodelay) delay = next_interval();

    if (this->epoch_ == ACE_Time_Value::zero) {
      this->epoch_ = ACE_OS::gettimeofday();
    }

    this->reactor_ = reactor;
    this->timer_id_ = reactor->schedule_timer(this,  // event_handler
                                              arg,
                                              delay);
    return this->timer_id_ != -1;
  }
};

} // namespace DCPS
} // namespace OpenDDS

#endif  /* DCPS_DATALINKWATCHDOG_T_H */
