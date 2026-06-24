const $ = (id) => document.getElementById(id);
const state = {
  audioWs: null,
  devices: null,
  audioCtx: null,
  micWorkletNode: null,
  micPlayer: null,
  micScriptPlayer: null,
  micWorkletReady: null,
  micWorkletFailed: false,
  micWorkletFormatKey: "",
  toneSending: false,
  filePlaying: false,
  fileStop: false,
  fileTx: null,
  spkResetting: false,
  spkRefillBudget: 0,
  spkBudgetWaitSince: 0,
  spkBufferWaitSince: 0,
  spkLastBudgetWaitLog: 0,
  spkLastBufferWaitLog: 0,
  micFrames: 0,
  micLevel: 0,
  cameraWs: null,
  cameraObjectUrl: null,
  cameraStreamActive: false,
  cameraStopRequested: false,
  cameraFirstFrameLogged: false,
  cameraLastConnected: false,
  cameraLastEnabled: false,
};
const SPK_MAX_PAYLOAD = 4092;
const SPK_WS_BUFFER_LIMIT = 65536;
const CAMERA_HEADER_SIZE = 16;
const CAMERA_FORMAT_MJPEG = 1;

function log(message) {
  const el = $("log");
  const time = new Date().toLocaleTimeString();
  el.textContent = `[${time}] ${message}\n` + el.textContent;
}

window.addEventListener("error", (event) => {
  log(`JS error: ${event.message}`);
});

window.addEventListener("unhandledrejection", (event) => {
  log(`Promise error: ${event.reason && event.reason.message ? event.reason.message : event.reason}`);
});

function sendAudioJson(obj) {
  if (state.audioWs && state.audioWs.readyState === WebSocket.OPEN) {
    state.audioWs.send(JSON.stringify(obj));
  }
}

function setPill(el, text, cls) {
  el.className = `pill ${cls || ""}`.trim();
  el.textContent = text;
}

function formatLabel(alt) {
  const freqs = alt.sample_freq_type === 0 ? `${alt.freqs[0]}-${alt.freqs[1]} Hz` : `${alt.freqs.join(",")} Hz`;
  return `Alt ${alt.alt}: ${alt.channels}ch ${alt.bit_resolution}bit ${freqs}`;
}

function fillFormats(select, stream) {
  select.innerHTML = "";
  if (!stream || !stream.connected) return;
  for (const alt of stream.alts) {
    const freqs = alt.sample_freq_type === 0 ? [stream.sample_freq || alt.freqs[0]] : alt.freqs;
    for (const freq of freqs) {
      const option = document.createElement("option");
      option.value = `${alt.alt}:${freq}`;
      option.textContent = formatLabel({ ...alt, freqs: [freq] });
      option.selected = alt.alt === stream.selected_alt && freq === stream.sample_freq;
      select.appendChild(option);
    }
  }
}

function updateControls(streamName, stream) {
  const enabled = $(`${streamName}Enabled`);
  const mute = $(`${streamName}Mute`);
  const volume = $(`${streamName}Volume`);
  const format = $(`${streamName}Format`);
  const connected = stream && stream.connected;
  enabled.disabled = !connected;
  mute.disabled = !connected || !stream.mute_supported;
  volume.disabled = !connected || !stream.volume_supported;
  format.disabled = !connected || stream.enabled;
  enabled.textContent = stream.enabled ? "Close" : "Open";
  enabled.dataset.enabled = stream.enabled ? "1" : "0";
  enabled.classList.toggle("is-open-action", connected && !stream.enabled);
  enabled.classList.toggle("is-close-action", connected && stream.enabled);
  mute.checked = !!stream.mute;
  volume.value = stream.volume || 80;
  fillFormats(format, stream);
}

function cameraFormatByName(camera, name) {
  if (!camera || !Array.isArray(camera.formats)) return null;
  return camera.formats.find((item) => item.format === name) || null;
}

function currentCameraFormat(camera) {
  return cameraFormatByName(camera, camera && camera.selected_format) || cameraFormatByName(camera, "mjpeg");
}

function fillCameraFormats(camera) {
  const select = $("cameraFormat");
  select.innerHTML = "";
  const formats = camera && Array.isArray(camera.formats) ? camera.formats : [];
  for (const item of formats) {
    const option = document.createElement("option");
    option.value = item.format;
    option.textContent = item.format.toUpperCase();
    option.selected = item.format === camera.selected_format;
    select.appendChild(option);
  }
  const h264 = document.createElement("option");
  h264.value = "h264";
  h264.textContent = "H.264 (unsupported)";
  h264.disabled = true;
  if (!formats.some((item) => item.format === "h264")) select.appendChild(h264);
}

function fillCameraResolutions(camera) {
  const select = $("cameraResolution");
  select.innerHTML = "";
  const format = currentCameraFormat(camera);
  const resolutions = format && Array.isArray(format.resolutions) ? format.resolutions : [];
  for (const item of resolutions) {
    const option = document.createElement("option");
    option.value = String(item.index);
    option.textContent = `${item.width}x${item.height}@${Math.round(item.fps || 0)}fps`;
    option.selected = item.index === camera.selected_resolution;
    select.appendChild(option);
  }
}

