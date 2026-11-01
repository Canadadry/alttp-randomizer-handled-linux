"""Unit tests for progress_tidy.py, and for the tidy-up exception of
progress_check.sh (the progress.md hook). Host only: each hook test
makes its own git repository in a temporary directory.
Run: make test-template"""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import progress_tidy  # noqa: E402

CHECK = ROOT / "scripts" / "progress_check.sh"
TIDY = ROOT / "scripts" / "progress_tidy.py"

# A plan-pilot run's list once groups were appended at the end: chunks of
# 1.2 and 2.1, a step added after 1.1 (with its own chunks), two groups for
# the same step 1.2's chunks.
BEFORE = """\
- [x] Phase 1 — First
  - [x] 1.1 One
  - [x] 1.2 Two
    - [x] chunk 1: a
  - [x] 1.3 Three
- [ ] Phase 2 — Second
  - [ ] 2.1 Four
- [x] Added after step 1.1
  - [x] 1.1b Late
    - [x] chunk 1: x
- [x] Step 1.2 chunks
  - [x] chunk 2: b
- [ ] Step 2.1 chunks
  - [x] chunk 1: c
  - [ ] chunk 2: d
- [x] Step 1.2 chunks
  - [x] chunk 3: e
"""

AFTER = """\
- [x] Phase 1 — First
  - [x] 1.1 One
  - [x] 1.1b Late
    - [x] chunk 1: x
  - [x] 1.2 Two
    - [x] chunk 1: a
    - [x] chunk 2: b
    - [x] chunk 3: e
  - [x] 1.3 Three
- [ ] Phase 2 — Second
  - [ ] 2.1 Four
    - [x] chunk 1: c
    - [ ] chunk 2: d
"""


class TidyTest(unittest.TestCase):
    def test_groups_move_under_their_step(self):
        self.assertEqual(progress_tidy.tidy(BEFORE), AFTER)

    def test_tidy_list_is_unchanged(self):
        self.assertEqual(progress_tidy.tidy(AFTER), AFTER)

    def test_two_added_groups_keep_their_order(self):
        text = (
            "- [ ] P\n  - [x] 1.1 One\n  - [ ] 1.2 Two\n"
            "- [x] Added after step 1.1\n  - [x] 1.1b B\n"
            "- [ ] Added after step 1.1\n  - [ ] 1.1c C\n"
        )
        self.assertEqual(
            progress_tidy.tidy(text),
            "- [ ] P\n  - [x] 1.1 One\n  - [x] 1.1b B\n  - [ ] 1.1c C\n  - [ ] 1.2 Two\n",
        )

    def test_unknown_step_fails_and_leaves_the_file(self):
        text = "- [ ] P\n  - [ ] 1.1 One\n- [ ] Step 9.9 chunks\n  - [ ] chunk 1: a\n"
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "progress.md"
            path.write_text(text)
            result = subprocess.run(
                [sys.executable, str(TIDY), str(path)], capture_output=True, text=True
            )
            self.assertEqual(result.returncode, 1)
            self.assertIn("no step 9.9", result.stderr)
            self.assertEqual(path.read_text(), text)

    def test_rewrites_in_place(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "progress.md"
            path.write_text(BEFORE)
            subprocess.run([sys.executable, str(TIDY), str(path)], check=True)
            self.assertEqual(path.read_text(), AFTER)


class HookTidyExceptionTest(unittest.TestCase):
    """The hook accepts a commit whose only change is the tidy-up."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.repo = self.tmp.name
        env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
        self.env = env
        self.git("init", "-q")
        self.git("config", "user.email", "t@example.com")
        self.git("config", "user.name", "t")
        self.stage(BEFORE)
        self.git("commit", "-q", "-m", "base")

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, *args):
        subprocess.run(["git", *args], cwd=self.repo, env=self.env, check=True)

    def stage(self, text):
        Path(self.repo, "progress.md").write_text(text)
        self.git("add", "progress.md")

    def hook(self):
        return subprocess.run(
            ["sh", str(CHECK), ".git/COMMIT_EDITMSG", "message"],
            cwd=self.repo, env=self.env, capture_output=True, text=True,
        ).returncode

    def test_tidy_only_is_accepted(self):
        self.stage(AFTER)
        self.assertEqual(self.hook(), 0)

    def test_tidy_with_another_move_is_refused(self):
        lines = AFTER.splitlines(keepends=True)
        lines[1], lines[2] = lines[2], lines[1]
        self.stage("".join(lines))
        self.assertEqual(self.hook(), 1)

    def test_no_change_is_still_refused(self):
        self.assertEqual(self.hook(), 1)


if __name__ == "__main__":
    unittest.main()
