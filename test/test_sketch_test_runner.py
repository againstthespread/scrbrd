"""Regression check for the renamed sketch and cwd-independent test entrypoint."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class SketchTestRunnerTest(unittest.TestCase):
    def test_alert_runner_from_outside_repository(self):
        runner = Path(__file__).resolve().with_name('fantasy_alert_queue_test.py')
        with tempfile.TemporaryDirectory(prefix='sketch-runner-') as directory:
            result = subprocess.run(
                [sys.executable, str(runner)], cwd=directory,
                capture_output=True, text=True,
            )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('sports storage passed.', result.stdout)


if __name__ == '__main__':
    unittest.main()
