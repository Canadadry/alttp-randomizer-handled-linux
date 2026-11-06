"""Usage: python3 -I test_push_gate.py

Unit tests of push_gate.py: real git pushes through the installed chain in planted repositories,
with a planted synthetic ROM and a planted link; never the real repository's hooks.
"""

import contextlib
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("push_gate", os.path.join(HERE, "push_gate.py"))
push_gate = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(push_gate)


ROM = push_gate.planted_rom()
LINK = push_gate.PLANTED_LINK


class SelfProof(unittest.TestCase):
    def test_the_self_proof_passes(self):
        self.assertEqual(push_gate.self_test(), [])


class Pushes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.repo = push_gate.PlantedRepo(cls.tmp.name, ROM)
        cls.count = 0

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def push(self, setup, unset_link=False):
        Pushes.count += 1
        status, output = self.repo.push(f"case/t{Pushes.count}", setup, unset_link)
        self.assertNotIn(LINK, output)
        return status, output

    def test_a_clean_push_passes_and_says_so(self):
        status, output = self.push(lambda r: r.commit("clean", [("a.txt", "text\n")]))
        self.assertEqual(status, 0, output)
        self.assertIn("push-gate: ok, 1 commits, 1 blobs, no link, 0 literal runs, cleared 0 "
                      "trivial, 0 low-info, 0 ladxr-public, 0 computed, no stray untracked file",
                      output)

    def test_cleared_runs_pass_and_are_counted(self):
        text = push_gate.planted_text(ROM)
        status, output = self.push(lambda r: r.commit("clean", [
            ("notes/a.md", b"[" + text[20:70] + b"]"),
            ("data/b.bin", ROM[0x2000:0x2020]),
            ("data/c.bin", ROM[0x2200:0x2220])]))
        self.assertEqual(status, 0, output)
        self.assertIn("0 literal runs, cleared 1 trivial, 1 low-info, 1 ladxr-public", output)

    def test_a_computed_run_of_a_write_log_passes_and_is_counted(self):
        log = push_gate._planted_log("maker", ROM[0x1080:0x1088])
        status, output = self.push(lambda r: r.commit("log", [
            ("tests/ladx/combos/c/rom_writes.txt", log)]))
        self.assertEqual(status, 0, output)
        self.assertIn("0 literal runs, cleared 0 trivial, 0 low-info, 0 ladxr-public, 1 computed",
                      output)

    def test_a_write_log_needing_a_missing_inventory_is_refused(self):
        log = push_gate._planted_log("maker", ROM[0x1080:0x1088])
        status, output = self.push(lambda r: r.commit("log", [
            ("tests/ladx/cases/c/rom_writes.txt", log)], removed=[push_gate.INVENTORY_PATH]))
        self.assertEqual(status, 1)
        self.assertRegex(output, r"push-gate: refused: tests/ladx/cases/c/rom_writes.txt \(blob "
                                 r"[0-9a-f]{12}\): setup: no inventory at ")

    def test_the_scope_follows_the_pushed_path(self):
        status, output = self.push(lambda r: r.commit("c", [("data/ladx/a.bin", ROM[300:305]),
                                                            ("data/a.bin", ROM[300:315])]))
        self.assertEqual(status, 1)
        self.assertRegex(output, r"push-gate: refused: data/ladx/a.bin \(blob [0-9a-f]{12}\): "
                                 r"literal run of 5 bytes at @0 \(raw, strict\)")
        self.assertNotIn("data/a.bin", output)

    def test_a_link_in_a_binary_file_is_refused(self):
        status, output = self.push(
            lambda r: r.commit("bin", [("x.bin", b"\0\0" + LINK.encode() + b"\0\xfe")]))
        self.assertEqual(status, 1)
        self.assertIn("push-gate: refused: the link is in commit ", output)

    def test_a_literal_only_in_an_intermediate_commit_is_refused(self):
        status, output = self.push(lambda r: (r.commit("add", [("data/ladx/s.bin", ROM[100:106])]),
                                              r.commit("drop", removed=["data/ladx/s.bin"])))
        self.assertEqual(status, 1)
        self.assertRegex(output, r"push-gate: refused: data/ladx/s.bin \(blob [0-9a-f]{12}\): "
                                 r"literal run of 6 bytes at @0 \(raw, strict\)")

    def test_a_literal_written_as_hex_is_refused(self):
        status, output = self.push(
            lambda r: r.commit("hex", [("t.txt", "db " + ROM[200:208].hex() + "\n")]))
        self.assertEqual(status, 1)
        self.assertIn("literal run of 8 bytes at @3 (hex, general)", output)
        self.assertNotIn(ROM[200:208].hex(), output)

    def test_untracked_files_under_docs_prd_pass_and_are_searched_for_the_link(self):
        status, output = self.push(
            lambda r: (r.commit("c"), r.write("docs/prd/triage/next.md", "# next\n")))
        self.assertEqual(status, 0, output)
        status, output = self.push(
            lambda r: (r.commit("c"), r.write("docs/prd/triage/next.md", "url " + LINK)))
        self.assertEqual(status, 1)
        self.assertIn("the link is in untracked file docs/prd/triage/next.md", output)

    def test_a_stray_untracked_file_is_refused(self):
        status, output = self.push(lambda r: (r.commit("c"), r.write("LADXR_7.gbc.log", "x")))
        self.assertEqual(status, 1)
        self.assertIn("push-gate: refused: untracked file LADXR_7.gbc.log", output)

    def test_no_link_in_the_environment_refuses(self):
        status, output = self.push(lambda r: r.commit("c"), unset_link=True)
        self.assertEqual(status, 1)
        self.assertIn("push-gate: setup: LADX_ROM_URL is not set, push refused", output)

    def test_a_rom_of_another_sha1_refuses(self):
        rom_file = os.path.join(self.repo.top, push_gate.ROM_PATH)
        try:
            self.repo.write(push_gate.ROM_PATH, ROM[:-1] + b"\0")
            status, output = self.push(lambda r: r.commit("c"))
        finally:
            self.repo.write(push_gate.ROM_PATH, ROM)
        self.assertEqual(status, 1)
        self.assertIn("push-gate: setup: no checked ROM in tmp/ladx/", output)
        os.remove(rom_file)
        try:
            status, output = self.push(lambda r: r.commit("c"))
        finally:
            self.repo.write(push_gate.ROM_PATH, ROM)
        self.assertEqual(status, 1)
        self.assertIn("push-gate: setup: no checked ROM in tmp/ladx/", output)

    def test_a_missing_or_unpinned_ladxr_checkout_refuses(self):
        moved = self.repo.ladxr + ".away"
        os.rename(self.repo.ladxr, moved)
        try:
            status, output = self.push(lambda r: r.commit("c"))
        finally:
            os.rename(moved, self.repo.ladxr)
        self.assertEqual(status, 1)
        self.assertIn("push-gate: setup: no LADXR checkout in ", output)
        self.assertIn(", push refused", output)
        git = lambda *args: subprocess.run(["git", *args], cwd=self.repo.ladxr, env=self.repo.env,
                                           check=True, capture_output=True)
        git("-c", "user.name=p", "-c", "user.email=p@example.invalid", "commit", "-q",
            "--allow-empty", "-m", "later")
        try:
            status, output = self.push(lambda r: r.commit("c"))
        finally:
            git("reset", "-q", "--hard", "HEAD~1")
        self.assertEqual(status, 1)
        self.assertRegex(output, r"push-gate: setup: LADXR checkout in .* not at the pin .*"
                                 r", push refused")

    def test_a_claude_branch_is_refused_by_the_chain_and_the_gate_still_reports(self):
        status, output = self.repo.push("claude/x", lambda r: r.commit("c", [("l.txt", LINK)]))
        self.assertEqual(status, 1)
        self.assertIn("claude/* branch", output)
        self.assertIn("push-gate: refused: the link is in commit ", output)
        self.assertNotIn(LINK, output)


