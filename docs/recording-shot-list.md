# 60-90 Second Phone-Friendly Recording Shot List

## Capture setup

- Regenerate data with `make dashboard-data` and serve `docs/` locally.
- Use a modern browser at approximately 390 x 844 CSS pixels to show the mobile layout.
- Record vertically at 1080 x 1920 if possible. Increase browser text size only if labels are not readable in the recording.
- Turn on Do Not Disturb and hide bookmarks, personal tabs, terminal paths, notifications, and account information.
- Use the committed telemetry files. Do not depend on internet access during recording.
- Perform one silent rehearsal and confirm that no unsafe action or unverified integration claim appears.

## Shot sequence

| Time | Screen action | Voiceover point |
| --- | --- | --- |
| 0-8 s | Start on the title, scenario choices, and Replay Only badge. | "This C-based demo turns motor-control telemetry into an operator-focused manufacturing workflow." |
| 8-20 s | Select Normal, press Start, and scroll to target, measured speed, PWM, and chart. | "The C simulation advances a local PI loop in fixed 10 millisecond steps; the page only replays its deterministic output." |
| 20-34 s | Tap Guided Interview Demo. Let the mechanical-jam trace reach Warning. | "A separate scenario adds load, while a separate monitor looks for sustained low speed and high drive effort." |
| 34-50 s | Hold on the Fault state, code, and zero PWM record after the local stop request. | "The monitor warns first, then latches a motor-stall fault and requests a local simulated stop." |
| 50-68 s | Scroll through the safe workflow and assistant evidence/source box. | "The operator sees safe next steps and a clearly simulated advisory response with sources and uncertainty. It cannot control the motor." |
| 68-82 s | End on the control-boundary card or a brief test-output screen. | "The strict C99 test suite covers control behavior, faults, edge cases, and deterministic full scenarios." |

## Final review checklist

- [ ] Text is readable on a phone without pausing the video.
- [ ] The video is between 60 and 90 seconds.
- [ ] Audio does not claim a real Tulip or language-model integration.
- [ ] The control boundary and advisory-only AI boundary are stated.
- [ ] No API key, username, private path, notification, or unrelated tab is visible.
- [ ] Captions are added for viewers watching without sound.
- [ ] The recording is watched once from beginning to end before sharing.

These checks are intentionally left for the repository owner to complete after recording.
