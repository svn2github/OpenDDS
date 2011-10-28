/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "TransportReassembly.h"
#include "TransportDebug.h"

namespace OpenDDS {
namespace DCPS {

TransportReassembly::FragRange::FragRange(const SequenceNumber& transportSeq,
                                          const ReceivedDataSample& data)
  : transport_seq_(transportSeq, transportSeq)
  , rec_ds_(data)
{
}

namespace {
  inline void join_err(const char* detail)
  {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: TransportReassembly::insert() - ")
      ACE_TEXT("DataSampleHeaders could not be joined: %C\n"), detail));
  }
}

bool
TransportReassembly::insert(std::list<FragRange>& flist,
                            const SequenceNumber& transportSeq,
                            ReceivedDataSample& data)
{
  using std::list;
  SequenceNumber next = transportSeq, prev = transportSeq.previous();
  ++next;

  for (list<FragRange>::iterator it = flist.begin(); it != flist.end(); ++it) {
    FragRange& fr = *it;
    if (next < fr.transport_seq_.first) {
      // insert before 'it'
      flist.insert(it, FragRange(transportSeq, data));
      VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::insert() "
        "inserted on left\n"));
      return true;

    } else if (next == fr.transport_seq_.first) {
      // combine on left of fr
      DataSampleHeader joined;
      if (!DataSampleHeader::join(data.header_, fr.rec_ds_.header_, joined)) {
        join_err("left");
        return false;
      }
      fr.rec_ds_.header_ = joined;
      if (fr.rec_ds_.sample_ && !data.sample_) {
        ACE_Message_Block::release(fr.rec_ds_.sample_);
        fr.rec_ds_.sample_ = 0;
      } else if (!fr.rec_ds_.sample_) {
        ACE_Message_Block::release(data.sample_);
      } else {
        ACE_Message_Block* last;
        for (last = data.sample_; last->cont(); last = last->cont()) ;
        last->cont(fr.rec_ds_.sample_);
        fr.rec_ds_.sample_ = data.sample_;
      }
      data.sample_ = 0;
      fr.transport_seq_.first = transportSeq;
      VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::insert() "
        "combined on left\n"));
      return true;

    } else if (prev == fr.transport_seq_.second) {
      // combine on right of fr
      DataSampleHeader joined;
      if (!DataSampleHeader::join(fr.rec_ds_.header_, data.header_, joined)) {
        join_err("right");
        return false;
      }
      fr.rec_ds_.header_ = joined;
      if (fr.rec_ds_.sample_ && !data.sample_) {
        ACE_Message_Block::release(fr.rec_ds_.sample_);
        fr.rec_ds_.sample_ = 0;
      } else if (!fr.rec_ds_.sample_) {
        ACE_Message_Block::release(data.sample_);
      } else {
        ACE_Message_Block* last;
        for (last = fr.rec_ds_.sample_; last->cont(); last = last->cont()) ;
        last->cont(data.sample_);
      }
      data.sample_ = 0;
      fr.transport_seq_.second = transportSeq;
      VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::insert() "
        "combined on right\n"));

      // check if the next FragRange in the list needs to be combined
      if (++it != flist.end()) {
        if (next == it->transport_seq_.first) {
          if (!DataSampleHeader::join(fr.rec_ds_.header_, it->rec_ds_.header_,
                                      joined)) {
            join_err("combined next");
            return false;
          }
          fr.rec_ds_.header_ = joined;
          if (!it->rec_ds_.sample_) {
            ACE_Message_Block::release(fr.rec_ds_.sample_);
            fr.rec_ds_.sample_ = 0;
          } else {
            ACE_Message_Block* last;
            for (last = fr.rec_ds_.sample_; last->cont(); last = last->cont()) ;
            last->cont(it->rec_ds_.sample_);
            it->rec_ds_.sample_ = 0;
          }
          fr.transport_seq_.second = it->transport_seq_.second;
          flist.erase(it);
          VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::insert() "
            "coalesced on right\n"));
        }
      }
      return true;
    }
  }

  // add to end of list
  flist.push_back(FragRange(transportSeq, data));
  VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::insert() "
    "inserted at end of list\n"));
  return true;
}

