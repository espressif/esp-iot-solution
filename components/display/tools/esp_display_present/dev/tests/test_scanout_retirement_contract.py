# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Executable examples for the proposed driver retirement contract.

This is a model of the contract, not a test of the IDF driver or hardware.
"""
import unittest
from dataclasses import dataclass


@dataclass(frozen=True)
class Evidence:
    epoch: int
    generation: int
    retired: str
    selected: str


class RetirementModel:
    def __init__(self, active):
        self.active = active
        self.epoch = 1
        self.next_generation = 1
        self.pending = None
        self.early = None
        self.released = []
        self.faulted = False

    def submit(self, target):
        if self.pending or self.faulted or target == self.active:
            raise ValueError('invalid submission')
        generation = self.next_generation
        self.next_generation += 1
        self.pending = (self.epoch, generation, self.active, target, False)
        return generation

    def event(self, evidence):
        if not self.pending or self.faulted:
            return
        epoch, generation, old, new, accepted = self.pending
        if evidence != Evidence(epoch, generation, old, new):
            return
        if not accepted:
            self.early = evidence
            return
        self.active = new
        self.released.append(old)
        self.pending = None
        self.early = None

    def resolve(self, outcome):
        if not self.pending:
            raise ValueError('no submission')
        if outcome != 'accepted':
            # With a matching early event even rejection is contradictory:
            # do not infer that either buffer is safe to write.
            if outcome == 'uncertain' or self.early is not None:
                self.faulted = True
            else:
                self.pending = None
            return
        epoch, generation, old, new, _ = self.pending
        self.pending = (epoch, generation, old, new, True)
        if self.early:
            self.event(self.early)

    def restart(self):
        # Restart invalidates old evidence; it does not itself retire memory.
        self.epoch += 1
        self.early = None
        self.faulted = True


class RetirementContractTest(unittest.TestCase):
    def test_matching_event_retires_once(self):
        model = RetirementModel('A')
        gen = model.submit('B')
        model.resolve('accepted')
        event = Evidence(1, gen, 'A', 'B')
        model.event(event)
        model.event(event)
        self.assertEqual(model.released, ['A'])
        self.assertEqual(model.active, 'B')

    def test_early_event_waits_for_submit_resolution(self):
        model = RetirementModel('A')
        gen = model.submit('B')
        model.event(Evidence(1, gen, 'A', 'B'))
        self.assertEqual(model.released, [])
        model.resolve('accepted')
        self.assertEqual(model.released, ['A'])

    def test_old_wrong_and_future_events_do_not_retire(self):
        model = RetirementModel('A')
        gen = model.submit('B')
        model.resolve('accepted')
        for evidence in (Evidence(1, gen - 1, 'A', 'B'),
                         Evidence(1, gen + 1, 'A', 'B'),
                         Evidence(1, gen, 'B', 'A'),
                         Evidence(2, gen, 'A', 'B')):
            model.event(evidence)
        self.assertEqual(model.released, [])
        model.event(Evidence(1, gen, 'A', 'B'))
        self.assertEqual(model.released, ['A'])

    def test_aba_address_does_not_match_old_generation(self):
        model = RetirementModel('A')
        first = model.submit('B')
        model.resolve('accepted')
        model.event(Evidence(1, first, 'A', 'B'))
        second = model.submit('A')
        model.resolve('accepted')
        model.event(Evidence(1, second, 'B', 'A'))
        third = model.submit('B')
        model.resolve('accepted')
        model.event(Evidence(1, first, 'A', 'B'))
        self.assertEqual(model.released, ['A', 'B'])
        model.event(Evidence(1, third, 'A', 'B'))
        self.assertEqual(model.released, ['A', 'B', 'A'])

    def test_uncertain_and_restart_never_release(self):
        for action in ('uncertain', 'restart', 'contradictory_rejection'):
            with self.subTest(action=action):
                model = RetirementModel('A')
                gen = model.submit('B')
                event = Evidence(1, gen, 'A', 'B')
                if action == 'restart':
                    model.resolve('accepted')
                    model.restart()
                else:
                    if action == 'contradictory_rejection':
                        model.event(event)
                    model.resolve('rejected' if action == 'contradictory_rejection' else 'uncertain')
                model.event(event)
                self.assertEqual(model.released, [])
                self.assertTrue(model.faulted)

    def test_rejection_without_access_allows_retry(self):
        model = RetirementModel('A')
        first = model.submit('B')
        model.resolve('rejected')
        second = model.submit('B')
        self.assertGreater(second, first)
        model.resolve('accepted')
        model.event(Evidence(1, first, 'A', 'B'))
        self.assertEqual(model.released, [])
        model.event(Evidence(1, second, 'A', 'B'))
        self.assertEqual(model.released, ['A'])


if __name__ == '__main__':
    unittest.main()
