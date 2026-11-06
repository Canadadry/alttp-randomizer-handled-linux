"""Unit tests for no_claude_branch.sh, the pre-push hook refusing claude/*
branches. Host only: each test pushes between two repositories it makes in a
temporary directory. Run: make test-template"""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

HOOK = Path(__file__).resolve().parent.parent / "no_claude_branch.sh"


class PrePushTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
        self.remote = Path(self.tmp.name, "remote.git")
        self.repo = Path(self.tmp.name, "repo")
        subprocess.run(["git", "init", "-q", "--bare", str(self.remote)], env=self.env, check=True)
        subprocess.run(["git", "init", "-q", str(self.repo)], env=self.env, check=True)
        self.git("config", "user.email", "t@example.com")
        self.git("config", "user.name", "t")
        self.git("remote", "add", "origin", str(self.remote))
        self.git("commit", "-q", "--allow-empty", "-m", "base")
        hooks = self.repo / ".git" / "hooks"
        hooks.mkdir(exist_ok=True)
        (hooks / "pre-push").write_text(f'#!/bin/sh\nexec sh "{HOOK}" "$@"\n')
        (hooks / "pre-push").chmod(0o755)

    def tearDown(self):
        self.tmp.cleanup()

    def git(self, *args, check=True):
        return subprocess.run(
            ["git", *args], cwd=self.repo, env=self.env, check=check, capture_output=True, text=True
        )

    def push(self, refspec):
        return self.git("push", "-q", "origin", refspec, check=False)

    def test_named_branch_is_pushed(self):
        self.git("branch", "feat/import-csv")
        self.assertEqual(self.push("feat/import-csv").returncode, 0)

    def test_claude_branch_is_refused(self):
        self.git("branch", "claude/awesome-clarke")
        result = self.push("claude/awesome-clarke")
        self.assertEqual(result.returncode, 1)
        self.assertIn("claude/* branch", result.stderr)

    def test_named_branch_to_a_remote_claude_branch_is_refused(self):
        self.git("branch", "feat/import-csv")
        self.assertEqual(self.push("feat/import-csv:claude/awesome-clarke").returncode, 1)

    def test_install_leaves_a_foreign_hook_alone(self):
        hook = self.repo / ".git" / "hooks" / "pre-push"
        hook.write_text("#!/bin/sh\nexit 0\n")
        subprocess.run(["sh", str(HOOK), "--install"], cwd=self.repo, env=self.env, check=True,
                       capture_output=True)
        self.assertEqual(hook.read_text(), "#!/bin/sh\nexit 0\n")

    def test_install_writes_the_hook(self):
        hook = self.repo / ".git" / "hooks" / "pre-push"
        hook.unlink()
        subprocess.run(["sh", str(HOOK), "--install"], cwd=self.repo, env=self.env, check=True)
        self.assertIn("no_claude_branch.sh", hook.read_text())

    def install(self):
        return subprocess.run(["sh", str(HOOK), "--install"], cwd=self.repo, env=self.env,
                              check=True, capture_output=True, text=True)

    def test_install_leaves_a_hook_chaining_it_alone(self):
        hook = self.repo / ".git" / "hooks" / "pre-push"
        chain = ("#!/bin/sh\n# chained pre-push hook\n"
                 "sh .claude/hooks/no_claude_branch.sh \"$@\" || status=1\nexit ${status:-0}\n")
        hook.write_text(chain)
        result = self.install()
        self.assertEqual(hook.read_text(), chain)
        self.assertIn("already chains no_claude_branch.sh, left alone", result.stderr)

    def test_install_rewrites_its_own_hook_identically(self):
        hook = self.repo / ".git" / "hooks" / "pre-push"
        hook.unlink()
        self.install()
        own = hook.read_bytes()
        result = self.install()
        self.assertEqual(hook.read_bytes(), own)
        self.assertEqual(result.stderr, "")
        self.assertTrue(os.access(hook, os.X_OK))


if __name__ == "__main__":
    unittest.main()
