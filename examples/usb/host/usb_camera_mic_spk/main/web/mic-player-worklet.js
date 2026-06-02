class MicPlayerProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.channels = 1;
    this.inputSampleRate = sampleRate;
    this.bitDepth = 16;
    this.subframeSize = 2;
    this.capacityFrames = sampleRate;
    this.buffer = new Float32Array(this.capacityFrames * this.channels);
    this.readFrame = 0;
    this.writeFrame = 0;
    this.availableFrames = 0;
    this.readFrac = 0;
    this.port.onmessage = (event) => this.handleMessage(event.data);
  }

  handleMessage(data) {
    if (!data || !data.type) return;
    if (data.type === "format") {
      this.configure(data);
    } else if (data.type === "pcm" && data.pcm) {
      this.pushPcm(new Uint8Array(data.pcm));
    }
  }

  configure(data) {
    const channels = Math.max(1, Math.min(8, Number(data.channels) || 1));
    const inputSampleRate = Math.max(8000, Number(data.sampleRate) || sampleRate);
    this.channels = channels;
    this.inputSampleRate = inputSampleRate;
    this.bitDepth = Number(data.bitDepth) || 16;
    this.subframeSize = Number(data.subframeSize) || Math.ceil(this.bitDepth / 8);
    this.capacityFrames = Math.max(1024, Math.ceil(inputSampleRate * 0.6));
    this.buffer = new Float32Array(this.capacityFrames * channels);
    this.readFrame = 0;
    this.writeFrame = 0;
    this.availableFrames = 0;
    this.readFrac = 0;
  }

  readSample(bytes, offset) {
    if (this.bitDepth === 8 && this.subframeSize === 1) return (bytes[offset] - 128) / 128;
    if (this.bitDepth <= 16 && this.subframeSize >= 2) {
      const value = bytes[offset] | (bytes[offset + 1] << 8);
      return (value & 0x8000 ? value - 0x10000 : value) / 32768;
    }
    if (this.bitDepth <= 24 && this.subframeSize >= 3) {
      let value = bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16);
      if (value & 0x800000) value |= 0xff000000;
      return value / 8388608;
    }
    if (this.bitDepth <= 32 && this.subframeSize >= 4) {
      const value = bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24);
      return value / 2147483648;
    }
    return 0;
  }

  pushPcm(bytes) {
    const frameBytes = this.channels * this.subframeSize;
    const frames = Math.floor(bytes.byteLength / frameBytes);
    if (frames <= 0) return;
    if (frames >= this.capacityFrames) {
      bytes = bytes.subarray((frames - this.capacityFrames + 1) * frameBytes);
    }

    let sumSquares = 0;
    let peak = 0;
    const usableFrames = Math.floor(bytes.byteLength / frameBytes);
    const overflow = Math.max(0, this.availableFrames + usableFrames - this.capacityFrames);
    if (overflow > 0) {
      this.readFrame = (this.readFrame + overflow) % this.capacityFrames;
      this.availableFrames -= overflow;
      this.readFrac = 0;
    }

    for (let i = 0; i < usableFrames; i++) {
      for (let ch = 0; ch < this.channels; ch++) {
        const sampleValue = this.readSample(bytes, i * frameBytes + ch * this.subframeSize);
        this.buffer[(this.writeFrame * this.channels) + ch] = sampleValue;
        sumSquares += sampleValue * sampleValue;
        peak = Math.max(peak, Math.abs(sampleValue));
      }
      this.writeFrame = (this.writeFrame + 1) % this.capacityFrames;
    }
    this.availableFrames += usableFrames;

    const rms = Math.sqrt(sumSquares / Math.max(1, usableFrames * this.channels));
    this.port.postMessage({ type: "level", level: Math.min(1, Math.max(rms * 4, peak * 0.8)), queuedMs: Math.round(this.availableFrames * 1000 / this.inputSampleRate) });
  }

  process(inputs, outputs) {
    const output = outputs[0];
    if (!output || output.length === 0) return true;
    const frames = output[0].length;
    const step = this.inputSampleRate / sampleRate;
    for (let i = 0; i < frames; i++) {
      if (this.availableFrames <= Math.ceil(this.readFrac)) {
        for (let ch = 0; ch < output.length; ch++) output[ch][i] = 0;
        continue;
      }
      const sourceOffset = Math.floor(this.readFrac);
      const sourceFrame = (this.readFrame + sourceOffset) % this.capacityFrames;
      for (let ch = 0; ch < output.length; ch++) {
        const sourceCh = Math.min(ch, this.channels - 1);
        output[ch][i] = this.buffer[(sourceFrame * this.channels) + sourceCh];
      }
      this.readFrac += step;
      const consumeFrames = Math.floor(this.readFrac);
      if (consumeFrames > 0) {
        this.readFrame = (this.readFrame + consumeFrames) % this.capacityFrames;
        this.availableFrames = Math.max(0, this.availableFrames - consumeFrames);
        this.readFrac -= consumeFrames;
      }
    }
    return true;
  }
}

registerProcessor("mic-player", MicPlayerProcessor);
