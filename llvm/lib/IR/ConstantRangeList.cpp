//===- ConstantRangeList.cpp - ConstantRangeList implementation -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
#include "llvm/IR/ConstantRangeList.h"
#include <cstddef>

using namespace llvm;

// [0, 0)     = {}       = Empty set
// [255, 255) = {0..255} = Full Set
ConstantRangeList::ConstantRangeList(uint32_t BitWidth, bool Full) {
  APInt Lower =
      Full ? APInt::getMaxValue(BitWidth) : APInt::getMinValue(BitWidth);
  Ranges.push_back(ConstantRange(Lower, Lower));
}

ConstantRangeList::ConstantRangeList(int64_t Lower, int64_t Upper) {
  Ranges.push_back(ConstantRange(APInt(64, StringRef(std::to_string(Lower)), 10),
                                 APInt(64, StringRef(std::to_string(Upper)), 10)));
}

ConstantRangeList ConstantRangeList::inverse() const {
  if (isEmptySet()) return getFull(getBitWidth());
  if (isFullSet()) return getEmpty(getBitWidth());
  ConstantRangeList Result(getBitWidth(), false);

  APInt Start = APInt::getSignedMinValue(getBitWidth());
  for (size_t i = 0; i < size(); ++i) {
    if (Ranges[i].getLower().eq(Start)) {
      Start = Ranges[i].getUpper();
      continue;
    }
    Result.append(ConstantRange(Start, Ranges[i].getLower()));
    Start = Ranges[i].getUpper();
    if (i == size() - 1 && !Start.eq(APInt::getSignedMaxValue(getBitWidth()))) {
      Result.append(ConstantRange(Start, APInt::getSignedMaxValue(getBitWidth())));
    }
  }
  return Result;
}

ConstantRangeList ConstantRangeList::intersectWith(const ConstantRangeList &CRL) const {
  assert(getBitWidth() == CRL.getBitWidth() &&
         "ConstantRangeList types don't agree!");

  // Handle common cases.
  if (isEmptySet() || CRL.isFullSet()) return *this;
  if (CRL.isEmptySet() || isFullSet()) return CRL;

  // Intersect two range lists.
  ConstantRangeList Result(getBitWidth(), false);
  size_t i = 0, j = 0;
  while (i < size() && j < CRL.size()) {
    auto &Range = this->Ranges[i];
    auto &OtherRange = CRL.Ranges[j];
    assert(!Range.isSignWrappedSet() && !OtherRange.isSignWrappedSet() &&
          "Upper wrapped ranges are not supported");

    APInt Start = Range.getLower().slt(OtherRange.getLower()) ?
        OtherRange.getLower() : Range.getLower();
    APInt End = Range.getUpper().slt(OtherRange.getUpper()) ?
        Range.getUpper() : OtherRange.getUpper();
    if (Start.slt(End))
      Result.append(ConstantRange(Start, End));

    if (Range.getUpper().slt(OtherRange.getUpper()))
      i++;
    else
      j++;
  }
  return Result;
}

ConstantRangeList ConstantRangeList::unionWith(const ConstantRangeList &CRL) const {
  assert(getBitWidth() == CRL.getBitWidth() &&
         "ConstantRangeList types don't agree!");

  // Handle common cases.
  if (isEmptySet() || CRL.isFullSet()) return CRL;
  if (CRL.isEmptySet() || isFullSet()) return *this;

  ConstantRangeList Result(getBitWidth(), false);
  size_t i = 0, j =0;
  ConstantRange PreviousRange(getBitWidth(), false);
  if (Ranges[i].getLower().slt(CRL.Ranges[j].getLower())) {
    PreviousRange = Ranges[i++];
  } else {
    PreviousRange = CRL.Ranges[j++];
  }

  auto UnionAndUpdateRange = [&PreviousRange, &Result](const ConstantRange &CR) {
    assert(!CR.isSignWrappedSet() && "Upper wrapped ranges are not supported");
    if (PreviousRange.getUpper().slt(CR.getLower())) {
      Result.append(PreviousRange);
      PreviousRange = CR;
    } else {
      PreviousRange = ConstantRange(PreviousRange.getLower(), CR.getUpper());
    }
  };

  while (i < size() || j < CRL.size()) {
    if (j == CRL.size() || (i < size() &&
                            Ranges[i].getLower().slt(CRL.Ranges[j].getLower()))) {
      // Merge PreviousRange with this.
      UnionAndUpdateRange(Ranges[i++]);
    } else {
      // Merge PreviousRange with CRL.
      UnionAndUpdateRange(CRL.Ranges[j++]);
    }
  }
  Result.append(PreviousRange);
  return Result;
}

ConstantRangeList ConstantRangeList::subtractWith(const ConstantRangeList &CRL) const {
  assert(getBitWidth() == CRL.getBitWidth() &&
         "ConstantRangeList types don't agree!");

  // Handle common cases.
  if (isEmptySet() || CRL.isFullSet()) return getEmpty(getBitWidth());
  if (CRL.isEmptySet()) return *this;
  if (isFullSet()) return CRL.inverse();
  return intersectWith(CRL.inverse());
}

void ConstantRangeList::print(raw_ostream &OS, std::string prefix) const {
  OS << prefix;
  if (isFullSet())
    OS << "full-set";
  else if (isEmptySet())
    OS << "empty-set";
  else
  for (const auto &Range : Ranges)
    Range.print(OS);
}
