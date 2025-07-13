import re

# Regex pattern to match pytest summary lines
summary_pattern = re.compile(r'(?P<failed>\d+ failed, )?(?P<passed>\d+ passed)?(, )?(?P<xpassed>\d+ xpassed)? .* in .*')

log_file = "log.txt"
summary_lines = []
totals = {"failed": 0, "passed": 0, "xpassed": 0}
failed_lines = []

with open(log_file, "r") as f:
    for line_num, line in enumerate(f, 1):
        match = summary_pattern.search(line)
        if match:
            summary_lines.append((line_num, line.strip()))
            if match.group("failed"):
                failed = int(match.group("failed").split()[0])
                totals["failed"] += failed
                failed_lines.append(line_num)
            if match.group("passed"):
                passed = int(match.group("passed").split()[0])
                totals["passed"] += passed
            if match.group("xpassed"):
                xpassed = int(match.group("xpassed").split()[0])
                totals["xpassed"] += xpassed

# Final summary output
print("=== Summary from log.txt ===")
print(f"Total passed:   {totals['passed']}")
print(f"Total xpassed:  {totals['xpassed']}")
print(f"Total failed:   {totals['failed']}")

if failed_lines:
    print(f"\nFailures detected on summary lines: {failed_lines}")
else:
    print("\n✅ All tests passed!")

