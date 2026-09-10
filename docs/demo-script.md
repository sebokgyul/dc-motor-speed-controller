# Three-minute demo

The dashboard is supporting evidence, not the whole presentation. Rehearse this in your own words.

## 0:00–0:30 — The problem

**Show:** the dashboard title and three scenarios.

**Explain:** A controller's raw values are difficult for an operator to interpret. This learning project turns a simulated motor's data into a clear normal/warning/fault story. It is not real hardware or a Tulip integration.

## 0:30–1:00 — Normal operation

**Show:** Normal at about 3.0 seconds. Point to target, measured speed, applied PWM, and the chart.

**Explain:** A C program advances a simple motor model and PI controller in fixed 10 ms steps. The browser only replays telemetry sampled every 100 ms.

## 1:00–2:05 — Mechanical jam

**Show:** Mechanical Jam at 6.0 seconds, then 6.7 seconds. Point to the warning, falling speed, PI request, latched fault, applied PWM at zero, and event log.

**Explain:** At 5.5 seconds the scenario adds full simulated load. A separate monitor requires low speed, high PI request, and 750 ms of continuous evidence before latching the stall. The controller's request and the applied output are different at the fault transition because the monitor overrides the actuator. These are visible demo thresholds, not hardware settings.

## 2:05–2:35 — Operator response

**Show:** the four-step workflow and Rules-based guidance.

**Explain:** The guidance is fixed, deterministic text selected from the fault evidence. It does not use an LLM and cannot send commands. A real operator procedure would need site approval.

## 2:35–3:00 — Evidence and boundary

**Show:** the README test section or an A4 sheet; do not switch into source code unless asked.

**Explain:** The project builds as strict C99, has 26 focused tests, and produces repeatable table, CSV, and JSON Lines output. With real equipment, the controller and safety functions would stay local; slower telemetry and operator workflows could be handled by an operations platform such as Tulip.

## Phone/offline setup

- Public dashboard: save direct links to `?scenario=normal&at=3000` and `?scenario=mechanical-jam&at=6700` once hosting works.
- Offline backup: record a 60–90 second video containing those same two states.
- Printed backup: one A4 page with one real dashboard screenshot, the simple architecture, three verified facts, and the limitations.
