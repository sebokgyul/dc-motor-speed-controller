# Approved Troubleshooting Samples for the Demo

These are fictional, approved-sample instructions for the deterministic portfolio demo. They are not instructions for real equipment. A real deployment would use site-controlled maintenance documents reviewed for the specific machine and operator role.

## SOP-01: Safe inspection and escalation

1. Confirm that the machine is in a safe state using the site's approved procedure.
2. Read the active fault code and review recent telemetry.
3. Perform only inspections allowed for the operator's role.
4. Record the observation without assuming a root cause.
5. Escalate to qualified maintenance when the condition cannot be cleared safely.

Do not bypass guards, interlocks, emergency functions, or other safety mechanisms. Do not restart equipment that is not known to be safe.

## TRB-101: Speed-feedback checks

Applies to `F101_SENSOR_SIGNAL_INVALID` in this demo.

- Confirm that local control has commanded a safe output.
- From a safe state, visually inspect accessible speed-sensor connections for an obvious disconnection or damage.
- Compare the target, measured-speed field, controller state, and timestamp.
- Record the observed indication and connection condition.
- Escalate if inspection requires guarded access, electrical work, or an unsafe action.

An invalid signal indicates that the reading cannot be trusted. It does not, by itself, prove which physical component failed.

## TRB-201: Stalled-motor checks

Applies to `F201_MOTOR_STALL` in this demo.

- Confirm that the machine is stopped and in a safe state under the approved procedure.
- Review target speed, measured speed, PWM effort, and the warning-to-fault timeline.
- Visually inspect only accessible areas for an obstruction or abnormal load.
- Record what is observed without attempting to force movement.
- Escalate to qualified maintenance before any guarded, mechanical, or electrical intervention.

Low speed with high PWM is consistent with a stall or excessive load, but it is not proof of a particular mechanical root cause.