function stopCameraStream(text) {
  state.cameraStopRequested = true;
  if (state.cameraWs) {
    state.cameraWs.close();
    state.cameraWs = null;
  }
  if (state.cameraObjectUrl) {
    URL.revokeObjectURL(state.cameraObjectUrl);
    state.cameraObjectUrl = null;
  }
  const img = $("cameraStream");
  img.removeAttribute("src");
  img.classList.remove("active");
  $("cameraPlaceholder").textContent = text || "Camera disabled";
  $("cameraPlaceholder").classList.remove("hidden");
  state.cameraStreamActive = false;
  state.cameraFirstFrameLogged = false;
}

function showCameraJpeg(payload) {
  const oldUrl = state.cameraObjectUrl;
  const blob = new Blob([payload], { type: "image/jpeg" });
  const url = URL.createObjectURL(blob);
  state.cameraObjectUrl = url;
  const img = $("cameraStream");
  img.src = url;
  img.classList.add("active");
  $("cameraPlaceholder").classList.add("hidden");
  if (oldUrl) URL.revokeObjectURL(oldUrl);
}

function handleCameraBinary(buf) {
  if (buf.byteLength < CAMERA_HEADER_SIZE) {
    log(`Camera WS short packet: len=${buf.byteLength}`);
    return;
  }
  const dv = new DataView(buf);
  if (dv.getUint8(0) !== 0x43 || dv.getUint8(1) !== 1) {
    log(`Camera WS bad header: magic=${dv.getUint8(0)} version=${dv.getUint8(1)} len=${buf.byteLength}`);
    return;
  }
  const format = dv.getUint8(2);
  const payloadLen = dv.getUint32(4, true);
  if (payloadLen + CAMERA_HEADER_SIZE !== buf.byteLength) {
    log(`Camera WS bad length: payload=${payloadLen} packet=${buf.byteLength}`);
    return;
  }
  if (format === CAMERA_FORMAT_MJPEG) {
    if (!state.cameraFirstFrameLogged) {
      log(`Camera first MJPEG frame: ${dv.getUint16(12, true)}x${dv.getUint16(14, true)} len=${payloadLen}`);
      state.cameraFirstFrameLogged = true;
    }
    showCameraJpeg(new Uint8Array(buf, CAMERA_HEADER_SIZE, payloadLen));
  } else {
    log(`Camera WS unsupported frame format: ${format}`);
  }
}

function startCameraStream() {
  if (state.cameraStreamActive) return;
  state.cameraStopRequested = false;
  state.cameraStreamActive = true;
  const url = `ws://${location.host}/camera_ws`;
  log(`Camera WebSocket opening: ${url}`);
  try {
    state.cameraWs = new WebSocket(url);
  } catch (err) {
    state.cameraStreamActive = false;
    log(`Camera WebSocket constructor failed: ${err.message}`);
    return;
  }
  state.cameraWs.binaryType = "arraybuffer";
  state.cameraWs.onopen = () => {
    log("Camera WebSocket connected");
    const camera = state.devices && state.devices.camera;
    if (camera && camera.enabled && camera.selected_format === "mjpeg") {
      state.cameraWs.send(JSON.stringify({ type: "set_camera_enabled", enabled: true }));
    }
  };
  state.cameraWs.onmessage = (event) => {
    if (event.data instanceof ArrayBuffer) {
      handleCameraBinary(event.data);
    } else {
      log(`Camera WS non-binary message: ${typeof event.data}`);
    }
  };
  state.cameraWs.onerror = () => log(`Camera WebSocket error: ${url} state=${state.cameraWs ? state.cameraWs.readyState : "-"}`);
  state.cameraWs.onclose = (event) => {
    log(`Camera WebSocket disconnected: code=${event.code} reason=${event.reason || "-"} clean=${event.wasClean}`);
    state.cameraWs = null;
    state.cameraStreamActive = false;
    const camera = state.devices && state.devices.camera;
    if (!state.cameraStopRequested && camera && camera.connected && camera.enabled && camera.streaming) {
      setTimeout(startCameraStream, 500);
    }
  };
}

function sendCameraJson(obj) {
  startCameraStream();
  const payload = JSON.stringify(obj);
  const send = (retry = 0) => {
    if (state.cameraWs && state.cameraWs.readyState === WebSocket.OPEN) {
      state.cameraWs.send(payload);
    } else if (retry < 10) {
      setTimeout(() => send(retry + 1), 100);
    } else {
      log(`Camera WS command dropped: ${obj.type}`);
    }
  };
  send();
}

function sendCameraFormat() {
  sendCameraJson({
    type: "set_camera_format",
    format: $("cameraFormat").value,
    resolution: Number($("cameraResolution").value),
  });
}

