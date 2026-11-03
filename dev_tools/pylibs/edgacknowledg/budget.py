# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

'''A size budget shared by readers that count bytes or characters.'''


class BudgetExceeded(Exception):
  '''Raised when an operation would spend more than its budget allows.'''


class Budget:
  def __init__(self, limit: int) -> None:
    if limit < 0:
      raise ValueError('budget limit must be nonnegative')
    self.limit = limit
    self.remaining = limit
    self.exceeded = False

  def spend(self, count: int) -> None:
    if count < 0:
      raise ValueError('budget count must be nonnegative')
    if count > self.remaining:
      self.exceeded = True
      raise BudgetExceeded()
    self.remaining -= count
