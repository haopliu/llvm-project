//===- ConstantRangeList.cpp - ConstantRangeList implementation -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Represent a list of signed ConstantRange and do NOT support wrap around the
// end of the numeric range. Ranges in the list are ordered and no overlapping.
// Ranges should have the same bitwidth. Each range's lower should be less than
// its upper.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/ConstantRangeList.h"
#include <cstddef>

using namespace llvm;

void ConstantRangeList::insert(const ConstantRange &NewRange) {
  if (NewRange.isEmptySet())
    return;
  assert(!NewRange.isFullSet() && "Do not support full set");
  assert(NewRange.getLower().slt(NewRange.getUpper()));
  assert(getBitWidth() == NewRange.getBitWidth());
  // Handle common cases.
  if (empty() || Ranges.back().getUpper().slt(NewRange.getLower())) {
    Ranges.push_back(NewRange);
    return;
  }
  if (NewRange.getUpper().slt(Ranges.front().getLower())) {
    Ranges.insert(Ranges.begin(), NewRange);
    return;
  }

  // Slow insert.
  SmallVector<ConstantRange, 2> ExistingRanges(Ranges.begin(), Ranges.end());
  auto LowerBound =
      std::lower_bound(ExistingRanges.begin(), ExistingRanges.end(), NewRange,
                       [](const ConstantRange &a, const ConstantRange &b) {
                         return a.getLower().slt(b.getLower());
                       });
  Ranges.erase(Ranges.begin() + (LowerBound - ExistingRanges.begin()),
               Ranges.end());
  if (!Ranges.empty() && NewRange.getLower().slt(Ranges.back().getUpper())) {
    APInt NewLower = Ranges.back().getLower();
    APInt NewUpper =
        APIntOps::smax(NewRange.getUpper(), Ranges.back().getUpper());
    Ranges.back() = ConstantRange(NewLower, NewUpper);
  } else {
    Ranges.push_back(NewRange);
  }
  for (auto Iter = LowerBound; Iter != ExistingRanges.end(); Iter++) {
    if (Ranges.back().getUpper().slt(Iter->getLower())) {
      Ranges.push_back(*Iter);
    } else {
      APInt NewLower = Ranges.back().getLower();
      APInt NewUpper =
          APIntOps::smax(Iter->getUpper(), Ranges.back().getUpper());
      Ranges.back() = ConstantRange(NewLower, NewUpper);
    }
  }
  return;
}

ConstantRangeList
ConstantRangeList::unionWith(const ConstantRangeList &CRL) const {
  assert(getBitWidth() == CRL.getBitWidth() &&
         "ConstantRangeList types don't agree!");
  // Handle common cases.
  if (empty())
    return CRL;
  if (CRL.empty())
    return *this;

  ConstantRangeList Result;
  size_t i = 0, j = 0;
  ConstantRange PreviousRange(getBitWidth(), false);
  if (Ranges[i].getLower().slt(CRL.Ranges[j].getLower())) {
    PreviousRange = Ranges[i++];
  } else {
    PreviousRange = CRL.Ranges[j++];
  }
  auto UnionAndUpdateRange = [&PreviousRange,
                              &Result](const ConstantRange &CR) {
    assert(!CR.isSignWrappedSet() && "Upper wrapped ranges are not supported");
    if (PreviousRange.getUpper().slt(CR.getLower())) {
      Result.append(PreviousRange);
      PreviousRange = CR;
    } else {
      PreviousRange = ConstantRange(PreviousRange.getLower(), CR.getUpper());
    }
  };
  while (i < size() || j < CRL.size()) {
    if (j == CRL.size() ||
        (i < size() && Ranges[i].getLower().slt(CRL.Ranges[j].getLower()))) {
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

void ConstantRangeList::print(raw_ostream &OS) const {
  for (const auto &Range : Ranges)
    Range.print(OS);
}