function updateCamera(camera) {
  const connected = camera && camera.connected;
  const format = currentCameraFormat(camera);
  const resolutions = format && Array.isArray(format.resolutions) ? format.resolutions : [];
  const enabled = connected && camera.enabled;
  const streaming = enabled && camera.streaming && camera.selected_format === "mjpeg";
  const shouldOpenCameraWs = enabled && camera.selected_format === "mjpeg";
  setPill($("cameraState"), connected ? (enabled ? "Enabled" : "Ready") : "Disconnected", connected ? "ok" : "");
  $("cameraEnabled").disabled = !connected || !cameraFormatByName(camera, "mjpeg") || !resolutions.length;
  $("cameraEnabled").textContent = enabled ? "Close" : "Open";
  $("cameraEnabled").dataset.enabled = enabled ? "1" : "0";
  $("cameraEnabled").classList.toggle("is-open-action", connected && !enabled && !!cameraFormatByName(camera, "mjpeg") && !!resolutions.length);
  $("cameraEnabled").classList.toggle("is-close-action", connected && enabled);
  fillCameraFormats(camera);
  fillCameraResolutions(camera);
  $("cameraFormat").disabled = !connected || enabled;
  $("cameraResolution").disabled = !connected || enabled || !resolutions.length;
  if (!connected) {
    stopCameraStream("Camera disconnected");
  } else if (!enabled) {
    stopCameraStream("Camera disabled");
  } else if (!shouldOpenCameraWs) {
    stopCameraStream("Camera stream unavailable");
  } else {
    if (!streaming) $("cameraPlaceholder").textContent = "Camera starting";
    startCameraStream();
  }
  if (connected !== state.cameraLastConnected) {
    log(connected ? "Camera connected" : "Camera disconnected");
    state.cameraLastConnected = connected;
  }
  if (enabled !== state.cameraLastEnabled) {
    log(enabled ? "Camera enabled" : "Camera disabled");
    state.cameraLastEnabled = enabled;
  }
}

function setToolGroupDisabled(groupId, disabled) {
  const group = $(groupId);
  group.classList.toggle("disabled", disabled);
  group.setAttribute("aria-disabled", disabled ? "true" : "false");
  group.querySelectorAll("input, select, button").forEach((control) => {
    control.disabled = disabled;
  });
}

function resetFilePlaybackState(reason) {
  if (!state.filePlaying && !state.fileTx) return;
  state.fileStop = true;
  state.spkRefillBudget = 0;
  if (state.fileTx && !state.fileTx.done) {
    const reject = state.fileTx.reject;
    state.fileTx.done = true;
    state.fileTx = null;
    reject(new Error(reason || "device_disconnected"));
  } else {
    state.fileTx = null;
  }
  state.filePlaying = false;
  $("fileProgress").value = 0;
  $("fileInfo").textContent = "Playback reset";
  log("File playback reset");
}

function render(data) {
  state.devices = data;
  const mic = data.mic;
  const spk = data.spk;
  const camera = data.camera || { connected: false, enabled: false, formats: [] };
  const micState = mic.connected ? (mic.enabled ? "enabled" : "ready") : "off";
  const spkState = spk.connected ? (spk.enabled ? "enabled" : "ready") : "off";
  const cameraState = camera.connected ? (camera.enabled ? "enabled" : "ready") : "off";
  log(`State updated: mic=${micState} spk=${spkState} camera=${cameraState}`);
  if (!spk.connected) resetFilePlaybackState("device_disconnected");
  const any = mic.connected || spk.connected || camera.connected;
  setPill($("usbStatus"), any ? "USB connected" : "USB idle", any ? "ok" : "");
  setPill($("micState"), mic.connected ? (mic.enabled ? "Enabled" : "Ready") : "Disconnected", mic.connected ? "ok" : "");
  setPill($("spkState"), spk.connected ? (spk.enabled ? "Enabled" : "Ready") : "Disconnected", spk.connected ? "ok" : "");
  $("deviceSummary").textContent = any ? "USB device ready" : "Waiting for USB device";
  const audioVid = mic.vid || spk.vid;
  const audioPid = mic.pid || spk.pid;
  $("vidPid").textContent = audioVid && audioPid ? `${audioVid.toString(16)}:${audioPid.toString(16)}` : "--";
  $("product").textContent = mic.product || spk.product || "--";
  $("interfaces").textContent = `MIC ${mic.iface_num || "--"} / SPK ${spk.iface_num || "--"} / CAM ${camera.connected ? "yes" : "--"}`;
  updateControls("mic", mic);
  updateControls("spk", spk);
  updateCamera(camera);
  const spkReady = spk.connected && spk.enabled;
  setToolGroupDisabled("toneGroup", !spkReady || state.filePlaying || state.toneSending);
  setToolGroupDisabled("fileGroup", !spkReady);
  if (spkReady) {
    $("audioFile").disabled = state.filePlaying;
    $("playFile").disabled = state.filePlaying || state.spkResetting || !$("audioFile").files.length;
    $("stopFile").disabled = !state.filePlaying;
  }
  if (!mic.connected || !mic.enabled) updateMicMeter(0);
}

async function refreshDevices() {
  try {
    const res = await fetch("/api/devices");
    render(await res.json());
  } catch (err) {
    log(`Device refresh failed: ${err.message}`);
  }
}

function ensureAudio() {
  if (!state.audioCtx) {
    state.audioCtx = new AudioContext();
  }
  if (state.audioCtx.state === "suspended") state.audioCtx.resume();
  return state.audioCtx;
}

