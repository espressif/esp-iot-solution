const runtimeStatus = document.getElementById("runtimeStatus");
const runtimeTitle = document.getElementById("runtimeTitle");
const runtimeSubtitle = document.getElementById("runtimeSubtitle");
const runtimeMessage = document.getElementById("runtimeMessage");
const runtimeCountdown = document.getElementById("runtimeCountdown");
const runtimeCountdownLabel = document.getElementById("runtimeCountdownLabel");
const runtimeMode = document.getElementById("runtimeMode");
const runtimeLevel = document.getElementById("runtimeLevel");
const runtimeKind = document.getElementById("runtimeKind");
const runtimeAutoStart = document.getElementById("runtimeAutoStart");
const runtimeNotice = document.getElementById("runtimeNotice");

let runtimePollHandle = null;
let countdownHandle = null;
let countdownActive = false;
let countdownPosted = false;

function setRuntimeStatus(state, message) {
    runtimeStatus.textContent = state;
    runtimeStatus.className = "status-pill " + state.toLowerCase();
    if (message) {
        runtimeSubtitle.textContent = message;
    }
}

function setRuntimeNotice(active, level, message) {
    runtimeNotice.hidden = !active;
    runtimeNotice.textContent = active ? (message || "") : "";
    runtimeNotice.className = "runtime-notice";
    if (active && level) {
        runtimeNotice.classList.add(level.toLowerCase());
    }
}

function setCountdownValue(value, active) {
    runtimeCountdown.textContent = value;
    runtimeCountdown.className = "runtime-countdown " + (active ? "active" : "idle");
}

async function notifyRuntimeCalibrationStart() {
    if (countdownPosted) {
        return;
    }

    countdownPosted = true;
    setRuntimeStatus("Loading", "Gyroscope calibration sampling has started.");
    runtimeCountdownLabel.textContent =
        "Sampling is running now. Keep the device still until the page reports completion.";

    try {
        await fetch("/runtime-state/start", {
            method: "POST",
        });
    } catch (err) {
        setRuntimeStatus("Error", "Failed to notify the firmware to start calibration.");
        setRuntimeNotice(true, "error", String(err));
    }
}

function startCountdown(seconds) {
    if (countdownActive || countdownPosted) {
        return;
    }

    countdownActive = true;
    let remaining = Math.max(0, Number(seconds) || 0);
    setRuntimeStatus("Loading", "Prepare the device. Gyroscope calibration will start automatically.");
    runtimeTitle.textContent = "Gyroscope Countdown";
    runtimeMessage.textContent =
        "Keep the sensor motionless. The firmware will begin calibration when the countdown reaches zero.";
    runtimeCountdownLabel.textContent =
        "Place the device flat and still. No button press is required.";
    setCountdownValue(String(remaining), true);

    countdownHandle = window.setInterval(() => {
        remaining -= 1;
        if (remaining <= 0) {
            window.clearInterval(countdownHandle);
            countdownHandle = null;
            countdownActive = false;
            setCountdownValue("0", true);
            notifyRuntimeCalibrationStart();
            return;
        }
        setCountdownValue(String(remaining), true);
    }, 1000);
}

function stopCountdown() {
    if (countdownHandle) {
        window.clearInterval(countdownHandle);
        countdownHandle = null;
    }
    countdownActive = false;
}

function renderRuntimeState(state) {
    const level = state.level || "info";
    const mode = state.mode || "startup";
    const calibration = state.calibration || {};
    const kind = calibration.kind || "none";
    const pendingStart = Boolean(calibration.pending_start);

    runtimeMode.textContent = mode;
    runtimeLevel.textContent = level;
    runtimeKind.textContent = kind;
    runtimeAutoStart.textContent = pendingStart ? "countdown" : "running";
    setRuntimeNotice(Boolean(state.active), level, state.message || "");

    if (pendingStart && kind === "gyro") {
        startCountdown(calibration.countdown_seconds || 3);
        return;
    }

    if (!pendingStart) {
        stopCountdown();
    }

    if (kind === "gyro" && countdownPosted) {
        setRuntimeStatus("Loading", "Gyroscope calibration is in progress.");
        runtimeTitle.textContent = "Gyroscope Sampling";
        runtimeMessage.textContent =
            "The firmware is collecting still samples. Keep the device stationary.";
        runtimeCountdownLabel.textContent =
            "Sampling is active. The page will keep refreshing until the next step is ready.";
        setCountdownValue("...", true);
        return;
    }

    if (kind === "mag") {
        setRuntimeStatus("Loading", "Magnetometer calibration is in progress.");
        runtimeTitle.textContent = "Magnetometer Rotation";
        runtimeMessage.textContent =
            "Slowly rotate the device through different orientations so the magnetometer can gather enough coverage.";
        runtimeCountdownLabel.textContent =
            "Move smoothly through multiple directions until the firmware finishes calibration.";
        setCountdownValue("MAG", true);
        return;
    }

    if (!state.active) {
        setRuntimeStatus("Ready", "Calibration is complete. AirMouse will continue booting automatically.");
        runtimeTitle.textContent = "Calibration Complete";
        runtimeMessage.textContent =
            "All required startup calibration steps are finished. The HTTP server will close shortly.";
        runtimeCountdownLabel.textContent =
            "You can keep this page open while the firmware transitions into BLE mode.";
        setCountdownValue("OK", false);
        return;
    }

    setRuntimeStatus(level === "warning" ? "Loading" : "Ready", state.message || "");
    runtimeTitle.textContent = "Calibration Update";
    runtimeMessage.textContent =
        state.message || "Waiting for the next calibration update from the firmware.";
}

async function loadRuntimeState() {
    try {
        const response = await fetch("/runtime-state-data", {
            cache: "no-store",
        });
        if (!response.ok) {
            return;
        }

        const state = await response.json();
        renderRuntimeState(state);
    } catch (err) {
        setRuntimeStatus("Error", "Failed to load runtime calibration state.");
        setRuntimeNotice(true, "error", String(err));
    }
}

loadRuntimeState();
runtimePollHandle = window.setInterval(loadRuntimeState, 1000);