class Install(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.repo = push_gate.PlantedRepo(self.tmp.name, ROM)
        self.hook = os.path.join(self.repo.top, ".git", "hooks", "pre-push")

    def tearDown(self):
        self.tmp.cleanup()

    def read(self, path):
        with open(path) as f:
            return f.read()

    def test_install_leaves_prepare_commit_msg_and_a_foreign_pre_push_alone(self):
        self.repo.write(self.hook, "#!/bin/sh\nexit 0\n")
        status, message = push_gate.install_chain(self.repo.top)
        self.assertEqual(status, 1)
        self.assertIn("the gate is NOT armed", message)
        self.assertEqual(self.read(self.hook), "#!/bin/sh\nexit 0\n")
        self.assertEqual(self.read(self.repo.commit_msg_hook), push_gate.PLANTED_COMMIT_MSG_HOOK)

    def test_install_replaces_no_claude_branch_hook_and_rewrites_the_chain(self):
        self.repo.write(self.hook, push_gate.PLAIN_HOOK)
        self.assertEqual(push_gate.install_chain(self.repo.top)[0], 0)
        self.assertEqual(self.read(self.hook), push_gate.CHAIN_HOOK)
        self.assertEqual(push_gate.install_chain(self.repo.top)[0], 0)
        self.assertEqual(self.read(self.hook), push_gate.CHAIN_HOOK)

    def test_uninstall_restores_no_claude_branch_hook(self):
        self.assertIn(push_gate.MARKER, self.read(self.hook))
        status, message = push_gate.uninstall_chain(self.repo.top)
        self.assertEqual(status, 0, message)
        self.assertEqual(self.read(self.hook), push_gate.PLAIN_HOOK)
        self.assertEqual(self.read(self.repo.commit_msg_hook), push_gate.PLANTED_COMMIT_MSG_HOOK)
        self.repo.git("checkout", "-q", "-b", "claude/y")
        result = self.repo.git("push", "origin", "claude/y", check=False)
        self.assertEqual(result.returncode, 1)

    def test_no_claude_branch_install_keeps_the_chain(self):
        subprocess.run(["sh", push_gate.BRANCH_HOOK_PATH, "--install"], cwd=self.repo.top,
                       env=self.repo.env, check=True, capture_output=True)
        self.assertEqual(self.read(self.hook), push_gate.CHAIN_HOOK)

    def test_plain_hook_is_what_no_claude_branch_writes(self):
        os.remove(self.hook)
        subprocess.run(["sh", push_gate.BRANCH_HOOK_PATH, "--install"], cwd=self.repo.top,
                       env=self.repo.env, check=True, capture_output=True)
        self.assertEqual(self.read(self.hook), push_gate.PLAIN_HOOK)

    def run_main(self, *args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = push_gate.main(list(args))
        return code, out.getvalue()

    def test_install_arms_only_after_its_self_proof(self):
        os.remove(self.hook)
        real_top, real_self_test = push_gate.TOP, push_gate.self_test
        push_gate.TOP = self.repo.top
        try:
            push_gate.self_test = lambda: ["push-gate: self-test failed: planted"]
            code, out = self.run_main("--install")
            self.assertEqual(code, 1)
            self.assertIn("the gate is NOT armed", out)
            self.assertFalse(os.path.exists(self.hook))
            push_gate.self_test = lambda: []
            code, out = self.run_main("--install")
            self.assertEqual(code, 0, out)
            self.assertEqual(self.read(self.hook), push_gate.CHAIN_HOOK)
        finally:
            push_gate.TOP, push_gate.self_test = real_top, real_self_test


if __name__ == "__main__":
    unittest.main()
