# AI use

AI was used to propose and implement parts of this project extension, including controller edge-case handling, tests, dashboard code, and documentation. AI is not used while the motor simulation or dashboard runs.

Agent-assisted checks have included:

- strict GCC and Clang builds;
- the complete C test suite;
- all scenarios in table, CSV, and JSON Lines formats;
- repeat-output comparisons; and
- desktop and narrow-screen dashboard checks in Chromium.

These checks do not prove that the repository owner understands every change. Before presenting the project, the owner still needs to review the changes line by line, repeat the build and demo personally, and be able to explain or remove anything they cannot defend.

The dashboard's “Rules-based guidance” panel is a separate project feature. It selects fixed text from a fault code and a frozen telemetry snapshot. It is not a language model, does not retrieve external documents, and cannot control the simulated motor.
