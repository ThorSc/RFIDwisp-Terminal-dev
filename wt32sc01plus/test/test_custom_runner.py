# Runner for `pio test -e native`: the test is a plain main() that prints
# "<n> checks, <m> failed" (and one FAIL line per failure), so the result is
# read from that summary line.
import re

from platformio.public import TestCase, TestRunnerBase, TestStatus


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        super().on_testing_line_output(line)
        match = re.match(r"(\d+) checks, (\d+) failed", line)
        if match:
            failed = int(match.group(2)) > 0
            self.test_suite.add_case(
                TestCase(
                    name=self.test_suite.test_name.replace("test_", "", 1),
                    status=TestStatus.FAILED if failed else TestStatus.PASSED,
                    message=line.strip(),
                )
            )
