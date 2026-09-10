(function () {
  "use strict";

  var DATA_FILES = {
    "normal": "data/normal.jsonl",
    "sensor-disconnect": "data/sensor-disconnect.jsonl",
    "mechanical-jam": "data/mechanical-jam.jsonl"
  };

  var SCENARIO_LABELS = {
    "normal": "Normal scenario",
    "sensor-disconnect": "Sensor disconnect",
    "mechanical-jam": "Mechanical jam"
  };

  var STATUS_COPY = {
    disabled: "Machine disabled",
    running: "Machine running",
    warning: "Potential stall under evaluation",
    fault: "Machine fault latched"
  };

  var state = {
    scenario: "normal",
    records: [],
    index: 0,
    timer: null,
    playing: false,
    previousRecord: null,
    faultEvidence: null,
    guided: false,
    requestNumber: 0
  };

  var elements = {
    scenarioButtons: document.querySelectorAll(".scenario-button"),
    start: document.getElementById("start-button"),
    pause: document.getElementById("pause-button"),
    reset: document.getElementById("reset-button"),
    guided: document.getElementById("guided-button"),
    guidedBanner: document.getElementById("guided-banner"),
    guidedStep: document.getElementById("guided-step"),
    guidedCopy: document.getElementById("guided-copy"),
    loadError: document.getElementById("load-error"),
    statusStrip: document.querySelector(".status-strip"),
    machineSummary: document.getElementById("machine-summary"),
    scenarioLabel: document.getElementById("scenario-label"),
    simulationTime: document.getElementById("simulation-time"),
    machineBadge: document.getElementById("machine-state-badge"),
    target: document.getElementById("target-value"),
    measured: document.getElementById("measured-value"),
    pwm: document.getElementById("pwm-value"),
    controllerPwmNote: document.getElementById("controller-pwm-note"),
    targetBar: document.getElementById("target-bar"),
    measuredBar: document.getElementById("measured-bar"),
    pwmBar: document.getElementById("pwm-bar"),
    controller: document.getElementById("controller-value"),
    monitor: document.getElementById("monitor-value"),
    faultCodeValue: document.getElementById("fault-code-value"),
    chart: document.getElementById("rpm-chart"),
    chartSummary: document.getElementById("chart-summary"),
    eventLog: document.getElementById("event-log"),
    eventCount: document.getElementById("event-count"),
    faultPanel: document.getElementById("fault-panel"),
    faultTitle: document.getElementById("fault-title"),
    faultMessage: document.getElementById("fault-message"),
    faultCodeBadge: document.getElementById("fault-code-badge"),
    assistantResponse: document.getElementById("assistant-response"),
    assistantSources: document.getElementById("assistant-sources"),
    operatorNote: document.getElementById("operator-note"),
    recordNote: document.getElementById("record-note-button")
  };

  function titleCase(value) {
    return value.replace(/_/g, " ").replace(/\b\w/g, function (letter) {
      return letter.toUpperCase();
    });
  }

  function formatTime(timestampMs) {
    return (timestampMs / 1000).toFixed(1) + " s";
  }

  function setSelectedButton(scenario) {
    elements.scenarioButtons.forEach(function (button) {
      var selected = button.dataset.scenario === scenario;
      button.classList.toggle("is-selected", selected);
      button.setAttribute("aria-pressed", selected ? "true" : "false");
    });
  }

  function setPlaying(playing) {
    state.playing = playing;
    elements.start.disabled = playing || state.records.length === 0;
    elements.pause.disabled = !playing;
  }

  function stopReplay() {
    if (state.timer !== null) {
      window.clearInterval(state.timer);
      state.timer = null;
    }
    setPlaying(false);
  }

  function clearEvents() {
    elements.eventLog.replaceChildren();
    elements.eventCount.textContent = "0 events";
    state.previousRecord = null;
  }

  function addEvent(timestampMs, message, tone) {
    var item = document.createElement("li");
    var time = document.createElement("time");
    var marker = document.createElement("span");
    var copy = document.createElement("span");

    if (tone) {
      item.className = "event-" + tone;
    }
    time.dateTime = "PT" + timestampMs / 1000 + "S";
    time.textContent = formatTime(timestampMs);
    marker.className = "event-marker";
    marker.setAttribute("aria-hidden", "true");
    copy.textContent = message;
    item.append(time, marker, copy);
    elements.eventLog.appendChild(item);
    elements.eventLog.scrollTop = elements.eventLog.scrollHeight;
    elements.eventCount.textContent = elements.eventLog.children.length +
      (elements.eventLog.children.length === 1 ? " event" : " events");
  }

  function trackEvents(record) {
    var previous = state.previousRecord;

    if (previous === null) {
      addEvent(record.timestamp_ms, "Telemetry replay initialized from C simulation output.");
    } else {
      if (record.target_rpm !== previous.target_rpm) {
        addEvent(record.timestamp_ms, "Target changed to " + Math.round(record.target_rpm) + " RPM.");
      }
      if (record.controller_status !== previous.controller_status) {
        addEvent(record.timestamp_ms, "Controller state changed to " + titleCase(record.controller_status) + ".");
      }
      if (record.machine_status !== previous.machine_status) {
        addEvent(
          record.timestamp_ms,
          "Machine state changed to " + titleCase(record.machine_status) + ".",
          record.machine_status === "fault" ? "fault" :
            record.machine_status === "warning" ? "warning" : ""
        );
      }
      if (record.fault_code !== previous.fault_code && record.fault_code !== "NONE") {
        addEvent(record.timestamp_ms, record.fault_code + ": " + record.fault_message, "fault");
      }
    }

    state.previousRecord = record;
  }

  function recentTelemetrySummary(record, recordIndex) {
    var end = Number.isInteger(recordIndex) ? recordIndex : state.index;
    var start = Math.max(0, end - 10);
    var recent = state.records.slice(start, end + 1);
    var peakControllerPwm = recent.reduce(function (peak, item) {
      return Math.max(peak, item.controller_pwm_duty);
    }, 0);

    return {
      target: Math.round(record.target_rpm),
      measured: Number.isFinite(record.measured_rpm) ? Math.round(record.measured_rpm) : "unavailable",
      peakControllerPwm: peakControllerPwm.toFixed(1)
    };
  }

  function populateGuidance(evidence) {
    var record = evidence.record;
    var telemetry = evidence.telemetry;
    var sources;

    if (record.fault_code === "F101_SENSOR_SIGNAL_INVALID") {
      elements.faultTitle.textContent = "Measured-speed sensor signal invalid";
      elements.assistantResponse.textContent =
        "The speed channel became invalid while the target remained " + telemetry.target +
        " RPM. The local controller entered Input Fault and commanded 0% PWM. This is consistent " +
        "with a disconnected or out-of-range sensor signal, but the simulated evidence cannot prove " +
        "the physical cause. Keep the machine safe and inspect the feedback connection before escalation.";
      sources = [
        "TRB-101 / Speed-feedback checks",
        "SOP-01 / Safe inspection and escalation"
      ];
    } else if (record.fault_code === "F201_MOTOR_STALL") {
      elements.faultTitle.textContent = "Sustained motor stall detected";
      elements.assistantResponse.textContent =
        "Measured speed fell to " + telemetry.measured + " RPM against a " + telemetry.target +
        " RPM target while the PI request reached " + telemetry.peakControllerPwm +
        "%. The local monitor then requested a stop. This pattern is consistent with an obstruction " +
        "or excessive load, not proof of either cause. Do not restart; confirm a safe state and inspect " +
        "only within the approved procedure.";
      sources = [
        "TRB-201 / Stalled-motor checks",
        "SOP-01 / Safe inspection and escalation"
      ];
    } else {
      elements.faultTitle.textContent = "Local controller configuration fault";
      elements.assistantResponse.textContent =
        "The local control simulation reported " + record.fault_code +
        ". The available telemetry is not sufficient for an operator diagnosis. Keep the machine " +
        "in a safe state and escalate the controller or timing configuration to qualified maintenance. " +
        "Do not treat this fallback guidance as a root-cause determination.";
      sources = [
        "SOP-01 / Safe inspection and escalation"
      ];
    }

    elements.assistantSources.replaceChildren();
    sources.forEach(function (source) {
      var item = document.createElement("li");
      item.textContent = source;
      elements.assistantSources.appendChild(item);
    });
  }

  function updateFaultPanel(record) {
    if (record.fault_code === "NONE") {
      elements.faultPanel.hidden = true;
      return;
    }

    if (state.faultEvidence === null
        || state.faultEvidence.record.fault_code !== record.fault_code) {
      state.faultEvidence = {
        record: record,
        telemetry: recentTelemetrySummary(record)
      };
    }

    elements.faultPanel.hidden = false;
    elements.faultMessage.textContent = record.fault_message;
    elements.faultCodeBadge.textContent = record.fault_code;
    populateGuidance(state.faultEvidence);
  }

  function updateGuidedStep(record) {
    if (!state.guided) {
      return;
    }

    if (record.timestamp_ms < 1000) {
      elements.guidedStep.textContent = "Step 1 of 4";
      elements.guidedCopy.textContent = "Establish the boundary: the 10 ms simulated PI step is local; this page is read-only.";
    } else if (record.timestamp_ms < 5500) {
      elements.guidedStep.textContent = "Step 2 of 4";
      elements.guidedCopy.textContent = "Show the target, feedback response, PWM output, and timestamped event history.";
    } else if (record.machine_status !== "fault") {
      elements.guidedStep.textContent = "Step 3 of 4";
      elements.guidedCopy.textContent = "A simulated jam drives speed down and PWM up; sustained evidence creates a warning.";
    } else {
      elements.guidedStep.textContent = "Step 4 of 4";
      elements.guidedCopy.textContent = "The local monitor latches a fault; operator guidance and advisory evidence are now visible.";
    }
  }

  function drawChart() {
    var canvas = elements.chart;
    var context = canvas.getContext("2d");
    var bounds = canvas.getBoundingClientRect();
    var ratio = Math.min(window.devicePixelRatio || 1, 2);
    var width = Math.max(bounds.width, 280);
    var height = Math.max(bounds.height, 180);
    var padding = { top: 13, right: 15, bottom: 28, left: 49 };
    var plotWidth = width - padding.left - padding.right;
    var plotHeight = height - padding.top - padding.bottom;
    var maximumTime = state.records.length > 0
      ? state.records[state.records.length - 1].timestamp_ms
      : 11000;
    var visible = state.records.slice(0, state.index + 1);

    canvas.width = Math.round(width * ratio);
    canvas.height = Math.round(height * ratio);
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, width, height);
    context.font = "12px " + getComputedStyle(document.documentElement).getPropertyValue("--mono");
    context.textBaseline = "middle";

    [0, 1000, 2000, 3000].forEach(function (rpm) {
      var y = padding.top + plotHeight - rpm / 3000 * plotHeight;
      context.strokeStyle = "rgba(56, 84, 107, 0.55)";
      context.lineWidth = 1;
      context.beginPath();
      context.moveTo(padding.left, y);
      context.lineTo(width - padding.right, y);
      context.stroke();
      context.fillStyle = "#7890a2";
      context.textAlign = "right";
      context.fillText(String(rpm), padding.left - 8, y);
    });

    [0, 2, 4, 6, 8, 10].forEach(function (seconds) {
      var x = padding.left + seconds * 1000 / maximumTime * plotWidth;
      context.fillStyle = "#7890a2";
      context.textAlign = "center";
      context.fillText(seconds + "s", x, height - 10);
    });

    function plot(key, color, dashed) {
      var segmentStarted = false;

      if (visible.length === 0) {
        return;
      }
      context.strokeStyle = color;
      context.lineWidth = 2;
      context.setLineDash(dashed ? [6, 5] : []);
      context.beginPath();
      visible.forEach(function (record) {
        if (!Number.isFinite(record[key])) {
          segmentStarted = false;
          return;
        }
        var x = padding.left + record.timestamp_ms / maximumTime * plotWidth;
        var y = padding.top + plotHeight - Math.min(record[key], 3000) / 3000 * plotHeight;
        if (!segmentStarted) {
          context.moveTo(x, y);
        } else {
          context.lineTo(x, y);
        }
        segmentStarted = true;
      });
      context.stroke();
      context.setLineDash([]);
    }

    plot("target_rpm", "#58d3dd", true);
    plot("measured_rpm", "#edf5f7", false);

    if (visible.length > 0) {
      var current = visible[visible.length - 1];
      var cursorX = padding.left + current.timestamp_ms / maximumTime * plotWidth;
      context.strokeStyle = current.machine_status === "fault" ? "#ff6b6b" : "rgba(145, 167, 184, 0.55)";
      context.lineWidth = 1;
      context.beginPath();
      context.moveTo(cursorX, padding.top);
      context.lineTo(cursorX, padding.top + plotHeight);
      context.stroke();
    }
  }

  function renderRecord(record) {
    var machineStatus = record.machine_status;
    var measuredAvailable = Number.isFinite(record.measured_rpm);
    var measuredCopy = measuredAvailable ? Math.round(record.measured_rpm) + " RPM" : "unavailable";

    elements.target.textContent = Math.round(record.target_rpm);
    elements.measured.textContent = measuredAvailable ? Math.round(record.measured_rpm) : "--";
    elements.pwm.textContent = record.applied_pwm_duty.toFixed(1);
    elements.controllerPwmNote.textContent = "PI request: " +
      record.controller_pwm_duty.toFixed(1) + "%";
    elements.targetBar.style.width = Math.min(record.target_rpm / 3000 * 100, 100) + "%";
    elements.measuredBar.style.width = measuredAvailable
      ? Math.min(record.measured_rpm / 3000 * 100, 100) + "%"
      : "0%";
    elements.pwmBar.style.width = Math.min(record.applied_pwm_duty, 100) + "%";
    elements.controller.textContent = titleCase(record.controller_status);
    elements.monitor.textContent = titleCase(machineStatus);
    elements.faultCodeValue.textContent = "Fault code: " + record.fault_code;
    elements.machineSummary.textContent = STATUS_COPY[machineStatus] || "Machine state unknown";
    elements.scenarioLabel.textContent = SCENARIO_LABELS[state.scenario];
    elements.simulationTime.textContent = "T+" + (record.timestamp_ms / 1000).toFixed(1).padStart(4, "0") + " s";
    elements.machineBadge.textContent = titleCase(machineStatus);
    elements.machineBadge.className = "state-badge state-" + machineStatus;
    elements.statusStrip.className = "status-strip is-" + machineStatus;
    elements.chartSummary.textContent = "At " + formatTime(record.timestamp_ms) +
      ", target speed is " + Math.round(record.target_rpm) + " RPM and measured speed is " +
      measuredCopy + ". Machine state is " + machineStatus + ".";

    trackEvents(record);
    updateFaultPanel(record);
    updateGuidedStep(record);
    drawChart();
  }

  function showCurrentRecord() {
    if (state.records.length > 0) {
      renderRecord(state.records[state.index]);
    }
  }

  function advanceReplay() {
    if (state.index >= state.records.length - 1) {
      stopReplay();
      addEvent(state.records[state.index].timestamp_ms, "Telemetry replay complete.");
      return;
    }

    state.index += 1;
    showCurrentRecord();
  }

  function startReplay() {
    if (state.records.length === 0 || state.playing) {
      return;
    }
    if (state.index >= state.records.length - 1) {
      resetReplay();
    }
    setPlaying(true);
    state.timer = window.setInterval(advanceReplay, 100);
  }

  function resetReplay() {
    stopReplay();
    state.index = 0;
    clearEvents();
    state.faultEvidence = null;
    elements.operatorNote.value = "";
    showCurrentRecord();
  }

  function recordsAreValid(records) {
    return records.every(function (record, index) {
      var measuredIsValid = record.measured_rpm === null
        || Number.isFinite(record.measured_rpm);
      var timestampIsOrdered = index === 0
        || record.timestamp_ms > records[index - 1].timestamp_ms;

      return Number.isInteger(record.timestamp_ms)
        && record.timestamp_ms >= 0
        && timestampIsOrdered
        && Number.isFinite(record.target_rpm)
        && measuredIsValid
        && Number.isFinite(record.controller_pwm_duty)
        && Number.isFinite(record.applied_pwm_duty)
        && typeof record.controller_status === "string"
        && typeof record.machine_status === "string"
        && typeof record.fault_code === "string"
        && typeof record.fault_message === "string";
    });
  }

  function parseJsonLines(text) {
    var records = text.trim().split("\n").filter(Boolean).map(function (line) {
      return JSON.parse(line);
    });

    if (!recordsAreValid(records)) {
      throw new Error("Telemetry records do not match the expected schema.");
    }
    return records;
  }

  function renderLoadingState(scenario) {
    elements.target.textContent = "0";
    elements.measured.textContent = "--";
    elements.pwm.textContent = "0.0";
    elements.controllerPwmNote.textContent = "PI request: 0.0%";
    elements.targetBar.style.width = "0%";
    elements.measuredBar.style.width = "0%";
    elements.pwmBar.style.width = "0%";
    elements.controller.textContent = "Awaiting data";
    elements.monitor.textContent = "Awaiting data";
    elements.faultCodeValue.textContent = "Fault code: NONE";
    elements.machineSummary.textContent = "Loading telemetry";
    elements.scenarioLabel.textContent = SCENARIO_LABELS[scenario];
    elements.simulationTime.textContent = "T+00.0 s";
    elements.machineBadge.textContent = "Loading";
    elements.machineBadge.className = "state-badge state-disabled";
    elements.statusStrip.className = "status-strip";
    elements.faultPanel.hidden = true;
    drawChart();
  }

  function showReplayAt(timestampMs) {
    var targetIndex = 0;
    var recordIndex;

    for (recordIndex = 0; recordIndex < state.records.length; recordIndex += 1) {
      if (state.records[recordIndex].timestamp_ms > timestampMs) {
        break;
      }
      targetIndex = recordIndex;
    }

    stopReplay();
    clearEvents();
    state.faultEvidence = null;
    state.index = targetIndex;
    for (recordIndex = 0; recordIndex <= targetIndex; recordIndex += 1) {
      trackEvents(state.records[recordIndex]);
      if (state.faultEvidence === null
          && state.records[recordIndex].fault_code !== "NONE") {
        state.faultEvidence = {
          record: state.records[recordIndex],
          telemetry: recentTelemetrySummary(state.records[recordIndex], recordIndex)
        };
      }
    }
    showCurrentRecord();
  }

  function loadScenario(scenario, startAfterLoad, initialTimestampMs) {
    var requestNumber = state.requestNumber + 1;
    state.requestNumber = requestNumber;
    stopReplay();
    state.scenario = scenario;
    state.records = [];
    state.index = 0;
    clearEvents();
    setSelectedButton(scenario);
    elements.loadError.hidden = true;
    elements.start.disabled = true;
    renderLoadingState(scenario);

    return window.fetch(DATA_FILES[scenario]).then(function (response) {
      if (!response.ok) {
        throw new Error("Telemetry request returned HTTP " + response.status + ".");
      }
      return response.text();
    }).then(function (text) {
      if (requestNumber !== state.requestNumber) {
        return;
      }
      state.records = parseJsonLines(text);
      if (state.records.length === 0) {
        throw new Error("The telemetry file is empty.");
      }
      resetReplay();
      if (Number.isFinite(initialTimestampMs)) {
        showReplayAt(initialTimestampMs);
      } else if (startAfterLoad) {
        startReplay();
      }
    }).catch(function (error) {
      if (requestNumber !== state.requestNumber) {
        return;
      }
      elements.loadError.hidden = false;
      elements.loadError.textContent = "Unable to load telemetry: " + error.message +
        " Check that the data files are present and that this page is served over HTTP.";
      elements.machineSummary.textContent = "Telemetry unavailable";
      elements.machineBadge.textContent = "Unavailable";
      setPlaying(false);
    });
  }

  function exitGuidedMode() {
    state.requestNumber += 1;
    state.guided = false;
    elements.guided.setAttribute("aria-pressed", "false");
    elements.guided.querySelector("strong").textContent = "Guided interview demo";
    elements.guidedBanner.hidden = true;
  }

  elements.scenarioButtons.forEach(function (button) {
    button.addEventListener("click", function () {
      exitGuidedMode();
      loadScenario(button.dataset.scenario, false);
    });
  });

  elements.start.addEventListener("click", startReplay);
  elements.pause.addEventListener("click", stopReplay);
  elements.reset.addEventListener("click", resetReplay);

  elements.guided.addEventListener("click", function () {
    if (state.guided) {
      exitGuidedMode();
      stopReplay();
      loadScenario(state.scenario, false);
      return;
    }

    state.guided = true;
    elements.guided.setAttribute("aria-pressed", "true");
    elements.guided.querySelector("strong").textContent = "Exit guided demo";
    elements.guidedBanner.hidden = false;
    loadScenario("mechanical-jam", true);
  });

  elements.recordNote.addEventListener("click", function () {
    var note = elements.operatorNote.value.trim();
    var timestamp = state.records.length > 0 ? state.records[state.index].timestamp_ms : 0;

    if (note.length === 0) {
      elements.operatorNote.focus();
      return;
    }
    addEvent(timestamp, "Operator observation recorded: " + note);
    elements.operatorNote.value = "";
  });

  function initialView() {
    var parameters = new URLSearchParams(window.location.search);
    var requestedScenario = parameters.get("scenario");
    var requestedTimestamp = Number(parameters.get("at"));

    return {
      scenario: Object.prototype.hasOwnProperty.call(DATA_FILES, requestedScenario)
        ? requestedScenario
        : "normal",
      timestamp: parameters.has("at") && Number.isFinite(requestedTimestamp)
        ? Math.max(0, requestedTimestamp)
        : null
    };
  }

  window.addEventListener("resize", drawChart);
  var openingView = initialView();
  loadScenario(openingView.scenario, false, openingView.timestamp);
}());