async function ensureMicPlayer(format) {
  const ctx = ensureAudio();
  if (!ctx.audioWorklet || state.micWorkletFailed) return ensureScriptMicPlayer(ctx, format);
  try {
    if (!state.micWorkletReady) state.micWorkletReady = ctx.audioWorklet.addModule("/assets/mic-player-worklet.js");
    await state.micWorkletReady;
  } catch (err) {
    state.micWorkletReady = null;
    state.micWorkletFailed = true;
    log(`MIC worklet unavailable, fallback to script player: ${err.message}`);
    return ensureScriptMicPlayer(ctx, format);
  }

  const channels = Math.max(1, Number(format.channels) || 1);
  const formatKey = `${format.sampleRate}:${channels}:${format.bitDepth}:${format.subframeSize}`;
  if (state.micPlayer && state.micWorkletNode && state.micWorkletFormatKey === formatKey) return state.micPlayer;
  if (state.micWorkletNode) state.micWorkletNode.disconnect();
  if (state.micScriptPlayer) {
    state.micScriptPlayer.node.disconnect();
    state.micScriptPlayer = null;
  }

  let node = null;
  try {
    node = new AudioWorkletNode(ctx, "mic-player", { outputChannelCount: [channels] });
  } catch (err) {
    state.micWorkletFailed = true;
    log(`MIC worklet node failed, fallback to script player: ${err.message}`);
    return ensureScriptMicPlayer(ctx, format);
  }
  node.port.onmessage = (event) => {
    if (event.data && event.data.type === "level") updateMicMeter(Math.max(event.data.level || 0, state.micLevel * 0.72));
  };
  node.connect(ctx.destination);
  node.port.postMessage({ type: "format", sampleRate: format.sampleRate, channels, bitDepth: format.bitDepth, subframeSize: format.subframeSize });
  state.micWorkletNode = node;
  state.micWorkletFormatKey = formatKey;
  state.micPlayer = { send: (pcmBuffer) => node.port.postMessage({ type: "pcm", pcm: pcmBuffer }, [pcmBuffer]) };
  log(`MIC worklet ready: ${format.sampleRate}Hz ${channels}ch ${format.bitDepth}bit`);
  return state.micPlayer;
}

function currentMicFormat() {
  const mic = state.devices && state.devices.mic;
  if (!mic || !mic.sample_freq) return null;
  const alt = mic.alts.find((item) => item.alt === mic.selected_alt) || mic.alts[0];
  if (!alt) return null;
  return {
    mic,
    alt,
    sampleRate: mic.sample_freq,
    channels: alt.channels,
    bitDepth: alt.bit_resolution,
    subframeSize: alt.subframe_size || Math.ceil(alt.bit_resolution / 8),
  };
}

function readPcmSample(dv, bytes, offset, bitDepth, subframeSize) {
  if (bitDepth === 8 && subframeSize === 1) return (bytes[offset] - 128) / 128;
  if (bitDepth <= 16 && subframeSize >= 2) return dv.getInt16(offset, true) / 32768;
  if (bitDepth <= 24 && subframeSize >= 3) {
    let value = bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16);
    if (value & 0x800000) value |= 0xff000000;
    return value / 8388608;
  }
  if (bitDepth <= 32 && subframeSize >= 4) return dv.getInt32(offset, true) / 2147483648;
  return 0;
}

function ensureScriptMicPlayer(ctx, format) {
  const channels = Math.max(1, Number(format.channels) || 1);
  const formatKey = `script:${format.sampleRate}:${channels}:${format.bitDepth}:${format.subframeSize}`;
  if (state.micPlayer && state.micScriptPlayer && state.micWorkletFormatKey === formatKey) return state.micPlayer;
  if (state.micWorkletNode) {
    state.micWorkletNode.disconnect();
    state.micWorkletNode = null;
  }
  if (state.micScriptPlayer) state.micScriptPlayer.node.disconnect();

  const capacityFrames = Math.max(1024, Math.ceil(format.sampleRate * 0.6));
  const player = {
    node: ctx.createScriptProcessor(1024, 0, channels),
    channels,
    capacityFrames,
    buffer: new Float32Array(capacityFrames * channels),
    readFrame: 0,
    writeFrame: 0,
    availableFrames: 0,
    readFrac: 0,
  };
  player.node.onaudioprocess = (event) => processScriptMicPlayer(event, player, format);
  player.node.connect(ctx.destination);
  state.micScriptPlayer = player;
  state.micWorkletFormatKey = formatKey;
  state.micPlayer = { send: (pcmBuffer) => pushScriptMicPcm(player, format, new Uint8Array(pcmBuffer)) };
  log(`MIC script ring player ready: ${format.sampleRate}Hz ${channels}ch ${format.bitDepth}bit`);
  return state.micPlayer;
}

function pushScriptMicPcm(player, format, bytes) {
  const frameBytes = player.channels * format.subframeSize;
  const frames = Math.floor(bytes.byteLength / frameBytes);
  if (frames <= 0) return;
  if (frames >= player.capacityFrames) bytes = bytes.subarray((frames - player.capacityFrames + 1) * frameBytes);
  const usableFrames = Math.floor(bytes.byteLength / frameBytes);
  const overflow = Math.max(0, player.availableFrames + usableFrames - player.capacityFrames);
  if (overflow > 0) {
    player.readFrame = (player.readFrame + overflow) % player.capacityFrames;
    player.availableFrames -= overflow;
    player.readFrac = 0;
  }

  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  let sumSquares = 0;
  let peak = 0;
  for (let i = 0; i < usableFrames; i++) {
    for (let ch = 0; ch < player.channels; ch++) {
      const sample = readPcmSample(dv, bytes, i * frameBytes + ch * format.subframeSize, format.bitDepth, format.subframeSize);
      player.buffer[(player.writeFrame * player.channels) + ch] = sample;
      sumSquares += sample * sample;
      peak = Math.max(peak, Math.abs(sample));
    }
    player.writeFrame = (player.writeFrame + 1) % player.capacityFrames;
  }
  player.availableFrames += usableFrames;
  const rms = Math.sqrt(sumSquares / Math.max(1, usableFrames * player.channels));
  updateMicMeter(Math.max(Math.min(1, Math.max(rms * 4, peak * 0.8)), state.micLevel * 0.72));
}

