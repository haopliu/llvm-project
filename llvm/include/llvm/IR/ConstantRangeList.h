//===- ConstantRangeList.h - A list of constant range -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//

#ifndef LLVM_IR_CONSTANTRANGELIST_H
#define LLVM_IR_CONSTANTRANGELIST_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/Compiler.h"
#include <cstdint>

namespace llvm {

class raw_ostream;

/// This class represents a list of constant ranges.
class ConstantRangeList {
  SmallVector<ConstantRange, 2> Ranges;
  unsigned HashValue = 0;

  /// Create empty constant range with same bitwidth.
  ConstantRangeList getEmpty(uint32_t BitWidth) const {
    return ConstantRangeList(BitWidth, false);
  }

  /// Create full constant range with same bitwidth.
  ConstantRangeList getFull(uint32_t BitWidth) const {
    return ConstantRangeList(BitWidth, true);
  }

public:
  /// Initialize a full or empty set for the specified bit width.
  explicit ConstantRangeList(uint32_t BitWidth, bool isFullSet);

  ConstantRangeList(int64_t Lower, int64_t Upper);
  void computeHash() {
    llvm::FoldingSetNodeID ID;
    ID.AddInteger(Ranges.size());
    for (const auto &R : Ranges) {
      ID.AddInteger(R.getLower());
      ID.AddInteger(R.getUpper());
    }
    HashValue = ID.ComputeHash();
  }

  SmallVectorImpl<ConstantRange>::iterator begin() { return Ranges.begin(); }
  SmallVectorImpl<ConstantRange>::iterator end() { return Ranges.end(); }
  SmallVectorImpl<ConstantRange>::const_iterator begin() const {
    return Ranges.begin();
  }
  SmallVectorImpl<ConstantRange>::const_iterator end() const{
    return Ranges.end();
  }

  /// Return the range list that results from the intersection of this range
  /// with another range list.
  ConstantRangeList intersectWith(const ConstantRangeList &CRL) const;

  /// Return the range list that results from the union of this range
  /// with another range list.
  ConstantRangeList unionWith(const ConstantRangeList &CRL) const;

  /// Return a new range list that is the logical not of the current set.
  ConstantRangeList inverse() const;

  /// Return true if this set contains no members.
  bool isEmptySet() const {
    return Ranges.size() == 1 && Ranges[0].isEmptySet();
  }
  /// Return true if this set contains all of the elements possible
  /// for this data-type.
  bool isFullSet() const {
    return Ranges.size() == 1 && Ranges[0].isFullSet();
  }

  /// Return a new range list representing the values resulting from a
  /// subtraction of a value in this range and a value in another range list.
  ConstantRangeList subtractWith(const ConstantRangeList &CRL) const;

  /// Get the bit width of this ConstantRangeList.
  uint32_t getBitWidth() const { return Ranges[0].getBitWidth(); }

  size_t size() const { return Ranges.size(); }

  void append(const ConstantRange &Range) {
    if (isFullSet()) return;
    if (isEmptySet()) {
      Ranges[0] = Range;
      return;
    }

    Ranges.push_back(Range);
  }

  void append(APInt Lower, APInt Upper) {
    append(ConstantRange(Lower, Upper));
  }
  void append(int64_t Lower, int64_t Upper) {
    append(APInt(64, StringRef(std::to_string(Lower)), 10),
                         APInt(64, StringRef(std::to_string(Upper)), 10));
  }

  /// Return true if this range is equal to another range.
  bool operator==(const ConstantRangeList &CRL) const {
    /*if (size() != CRL.size()) return false;
    for (size_t i = 0; i < size(); ++i) {
      if (Ranges[i] != CRL.Ranges[i]) return false;
    }
    return true;*/
    return HashValue == CRL.HashValue;
  }
  bool operator!=(const ConstantRangeList &CRL) const {
    return !operator==(CRL);
  }

  /// Print out the bounds to a stream.
  void print(raw_ostream &OS, std::string prefix = "") const;
};

} // end namespace llvm

#endif // LLVM_IR_CONSTANTRANGELIST_H
