const $ = (id) => document.getElementById(id);
const state = {
  ws: null,
  devices: null,
  audioCtx: null,
  nextMicTime: 0,
  toneSending: false,
  filePlaying: false,
  fileStop: false,
  fileTx: null,
  spkRefillBudget: 0,
  micFrames: 0,
  micLevel: 0,
};
const MIC_PLAYBACK_TARGET_LATENCY = 0.08;
const MIC_PLAYBACK_MAX_QUEUE = 0.20;
const SPK_MAX_PAYLOAD = 4092;
const SPK_WS_BUFFER_LIMIT = 65536;

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

function sendJson(obj) {
  if (state.ws && state.ws.readyState === WebSocket.OPEN) {
    state.ws.send(JSON.stringify(obj));
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
  enabled.checked = !!stream.enabled;
  mute.checked = !!stream.mute;
  volume.value = stream.volume || 80;
  fillFormats(format, stream);
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
  log(`State updated: mic=${mic.connected ? (mic.enabled ? "enabled" : "ready") : "off"} spk=${spk.connected ? (spk.enabled ? "enabled" : "ready") : "off"}`);
  if (!spk.connected) resetFilePlaybackState("device_disconnected");
  const any = mic.connected || spk.connected;
  setPill($("usbStatus"), any ? "USB connected" : "USB idle", any ? "ok" : "");
  setPill($("micState"), mic.connected ? (mic.enabled ? "Enabled" : "Ready") : "Disconnected", mic.connected ? "ok" : "");
  setPill($("spkState"), spk.connected ? (spk.enabled ? "Enabled" : "Ready") : "Disconnected", spk.connected ? "ok" : "");
  $("deviceSummary").textContent = any ? "USB audio device ready" : "Waiting for USB audio device";
  $("vidPid").textContent = any ? `${(mic.vid || spk.vid).toString(16)}:${(mic.pid || spk.pid).toString(16)}` : "--";
  $("product").textContent = mic.product || spk.product || "--";
  $("interfaces").textContent = `MIC ${mic.iface_num || "--"} / SPK ${spk.iface_num || "--"}`;
  updateControls("mic", mic);
  updateControls("spk", spk);
  const spkReady = spk.connected && spk.enabled;
  setToolGroupDisabled("toneGroup", !spkReady || state.filePlaying || state.toneSending);
  setToolGroupDisabled("fileGroup", !spkReady);
  if (spkReady) {
    $("audioFile").disabled = state.filePlaying;
    $("playFile").disabled = state.filePlaying || !$("audioFile").files.length;
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
    state.nextMicTime = state.audioCtx.currentTime;
  }
  if (state.audioCtx.state === "suspended") state.audioCtx.resume();
  return state.audioCtx;
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

function updateMicMeter(level) {
  state.micLevel = Math.max(0, Math.min(1, level));
  const percent = Math.round(state.micLevel * 100);
  const bar = $("micMeter").firstElementChild;
  bar.style.width = `${percent}%`;
  bar.classList.toggle("hot", percent >= 90);
  $("micLevelText").textContent = `${percent}%`;
}

function playMicPcm(payload) {
  const format = currentMicFormat();
  if (!format) return;
  const { sampleRate, channels, bitDepth, subframeSize } = format;
  const ctx = ensureAudio();
  const frameBytes = channels * subframeSize;
  const frames = Math.floor(payload.byteLength / frameBytes);
  if (frames <= 0) return;
  const dv = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
  const buffer = ctx.createBuffer(channels, frames, sampleRate);
  let sumSquares = 0;
  let peak = 0;
  for (let ch = 0; ch < channels; ch++) {
    const out = buffer.getChannelData(ch);
    for (let i = 0; i < frames; i++) {
      const sample = readPcmSample(dv, payload, i * frameBytes + ch * subframeSize, bitDepth, subframeSize);
      out[i] = sample;
      sumSquares += sample * sample;
      peak = Math.max(peak, Math.abs(sample));
    }
  }
  // Use RMS for stable visual volume and blend in peak so short sounds are visible.
  const rms = Math.sqrt(sumSquares / Math.max(1, frames * channels));
  const level = Math.min(1, Math.max(rms * 4, peak * 0.8));
  updateMicMeter(Math.max(level, state.micLevel * 0.72));
  const source = ctx.createBufferSource();
  source.buffer = buffer;
  source.connect(ctx.destination);
  const queued = state.nextMicTime - ctx.currentTime;
  if (queued > MIC_PLAYBACK_MAX_QUEUE) state.nextMicTime = ctx.currentTime + MIC_PLAYBACK_TARGET_LATENCY;
  const start = Math.max(ctx.currentTime + MIC_PLAYBACK_TARGET_LATENCY, state.nextMicTime);
  source.start(start);
  state.nextMicTime = start + buffer.duration;
  state.micFrames++;
  if (state.micFrames === 1) log(`MIC playback started: ${sampleRate}Hz ${channels}ch ${bitDepth}bit`);
}

function handleBinary(buf) {
  const view = new Uint8Array(buf);
  if (view.length <= 4 || view[0] !== 0x01) return;
  const len = view[2] | (view[3] << 8);
  if (len !== view.length - 4) return;
  playMicPcm(view.subarray(4));
}

function connectWs() {
  const url = `ws://${location.host}/ws`;
  let ws = null;
  try {
    ws = new WebSocket(url);
  } catch (err) {
    log(`WebSocket constructor failed: ${err.message}`);
    return;
  }
  ws.binaryType = "arraybuffer";
  state.ws = ws;
  ws.onopen = () => {
    setPill($("wsStatus"), "WS online", "ok");
    log("WebSocket connected");
    sendJson({ type: "get_state" });
    refreshDevices();
  };
  ws.onclose = (event) => {
    setPill($("wsStatus"), "WS offline", "warn");
    log(`WebSocket disconnected: code=${event.code} reason=${event.reason || "-"} clean=${event.wasClean}`);
    setTimeout(connectWs, 1000);
  };
  ws.onerror = () => log(`WebSocket error: ${url} state=${ws.readyState}`);
  ws.onmessage = (event) => {
    if (typeof event.data === "string") {
      const msg = JSON.parse(event.data);
      if (msg.type === "state") {
        render(msg);
      }
      if (msg.type === "error") log(`${msg.code}: ${msg.message}`);
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
  state.ws.send(packet);
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
  if (state.toneSending || state.filePlaying || !state.ws || state.ws.readyState !== WebSocket.OPEN) return;
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
      if (!state.ws || state.ws.readyState !== WebSocket.OPEN) throw new Error("WebSocket disconnected");
      const partLen = refillBudgetPayloadBytes(tx.frameBytes, tx.pcm.length - tx.off);
      if (partLen <= 0) return;
      if (state.ws.bufferedAmount > SPK_WS_BUFFER_LIMIT) {
        setTimeout(pumpFileTx, 5);
        return;
      }
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
      if (!state.ws || state.ws.readyState !== WebSocket.OPEN) throw new Error("WebSocket disconnected");
      const partLen = refillBudgetPayloadBytes(tx.frameBytes, (tx.buffer.length - tx.frameOff) * tx.frameBytes);
      if (partLen <= 0) return;
      const frames = Math.min(Math.floor(partLen / tx.frameBytes), tx.buffer.length - tx.frameOff);
      if (state.ws.bufferedAmount > SPK_WS_BUFFER_LIMIT) {
        setTimeout(pumpBufferTx, 5);
        return;
      }
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
  state.spkRefillBudget = Math.max(state.spkRefillBudget, Math.max(0, Number(bytes) || 0));
  if (state.fileTx && state.fileTx.buffer) {
    pumpBufferTx();
  } else {
    pumpFileTx();
  }
}

function sendPcmByRefillBudget(pcm, format, onProgress) {
  const { channels, subframeSize } = format;
  const frameBytes = channels * subframeSize;
  return new Promise((resolve, reject) => {
    state.spkRefillBudget = 0;
    state.fileTx = { pcm, frameBytes, off: 0, onProgress, resolve, reject, done: false };
    sendJson({ type: "request_spk_refill_budget" });
  });
}

function sendAudioBufferByRefillBudget(buffer, format, onProgress) {
  const { channels, subframeSize } = format;
  const frameBytes = channels * subframeSize;
  return new Promise((resolve, reject) => {
    state.spkRefillBudget = 0;
    state.fileTx = { buffer, format, frameBytes, frameOff: 0, onProgress, resolve, reject, done: false };
    sendJson({ type: "request_spk_refill_budget" });
  });
}

async function playSelectedFile() {
  const file = $("audioFile").files[0];
  if (!file || state.filePlaying || !state.ws || state.ws.readyState !== WebSocket.OPEN) return;
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
  if (state.fileTx && !state.fileTx.done) {
    const reject = state.fileTx.reject;
    state.fileTx.done = true;
    state.fileTx = null;
    reject(new Error("stopped"));
  }
  sendJson({ type: "stop_spk_playback" });
}

function bindControls() {
  $("micEnabled").onchange = (e) => {
    if (e.target.checked) ensureAudio();
    sendJson({ type: "set_mic_enabled", enabled: e.target.checked });
  };
  $("spkEnabled").onchange = (e) => sendJson({ type: "set_spk_enabled", enabled: e.target.checked });
  $("micMute").onchange = (e) => sendJson({ type: "set_mic_mute", mute: e.target.checked });
  $("spkMute").onchange = (e) => sendJson({ type: "set_spk_mute", mute: e.target.checked });
  $("micVolume").onchange = (e) => sendJson({ type: "set_mic_volume", volume: Number(e.target.value) });
  $("spkVolume").onchange = (e) => sendJson({ type: "set_spk_volume", volume: Number(e.target.value) });
  $("micFormat").onchange = () => sendJson({ type: "set_mic_format", ...selectedFormat("mic") });
  $("spkFormat").onchange = () => sendJson({ type: "set_spk_format", ...selectedFormat("spk") });
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
  connectWs();
} catch (err) {
  log(`Startup failed: ${err.message}`);
}
