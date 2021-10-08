// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_FORMAT_H_
#define STORAGE_LEVELDB_DB_FORMAT_H_

#include <stdio.h>
#include "unikv/comparator.h"
#include "unikv/db.h"
#include "unikv/filter_policy.h"
#include "unikv/slice.h"
#include "unikv/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

typedef unsigned char byte;

namespace leveldb {

// Grouping of constants.  We may want to make some of these
// parameters set via options.
namespace config {
  
static const int kNumPartition =50;//
static const long long int kSplitBytes=43999672960;//49999672960;
static const long long int kGCBytes=43999672960;//49999672960;
static const long long int kcontinueWriteBytes=25474836480;

static const int kNumLevels =2;//
static const int kTempLevel =0;//
static const int kL0_CompactionTrigger =80;//66 NOTE:kL0_CompactionTrigger*64MB< kSplitBytes
static const int kL0_SlowdownWritesTrigger =1.5*kL0_CompactionTrigger;//
static const int kL0_StopWritesTrigger =1.5*kL0_CompactionTrigger;//

static const int bucketNum=4000000;//
static const int cuckooHashNum=4;
static const int logFileNum=200;//
static const int maxThreadNum=30;
static const int maxScanThread=50;//50
//static const int file_size=66624;//
static const int triggerSizeBasedMerge=5;
static const bool seekPrefetch=true;
static const int pointerSize=10;
static const int minValueSize=100;
static const int maxValueSize=66536;
static const int maxScanLength=1000;

//static const int limitSacnFiles=8;
static const int checkPointInterval=100;
static const int kKeyLength =24;
//static const int  MaxTableNum=10;//
static const int  baseRange=1100000;//no use

static const char* B_TreeDir="../persitentIndexDir";
static const char* B_TreeFile="../persitentIndexDir/B_TreeStore.txt";
static const char* HashTableDir="../persitentIndexDir";
static const char* HashTableFile="../persitentIndexDir/hashTableStore.txt";


// Maximum level to which a new compacted memtable is pushed if it
// does not create overlap.  We try to push to level 2 to avoid the
// relatively expensive level 0=>1 compactions and to avoid some
// expensive manifest file operations.  We do not push all the way to
// the largest level since that can generate a lot of wasted disk
// space if the same key space is being repeatedly overwritten.
static const unsigned kMaxMemCompactLevel = 1;//////////////////////////2

// Approximate gap in bytes between samples of data read during iteration.
static const unsigned kReadBytesPeriod = 1048576;

}  // namespace config

class InternalKey;

// Value types encoded as the last component of internal keys.
// DO NOT CHANGE THESE ENUM VALUES: they are embedded in the on-disk
// data structures.
enum ValueType {
  kTypeDeletion = 0x0,
  kTypeValue = 0x1
};
// kValueTypeForSeek defines the ValueType that should be passed when
// constructing a ParsedInternalKey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// ValueType, not the lowest).
static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;

// We leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
static const SequenceNumber kMaxSequenceNumber =
    ((0x1ull << 56) - 1);

struct ParsedInternalKey {
  Slice user_key;
  SequenceNumber sequence;
  ValueType type;

  ParsedInternalKey()
    : user_key(),
      sequence(),
      type() { }
  ParsedInternalKey(const Slice& u, const SequenceNumber& seq, ValueType t)
      : user_key(u), sequence(seq), type(t) { }
  std::string DebugString() const;
};

struct CuckooIndexEntry{
    byte KeyTag[2];
    byte TableNum[2];
    //byte TableNum[3];
    //IndexEntry* nextEntry;
};

struct ListIndexEntry{
    byte KeyTag[2];
    byte TableNum[2];
    //byte TableNum[3];
    ListIndexEntry* nextEntry;
};

// Return the length of the encoding of "key".
inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key) {
  return key.user_key.size() + 8;
}


void  intTo4Byte(unsigned int i,byte *bytes);
void  intTo3Byte(unsigned int i,byte *bytes);
void  intTo2Byte(unsigned int i,byte *bytes);
void  intToByte(unsigned int i,byte *bytes);
unsigned int bytesToInt(byte* bytes) ;
unsigned int bytes2ToInt(byte* bytes) ;
unsigned int bytes3ToInt(byte* bytes) ;
unsigned int bytes4ToInt(byte* bytes) ;