function processScriptMicPlayer(event, player, format) {
  const output = event.outputBuffer;
  const frames = output.length;
  const step = format.sampleRate / output.sampleRate;
  for (let i = 0; i < frames; i++) {
    if (player.availableFrames <= Math.ceil(player.readFrac)) {
      for (let ch = 0; ch < output.numberOfChannels; ch++) output.getChannelData(ch)[i] = 0;
      continue;
    }
    const sourceOffset = Math.floor(player.readFrac);
    const sourceFrame = (player.readFrame + sourceOffset) % player.capacityFrames;
    for (let ch = 0; ch < output.numberOfChannels; ch++) {
      const sourceCh = Math.min(ch, player.channels - 1);
      output.getChannelData(ch)[i] = player.buffer[(sourceFrame * player.channels) + sourceCh];
    }
    player.readFrac += step;
    const consumeFrames = Math.floor(player.readFrac);
    if (consumeFrames > 0) {
      player.readFrame = (player.readFrame + consumeFrames) % player.capacityFrames;
      player.availableFrames = Math.max(0, player.availableFrames - consumeFrames);
      player.readFrac -= consumeFrames;
    }
  }
}

function updateMicMeter(level) {
  state.micLevel = Math.max(0, Math.min(1, level));
  const percent = Math.round(state.micLevel * 100);
  const bar = $("micMeter").firstElementChild;
  bar.style.width = `${percent}%`;
  bar.classList.toggle("hot", percent >= 90);
  $("micLevelText").textContent = `${percent}%`;
}

async function playMicPcm(pcmBuffer) {
  const format = currentMicFormat();
  if (!format) return;
  const { sampleRate, channels, bitDepth, subframeSize } = format;
  const frameBytes = channels * subframeSize;
  const frames = Math.floor(pcmBuffer.byteLength / frameBytes);
  if (frames <= 0) return;
  try {
    const player = await ensureMicPlayer(format);
    player.send(pcmBuffer);
    state.micFrames++;
    if (state.micFrames === 1) log(`MIC playback started: ${sampleRate}Hz ${channels}ch ${bitDepth}bit`);
  } catch (err) {
    log(`MIC playback failed: ${err.message}`);
  }
}

function handleBinary(buf) {
  const view = new Uint8Array(buf);
  if (view.length <= 4 || view[0] !== 0x01) return;
  const len = view[2] | (view[3] << 8);
  if (len !== view.length - 4) return;
  playMicPcm(buf.slice(4));
}

function connectAudioWs() {
  const url = `ws://${location.host}/ws`;
  let audioWs = null;
  try {
    audioWs = new WebSocket(url);
  } catch (err) {
    log(`Audio WebSocket constructor failed: ${err.message}`);
    return;
  }
  audioWs.binaryType = "arraybuffer";
  state.audioWs = audioWs;
  audioWs.onopen = () => {
    setPill($("audioWsStatus"), "Audio WS online", "ok");
    log("Audio WebSocket connected");
    sendAudioJson({ type: "get_state" });
    refreshDevices();
  };
  audioWs.onclose = (event) => {
    setPill($("audioWsStatus"), "Audio WS offline", "warn");
    log(`Audio WebSocket disconnected: code=${event.code} reason=${event.reason || "-"} clean=${event.wasClean}`);
    setTimeout(connectAudioWs, 1000);
  };
  audioWs.onerror = () => log(`Audio WebSocket error: ${url} state=${audioWs.readyState}`);
  audioWs.onmessage = (event) => {
    if (typeof event.data === "string") {
      const msg = JSON.parse(event.data);
      if (msg.type === "state") {
        render(msg);
      }
      if (msg.type === "error") log(`${msg.code}: ${msg.message}`);
      if (msg.type === "spk_reset_done") handleSpkResetDone();
      if (msg.type === "spk_refill_budget") handleSpkRefillBudget(msg.bytes || 0);
    } else {
      handleBinary(event.data);
    }
  };
}

function selectedFormat(prefix) {
  const [alt, freq] = $(`${prefix}Format`).value.split(":").map((v) => Number(v));
  return { alt, sample_freq: freq };
}

