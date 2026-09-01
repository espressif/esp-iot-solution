const connectionStatus = document.getElementById("connectionStatus");
const statusMessage = document.getElementById("statusMessage");
const runtimeNotice = document.getElementById("runtimeNotice");
const saveButton = document.getElementById("saveButton");
const reloadButton = document.getElementById("reloadButton");
const toggleJsonButton = document.getElementById("toggleJsonButton");
const jsonPanel = document.getElementById("jsonPanel");
const jsonSummary = document.getElementById("jsonSummary");
const jsonPreview = document.getElementById("jsonPreview");
const imuSdoControlToggle = document.querySelector(
    '[data-config-key="imu_sdo_gpio_control"]'
);
const imuSdoPinField = document.getElementById("imuSdoPinField");
const imuSdoPinInput = document.querySelector('[data-config-key="imu_sdo_pin"]');
const magEnableFlag = document.getElementById("magEnableFlag");
const magI2cControls = Array.from(document.querySelectorAll(
    '[data-config-key="mag_i2c_scl"], [data-config-key="mag_i2c_sda"]'
));

const configControls = Array.from(document.querySelectorAll("[data-config-key]"));

let isJsonExpanded = false;
let runtimeStatePollHandle = null;

function setStatus(state, message) {
    connectionStatus.textContent = state;
    connectionStatus.className = "status-pill " + state.toLowerCase();
    statusMessage.textContent = message || "";
}

function setRuntimeNotice(active, level, message) {
    if (!runtimeNotice) {
        return;
    }

    runtimeNotice.hidden = !active;
    runtimeNotice.textContent = active ? (message || "") : "";
    runtimeNotice.className = "runtime-notice";
    if (active && level) {
        runtimeNotice.classList.add(level.toLowerCase());
    }
}

function parseControlValue(control) {
    if (control.type === "checkbox") {
        return control.checked;
    }

    if (control.type === "number") {
        const parsed = Number.parseInt(control.value, 10);
        return Number.isNaN(parsed) ? 0 : parsed;
    }

    if (control.tagName === "SELECT") {
        const numericKeys = new Set([
            "knob_cw_sign",
            "space_switch_left_sign",
            "space_switch_up_sign",
        ]);

        if (numericKeys.has(control.dataset.configKey)) {
            return Number.parseInt(control.value, 10);
        }
    }

    return control.value;
}

function collectConfigFromForm() {
    const payload = {};

    configControls.forEach((control) => {
        if (
            control.dataset.configKey === "imu_sdo_pin" &&
            imuSdoControlToggle &&
            !imuSdoControlToggle.checked
        ) {
            return;
        }

        payload[control.dataset.configKey] = parseControlValue(control);
    });

    return payload;
}

function applyConfig(config) {
    const magEnabled = Boolean(config.mag_enable);

    if (magEnableFlag) {
        magEnableFlag.checked = magEnabled;
        magEnableFlag.title = magEnabled
            ? "BMM350 support is compiled into this firmware."
            : "BMM350 support is not compiled into this firmware.";
    }

    magI2cControls.forEach((control) => {
        control.disabled = !magEnabled;
        control.title = magEnabled
            ? ""
            : "This firmware was built without BMM350 support.";
    });

    configControls.forEach((control) => {
        const key = control.dataset.configKey;
        if (!(key in config)) {
            return;
        }

        if (control.type === "checkbox") {
            control.checked = Boolean(config[key]);
            return;
        }

        control.value = String(config[key]);
    });

    updateHardwareFieldState();
}

function updateHardwareFieldState() {
    const sdoControlEnabled = Boolean(imuSdoControlToggle?.checked);

    if (!imuSdoPinField || !imuSdoPinInput) {
        return;
    }

    imuSdoPinField.hidden = !sdoControlEnabled;
    imuSdoPinInput.disabled = !sdoControlEnabled;
}

function updateJsonPreview() {
    const payload = collectConfigFromForm();
    const fieldCount = Object.keys(payload).length;

    jsonSummary.textContent =
        "JSON Preview · " + fieldCount + " fields · " +
        (isJsonExpanded ? "expanded" : "collapsed");
    jsonPreview.textContent = JSON.stringify(payload, null, 2);
    jsonPanel.classList.toggle("expanded", isJsonExpanded);
    jsonPanel.classList.toggle("collapsed", !isJsonExpanded);
    toggleJsonButton.textContent = isJsonExpanded ? "Hide JSON" : "Show JSON";
}

async function loadConfig() {
    setStatus("Loading", "");

    try {
        const response = await fetch("/config");
        const text = await response.text();

        if (!response.ok) {
            setStatus("Error", "Load failed: " + text);
            return;
        }

        const config = JSON.parse(text);
        applyConfig(config);
        updateJsonPreview();
        setStatus("Ready", "");
    } catch (err) {
        setStatus("Error", "Load failed: " + err);
    }
}

async function saveConfig() {
    const payload = collectConfigFromForm();

    setStatus("Saving", "");
    updateJsonPreview();

    try {
        const response = await fetch("/config", {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify(payload),
        });

        const text = await response.text();
        if (!response.ok) {
            setStatus("Error", "Save failed: " + text);
            return;
        }

        const result = JSON.parse(text);
        if (result.next) {
            window.location.href = result.next;
            return;
        }

        await loadConfig();
    } catch (err) {
        setStatus("Error", "Save failed: " + err);
    }
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
        setRuntimeNotice(Boolean(state.active), state.level || "", state.message || "");
    } catch (err) {
        // Keep the config page usable even if the optional runtime-state probe fails.
    }
}

configControls.forEach((control) => {
    const eventName = control.type === "checkbox" ? "change" : "input";
    control.addEventListener(eventName, () => {
        updateHardwareFieldState();
        updateJsonPreview();
    });

    if (control.tagName === "SELECT" && control.type !== "checkbox") {
        control.addEventListener("change", () => {
            updateHardwareFieldState();
            updateJsonPreview();
        });
    }
});

toggleJsonButton.addEventListener("click", () => {
    isJsonExpanded = !isJsonExpanded;
    updateJsonPreview();
});

reloadButton.addEventListener("click", loadConfig);
saveButton.addEventListener("click", saveConfig);

updateJsonPreview();
updateHardwareFieldState();
loadConfig();
loadRuntimeState();
runtimeStatePollHandle = window.setInterval(loadRuntimeState, 1000);