// Append the serialization of "key" to *result.
extern void AppendInternalKey(std::string* result,
                              const ParsedInternalKey& key);

// Attempt to parse an internal key from "internal_key".  On success,
// stores the parsed data in "*result", and returns true.
//
// On error, returns false, leaves "*result" in an undefined state.
extern bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result);

// Returns the user key portion of an internal key.
inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - 8);
}

inline ValueType ExtractValueType(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  const size_t n = internal_key.size();
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  return static_cast<ValueType>(c);
}

// A comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;
 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) { }
  InternalKeyComparator(const InternalKeyComparator& other)
    : user_comparator_(other.user_comparator_) {}
  virtual const char* Name() const;
  virtual int Compare(const Slice& a, const Slice& b) const;
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const;
  virtual void FindShortSuccessor(std::string* key) const;

  const Comparator* user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey& a, const InternalKey& b) const;

  InternalKeyComparator& operator = (const InternalKeyComparator& rhs)
  { user_comparator_ = rhs.user_comparator_; return *this; }
};

// Filter policy wrapper that converts from internal keys to user keys
class InternalFilterPolicy : public FilterPolicy {
 private:
  const FilterPolicy* const user_policy_;
  InternalFilterPolicy(const InternalFilterPolicy&);
  InternalFilterPolicy& operator = (const InternalFilterPolicy&);
 public:
  explicit InternalFilterPolicy(const FilterPolicy* p) : user_policy_(p) { }
  virtual const char* Name() const;
  virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const;
  virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const;
};

// Modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an InternalKeyComparator.
class InternalKey {
 private:
  std::string rep_;
 public:
  InternalKey() : rep_() { } // Leave rep_ as empty to indicate it is invalid
  InternalKey(const Slice& _user_key, SequenceNumber s, ValueType t) : rep_() {
    AppendInternalKey(&rep_, ParsedInternalKey(_user_key, s, t));
  }
  InternalKey(const InternalKey& other) : rep_(other.rep_) {}

  void DecodeFrom(const Slice& s) { rep_.assign(s.data(), s.size()); }
  Slice Encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  Slice user_key() const { return ExtractUserKey(rep_); }

  void SetFrom(const ParsedInternalKey& p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  InternalKey& operator = (const InternalKey& rhs)
  { if (this != &rhs) { rep_ = rhs.rep_; } return *this; }

  std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(
    const InternalKey& a, const InternalKey& b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  result->user_key = Slice(internal_key.data(), n - 8);
  return (c <= static_cast<unsigned char>(kTypeValue));
}

// A helper class useful for DBImpl::Get()
class LookupKey {
 public:
  // Initialize *this for looking up user_key at a snapshot with
  // the specified sequence number.
  LookupKey(const Slice& user_key, SequenceNumber sequence);

  ~LookupKey();

  // Return a key suitable for lookup in a MemTable.
  Slice memtable_key() const { return Slice(start_, end_ - start_); }

  // Return an internal key (suitable for passing to an internal iterator)
  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

  // Return the user key
  Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

 private:
  // We construct a char array of the form:
  //    klength  varint32               <-- start_
  //    userkey  char[klength]          <-- kstart_
  //    tag      uint64
  //                                    <-- end_
  // The array is a suitable MemTable key.
  // The suffix starting with "userkey" can be used as an InternalKey.
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];      // Avoid allocation for short keys

  // No copying allowed
  LookupKey(const LookupKey&);
  void operator=(const LookupKey&);
};

inline LookupKey::~LookupKey() {
  if (start_ != space_) delete[] start_;
}

struct partitionInfo{
    //std::string smallestKey;
//    unsigned int smallestKey;//uint64_t
    //InternalKey smallestCharKey; 
    char smallestCharKey[100];
    uint32_t partitionID;
    uint32_t nextID;
  };

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_FORMAT_H_