function writePcmSample(dv, bytes, offset, sample, bitDepth, subframeSize) {
  const max = Math.pow(2, bitDepth - 1) - 1;
  const min = -Math.pow(2, bitDepth - 1);
  const value = Math.max(min, Math.min(max, Math.round(sample * max)));
  if (bitDepth === 8 && subframeSize === 1) {
    bytes[offset] = Math.max(0, Math.min(255, Math.round((sample + 1) * 127.5)));
  } else if (bitDepth <= 16 && subframeSize >= 2) {
    dv.setInt16(offset, value, true);
  } else if (bitDepth <= 24 && subframeSize >= 3) {
    bytes[offset] = value & 0xff;
    bytes[offset + 1] = (value >> 8) & 0xff;
    bytes[offset + 2] = (value >> 16) & 0xff;
    if (subframeSize >= 4) bytes[offset + 3] = value < 0 ? 0xff : 0x00;
  } else if (bitDepth <= 32 && subframeSize >= 4) {
    dv.setInt32(offset, value, true);
  }
}

function currentSpkFormat() {
  const spk = state.devices.spk;
  const alt = spk.alts.find((item) => item.alt === spk.selected_alt) || spk.alts[0];
  return {
    spk,
    alt,
    sampleRate: spk.sample_freq,
    channels: alt.channels,
    bitDepth: alt.bit_resolution,
    subframeSize: alt.subframe_size || Math.ceil(alt.bit_resolution / 8),
  };
}

function sendSpkPacket(part) {
  const packet = new Uint8Array(part.length + 4);
  packet[0] = 0x02;
  packet[1] = 0;
  packet[2] = part.length & 0xff;
  packet[3] = (part.length >> 8) & 0xff;
  packet.set(part, 4);
  state.audioWs.send(packet);
}

function refillBudgetPayloadBytes(frameBytes, remainingBytes) {
  const budget = Math.min(state.spkRefillBudget, SPK_MAX_PAYLOAD, remainingBytes);
  return Math.floor(budget / frameBytes) * frameBytes;
}