bool
TransportReassembly::reassemble(const SequenceNumber& transportSeq,
                                bool firstFrag,
                                ReceivedDataSample& data)
{
  VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::reassemble() "
    "tseq %q first %d dseq %q\n", transportSeq.getValue(), firstFrag ? 1 : 0,
    data.header_.sequence_.getValue()));

  if (firstFrag) {
    have_first_.insert(data.header_.sequence_);
  }

  FragMap::iterator iter = fragments_.find(data.header_.sequence_);
  if (iter == fragments_.end()) {
    fragments_[data.header_.sequence_].push_back(FragRange(transportSeq, data));
    // since this is the first fragment we've seen, it can't possibly be done
    VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::reassemble() "
      "stored first frag, returning 0 (incomplete)\n"));
    return false;
  }

  if (!insert(iter->second, transportSeq, data)) {
    // error condition, already logged by insert()
    return false;
  }

  // We can deliver data if all three of these conditions are met:
  // 1. we've seen the "first fragment" flag  [first frag is here]
  // 2. all fragments have been coalesced     [no gaps in the seq numbers]
  // 3. the "more fragments" flag is not set  [last frag is here]
  if (have_first_.count(data.header_.sequence_)
      && iter->second.size() == 1
      && !iter->second.front().rec_ds_.header_.more_fragments_) {
    swap(data, iter->second.front().rec_ds_);
    fragments_.erase(iter);
    have_first_.erase(data.header_.sequence_);
    VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::reassemble() "
      "removed frag, returning %d\n", data.sample_ ? 1 : 0));
    return data.sample_; // could be false if we had data_unavailable()
  }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::reassemble() "
    "returning 0 (incomplete)\n"));
  return false;
}

void
TransportReassembly::data_unavailable(const SequenceRange& dropped)
{
  for (SequenceNumber sn = dropped.second; sn >= dropped.first;
       sn = sn.previous()) {
    dropped_one(sn, dropped.first);
  }
}

void
TransportReassembly::dropped_one(const SequenceNumber& dropped,
                                 const SequenceNumber& first)
{
  VDBG((LM_DEBUG, "(%P|%t) DBG:   TransportReassembly::dropped_one() "
    "dropped %q first %q\n", dropped.getValue(), first.getValue()));
  using std::list;
  typedef list<FragRange>::iterator list_iterator;

  for (FragMap::iterator iter = fragments_.begin(); iter != fragments_.end();
       ++iter) {
    SequenceNumber dataSampleNumber = iter->first;
    list<FragRange>& flist = iter->second;

    ReceivedDataSample dummy;
    dummy.header_.sequence_ = dataSampleNumber;
    dummy.header_.more_fragments_ = true;

    // check if we should expand the front element (only if !have_first)
    const SequenceNumber prev = flist.front().transport_seq_.first.previous();
    if (dropped == prev && !have_first_.count(dataSampleNumber)) {
      have_first_.insert(dataSampleNumber);
      insert(flist, dropped, dummy);
      continue;
    }

    // find a gap between list elements where "dropped" fits
    for (list_iterator it = flist.begin(); it != flist.end(); ++it) {
      list_iterator it_next = it;
      ++it_next;
      if (it_next == flist.end()) {
        break;
      }
      FragRange& fr1 = *it;
      FragRange& fr2 = *it_next;
      if (dropped > fr1.transport_seq_.second
          && dropped < fr2.transport_seq_.first) {
        insert(flist, dropped, dummy);
        break;
      }
    }

    // check if we should expand the last element
    SequenceNumber next = flist.back().transport_seq_.second;
    ++next;
    if (first == next && flist.back().rec_ds_.header_.more_fragments_) {
      insert(flist, first, dummy);
    }
  }
}

}
}