function makeTonePcm() {
  const { sampleRate, channels, bitDepth, subframeSize } = currentSpkFormat();
  const freq = Number($("toneFreq").value || 1000);
  const ms = Number($("toneMs").value || 1000);
  const frames = Math.floor(sampleRate * ms / 1000);
  const pcm = new Uint8Array(frames * channels * subframeSize);
  const dv = new DataView(pcm.buffer);
  for (let i = 0; i < frames; i++) {
    const sample = Math.sin(2 * Math.PI * freq * i / sampleRate) * 0.35;
    for (let ch = 0; ch < channels; ch++) {
      writePcmSample(dv, pcm, (i * channels + ch) * subframeSize, sample, bitDepth, subframeSize);
    }
  }
  return pcm;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function sendTone() {
  if (state.toneSending || state.filePlaying || !state.audioWs || state.audioWs.readyState !== WebSocket.OPEN) return;
  state.toneSending = true;
  $("playTone").disabled = true;
  try {
    const pcm = makeTonePcm();
    const format = currentSpkFormat();
    await sendPcmByRefillBudget(pcm, format, () => {});
    log("Test tone sent");
  } catch (err) {
    log(`Test tone failed: ${err.message}`);
  } finally {
    state.toneSending = false;
    render(state.devices);
  }
}

async function decodeAudioFile(file) {
  const ctx = ensureAudio();
  const data = await file.arrayBuffer();
  return await ctx.decodeAudioData(data);
}

async function resampleAudioBuffer(buffer, sampleRate) {
  if (buffer.sampleRate === sampleRate) return buffer;
  const length = Math.max(1, Math.ceil(buffer.duration * sampleRate));
  const offline = new OfflineAudioContext(buffer.numberOfChannels, length, sampleRate);
  const source = offline.createBufferSource();
  source.buffer = buffer;
  source.connect(offline.destination);
  source.start(0);
  return await offline.startRendering();
}

function audioBufferToSpkPcm(buffer, format) {
  return audioBufferChunkToSpkPcm(buffer, format, 0, buffer.length);
}

function audioBufferChunkToSpkPcm(buffer, format, frameOff, frames) {
  const { channels, bitDepth, subframeSize } = format;
  const pcm = new Uint8Array(frames * channels * subframeSize);
  const dv = new DataView(pcm.buffer);
  const inputs = [];
  for (let ch = 0; ch < buffer.numberOfChannels; ch++) inputs.push(buffer.getChannelData(ch));
  for (let i = 0; i < frames; i++) {
    const srcIndex = frameOff + i;
    for (let ch = 0; ch < channels; ch++) {
      let sample = 0;
      if (buffer.numberOfChannels === 1) {
        sample = inputs[0][srcIndex];
      } else if (channels === 1) {
        for (let src = 0; src < buffer.numberOfChannels; src++) sample += inputs[src][srcIndex];
        sample /= buffer.numberOfChannels;
      } else {
        sample = inputs[Math.min(ch, buffer.numberOfChannels - 1)][srcIndex];
      }
      writePcmSample(dv, pcm, (i * channels + ch) * subframeSize, sample, bitDepth, subframeSize);
    }
  }
  return pcm;
}

function pumpFileTx() {
  const tx = state.fileTx;
  if (!tx || tx.done) return;
  try {
    while (tx.off < tx.pcm.length) {
      if (state.fileStop) throw new Error("stopped");
      if (!state.audioWs || state.audioWs.readyState !== WebSocket.OPEN) throw new Error("Audio WebSocket disconnected");
      const partLen = refillBudgetPayloadBytes(tx.frameBytes, tx.pcm.length - tx.off);
      if (partLen <= 0) {
        logSpkBudgetWait("pcm", tx.pcm.length - tx.off);
        return;
      }
      clearSpkBudgetWait();
      if (state.audioWs.bufferedAmount > SPK_WS_BUFFER_LIMIT) {
        logSpkBufferWait("pcm");
        setTimeout(pumpFileTx, 5);
        return;
      }
      clearSpkBufferWait();
      const part = tx.pcm.subarray(tx.off, tx.off + partLen);
      sendSpkPacket(part);
      tx.off += part.length;
      state.spkRefillBudget -= part.length;
      tx.onProgress(Math.min(100, Math.round(tx.off * 100 / tx.pcm.length)));
    }
    if (tx.off >= tx.pcm.length) {
      tx.done = true;
      const resolve = tx.resolve;
      state.fileTx = null;
      resolve();
    }
  } catch (err) {
    tx.done = true;
    const reject = tx.reject;
    state.fileTx = null;
    reject(err);
  }
}

function pumpBufferTx() {
  const tx = state.fileTx;
  if (!tx || tx.done) return;
  try {
    while (tx.frameOff < tx.buffer.length) {
      if (state.fileStop) throw new Error("stopped");
      if (!state.audioWs || state.audioWs.readyState !== WebSocket.OPEN) throw new Error("Audio WebSocket disconnected");
      const partLen = refillBudgetPayloadBytes(tx.frameBytes, (tx.buffer.length - tx.frameOff) * tx.frameBytes);
      if (partLen <= 0) {
        logSpkBudgetWait("buffer", (tx.buffer.length - tx.frameOff) * tx.frameBytes);
        return;
      }
      clearSpkBudgetWait();
      const frames = Math.min(Math.floor(partLen / tx.frameBytes), tx.buffer.length - tx.frameOff);
      if (state.audioWs.bufferedAmount > SPK_WS_BUFFER_LIMIT) {
        logSpkBufferWait("buffer");
        setTimeout(pumpBufferTx, 5);
        return;
      }
      clearSpkBufferWait();
      const part = audioBufferChunkToSpkPcm(tx.buffer, tx.format, tx.frameOff, frames);
      sendSpkPacket(part);
      tx.frameOff += frames;
      state.spkRefillBudget -= part.length;
      tx.onProgress(Math.min(100, Math.round(tx.frameOff * 100 / tx.buffer.length)));
    }
    if (tx.frameOff >= tx.buffer.length) {
      tx.done = true;
      const resolve = tx.resolve;
      state.fileTx = null;
      resolve();
    }
  } catch (err) {
    tx.done = true;
    const reject = tx.reject;
    state.fileTx = null;
    reject(err);
  }
}

function handleSpkRefillBudget(bytes) {
  if (state.spkResetting || !state.fileTx) {
    log(`Drop stale SPK refill budget: bytes=${bytes} resetting=${state.spkResetting}`);
    return;
  }
  state.spkRefillBudget = Math.max(state.spkRefillBudget, Math.max(0, Number(bytes) || 0));
  clearSpkBudgetWait();
  log(`SPK refill budget received: bytes=${bytes} budget=${state.spkRefillBudget}`);
  if (state.fileTx && state.fileTx.buffer) {
    pumpBufferTx();
  } else {
    pumpFileTx();
  }
}

function handleSpkResetDone() {
  state.spkRefillBudget = 0;
  state.spkResetting = false;
  clearSpkBudgetWait();
  clearSpkBufferWait();
  log("SPK reset done");
  if (state.devices) render(state.devices);
}

function logSpkBudgetWait(kind, remainingBytes) {
  const now = performance.now();
  if (!state.spkBudgetWaitSince) state.spkBudgetWaitSince = now;
  if (now - state.spkBudgetWaitSince >= 100 && now - state.spkLastBudgetWaitLog >= 500) {
    state.spkLastBudgetWaitLog = now;
    log(`SPK waiting refill budget: mode=${kind} wait=${Math.round(now - state.spkBudgetWaitSince)}ms remaining=${remainingBytes} budget=${state.spkRefillBudget}`);
  }
}

function clearSpkBudgetWait() {
  state.spkBudgetWaitSince = 0;
}

function logSpkBufferWait(kind) {
  const now = performance.now();
  if (!state.spkBufferWaitSince) state.spkBufferWaitSince = now;
  if (now - state.spkBufferWaitSince >= 100 && now - state.spkLastBufferWaitLog >= 500) {
    state.spkLastBufferWaitLog = now;
    log(`SPK WebSocket buffered wait: mode=${kind} wait=${Math.round(now - state.spkBufferWaitSince)}ms buffered=${state.audioWs ? state.audioWs.bufferedAmount : 0}`);
  }
}

function clearSpkBufferWait() {
  state.spkBufferWaitSince = 0;
}

function sendPcmByRefillBudget(pcm, format, onProgress) {
  const { channels, subframeSize } = format;
  const frameBytes = channels * subframeSize;
  return new Promise((resolve, reject) => {
    state.spkRefillBudget = 0;
    clearSpkBudgetWait();
    clearSpkBufferWait();
    state.fileTx = { pcm, frameBytes, off: 0, onProgress, resolve, reject, done: false };
    sendAudioJson({ type: "request_spk_refill_budget" });
  });
}

function sendAudioBufferByRefillBudget(buffer, format, onProgress) {
  const { channels, subframeSize } = format;
  const frameBytes = channels * subframeSize;
  return new Promise((resolve, reject) => {
    state.spkRefillBudget = 0;
    clearSpkBudgetWait();
    clearSpkBufferWait();
    state.fileTx = { buffer, format, frameBytes, frameOff: 0, onProgress, resolve, reject, done: false };
    sendAudioJson({ type: "request_spk_refill_budget" });
  });
}

async function playSelectedFile() {
  const file = $("audioFile").files[0];
  if (!file || state.filePlaying || state.spkResetting || !state.audioWs || state.audioWs.readyState !== WebSocket.OPEN) return;
  state.filePlaying = true;
  state.fileStop = false;
  render(state.devices);
  try {
    const format = currentSpkFormat();
    $("fileInfo").textContent = `Decoding ${file.name}`;
    $("fileProgress").value = 0;
    const decoded = await decodeAudioFile(file);
    if (state.fileStop) throw new Error("device_disconnected");
    const rendered = await resampleAudioBuffer(decoded, format.sampleRate);
    if (state.fileStop) throw new Error("device_disconnected");
    $("fileInfo").textContent = `${file.name} -> ${format.sampleRate}Hz ${format.channels}ch ${format.bitDepth}bit`;
    log(`File playback started: ${file.name}`);
    await sendAudioBufferByRefillBudget(rendered, format, (value) => ($("fileProgress").value = value));
    $("fileInfo").textContent = `Done: ${file.name}`;
    log("File playback done");
  } catch (err) {
    if (err.message === "stopped") {
      $("fileInfo").textContent = "Stopped";
      log("File playback stopped");
    } else if (err.message === "device_disconnected") {
      $("fileProgress").value = 0;
      $("fileInfo").textContent = "Playback reset";
    } else {
      $("fileInfo").textContent = "File playback failed";
      log(`File playback failed: ${err.message}`);
    }
  } finally {
    state.fileTx = null;
    state.filePlaying = false;
    state.fileStop = false;
    render(state.devices);
  }
}

function stopFilePlayback() {
  if (!state.filePlaying) return;
  state.fileStop = true;
  state.spkResetting = true;
  state.spkRefillBudget = 0;
  clearSpkBudgetWait();
  clearSpkBufferWait();
  if (state.fileTx && !state.fileTx.done) {
    const reject = state.fileTx.reject;
    state.fileTx.done = true;
    state.fileTx = null;
    reject(new Error("stopped"));
  }
  sendAudioJson({ type: "stop_spk_playback" });
  log("SPK stop command sent");
  if (state.devices) render(state.devices);
}

function bindControls() {
  $("micEnabled").onclick = (e) => {
    const enabled = e.currentTarget.dataset.enabled !== "1";
    if (enabled) ensureAudio();
    sendAudioJson({ type: "set_mic_enabled", enabled });
  };
  $("spkEnabled").onclick = (e) => {
    const enabled = e.currentTarget.dataset.enabled !== "1";
    sendAudioJson({ type: "set_spk_enabled", enabled });
  };
  $("micMute").onchange = (e) => sendAudioJson({ type: "set_mic_mute", mute: e.target.checked });
  $("spkMute").onchange = (e) => sendAudioJson({ type: "set_spk_mute", mute: e.target.checked });
  $("micVolume").onchange = (e) => sendAudioJson({ type: "set_mic_volume", volume: Number(e.target.value) });
  $("spkVolume").onchange = (e) => sendAudioJson({ type: "set_spk_volume", volume: Number(e.target.value) });
  $("micFormat").onchange = () => sendAudioJson({ type: "set_mic_format", ...selectedFormat("mic") });
  $("spkFormat").onchange = () => sendAudioJson({ type: "set_spk_format", ...selectedFormat("spk") });
  $("cameraEnabled").onclick = (e) => {
    const enabled = e.currentTarget.dataset.enabled !== "1";
    if (enabled) {
      $("cameraPlaceholder").textContent = "Camera starting";
    }
    sendCameraJson({ type: "set_camera_enabled", enabled });
    if (!enabled) {
      setTimeout(() => stopCameraStream("Camera disabled"), 50);
    }
  };
  $("cameraFormat").onchange = (e) => {
    log(`Camera format selected: ${e.target.value}`);
    sendCameraFormat();
  };
  $("cameraResolution").onchange = (e) => {
    log(`Camera resolution selected: ${e.target.options[e.target.selectedIndex].textContent}`);
    sendCameraFormat();
  };
  $("cameraStream").onerror = () => {
    log("Camera stream load error");
    stopCameraStream("Camera stream error");
  };
  $("playTone").onclick = sendTone;
  $("audioFile").onchange = () => {
    const file = $("audioFile").files[0];
    $("fileInfo").textContent = file ? `${file.name} (${Math.round(file.size / 1024)} KB)` : "No file selected";
    $("fileProgress").value = 0;
    render(state.devices);
  };
  $("playFile").onclick = playSelectedFile;
  $("stopFile").onclick = stopFilePlayback;
}

try {
  bindControls();
  refreshDevices();
  connectAudioWs();
} catch (err) {
  log(`Startup failed: ${err.message}`);
}
