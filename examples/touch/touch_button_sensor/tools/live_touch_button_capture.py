#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Plot the public output of the touch_button_sensor example.

This tool deliberately consumes only the example's public-layer records:

* channel=... amplitude=... for the positive button amplitude;
* channel=... state=... amplitude=... for stable callback events.

Raw samples, baselines, fused_filtered and FSM thresholds belong to the
repository FSM diagnostic example and are plotted by live_fsm_capture.py.
"""

import argparse
from collections import defaultdict, deque
import queue
import re
import threading


ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
LOG_TIME = re.compile(r'\b[IDWEV]\s+\((\d+)\)')


def parse_line(line):
    """Return one public button record, or None for unrelated log text."""
    records = parse_records(line)
    return records[0] if records else None


def _parse_public_segment(segment, timestamp):
    channel = re.search(r'\bchannel=(\d+)', segment)
    amplitude = re.search(r'\bamplitude=(\d+)', segment)
    if channel is None or amplitude is None:
        return None
    state = re.search(r'\bstate=(ACTIVE|INACTIVE|active|inactive)\b', segment)
    if state is not None:
        return {
            'kind': 'event',
            'ch': int(channel.group(1)),
            'state': 1 if state.group(1).lower() == 'active' else 0,
            'amplitude': int(amplitude.group(1)),
            'time': timestamp,
        }
    return {
        'kind': 'amplitude',
        'ch': int(channel.group(1)),
        'amplitude': int(amplitude.group(1)),
        'time': timestamp,
    }


def parse_records(line):
    """Return every public button record found in one serial line.

    A native USB serial read can contain adjacent log lines. Split at each
    monitor timestamp so one merged read cannot drop a channel or event.
    """
    line = ANSI.sub('', line)
    timestamps = list(LOG_TIME.finditer(line))
    records = []
    for index, match in enumerate(timestamps):
        end = timestamps[index + 1].start() if index + 1 < len(timestamps) else len(line)
        row = _parse_public_segment(
            line[match.start():end], int(match.group(1)) / 1000.0
        )
        if row is not None:
            records.append(row)
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--port', help='Serial device; close idf.py monitor first')
    source.add_argument('--file', help='Replay a saved touch-button log')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--channels', type=int, nargs='+', default=[6, 7])
    parser.add_argument('--refresh-ms', type=int, default=200)
    parser.add_argument('--window', type=float, default=60)
    parser.add_argument('--record', help='Save serial text to a NEW log file')
    parser.add_argument('--png', help='Render a saved log to PNG without a GUI')
    args = parser.parse_args()

    if args.window <= 0 or args.refresh_ms <= 0:
        parser.error('--window and --refresh-ms must be positive')
    if args.png and not args.file:
        parser.error('--png requires --file')
    if args.record and not args.port:
        parser.error('--record requires --port')

    import matplotlib
    if args.png:
        matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    channels = list(dict.fromkeys(args.channels))
    fig, axes = plt.subplots(
        len(channels), 2, squeeze=False,
        figsize=(12, 3.0 * len(channels)),
        sharex=True, constrained_layout=True,
    )
    fig.suptitle('touch_button_sensor | public amplitude and events')
    data = defaultdict(lambda: deque(maxlen=20000))
    lines = {}
    latest = [0.0]
    status = ['Waiting for touch_button_sensor records']
    incoming = queue.Queue(maxsize=4096)
    stop = threading.Event()

    def update_line(key, axis, x_values, y_values, style, label,
                    drawstyle='default'):
        if not x_values:
            if key in lines:
                lines[key].set_data([], [])
            return
        if key not in lines:
            lines[key], = axis.plot(
                [], [], style, label=label, marker='.', markersize=2,
                drawstyle=drawstyle,
            )
        lines[key].set_data(x_values, y_values)

    def accept(row):
        channel = row['ch']
        if channel not in channels:
            return
        data[channel].append(row)
        latest[0] = max(latest[0], row['time'])
        for channel_id, rows in list(data.items()):
            data[channel_id] = deque(
                (item for item in rows
                 if item['time'] >= latest[0] - args.window),
                maxlen=20000,
            )

    def draw(_=None):
        while True:
            try:
                kind, value = incoming.get_nowait()
            except queue.Empty:
                break
            if kind == 'row':
                accept(value)
            else:
                status[0] = value

        active_lines = set()
        for index, channel in enumerate(channels):
            rows = sorted(data.get(channel, ()), key=lambda item: item['time'])
            amplitude = [
                row for row in rows if row['kind'] == 'amplitude'
            ]
            events = [row for row in rows if row['kind'] == 'event']

            axis = axes[index, 0]
            update_line(
                (channel, 'amplitude'), axis,
                [row['time'] for row in amplitude],
                [row['amplitude'] for row in amplitude],
                '-', 'amplitude',
            )
            active_lines.add((channel, 'amplitude'))
            axis.set_title(f"CH {channel} — amplitude")
            axis.set_ylabel('Normalized counts')

            axis = axes[index, 1]
            update_line(
                (channel, 'event'), axis,
                [row['time'] for row in events],
                [row['state'] for row in events],
                '-', 'event state', 'steps-post',
            )
            active_lines.add((channel, 'event'))
            axis.set_title(f"CH {channel} — events")
            axis.set_ylabel('inactive / active')
            axis.set_ylim(-0.15, 1.15)

            for axis in axes[index]:
                axis.grid(alpha=.25)
                axis.relim()
                axis.autoscale_view(scalex=False)
                axis.set_xlim(
                    max(0, latest[0] - args.window),
                    max(1, latest[0]),
                )
                if index == len(channels) - 1:
                    axis.set_xlabel('Device time (s)')
                handles, labels = axis.get_legend_handles_labels()
                if handles:
                    axis.legend(fontsize=8, loc='upper left', ncol=2)

        for key, line in lines.items():
            if key[0] in channels and key not in active_lines:
                line.set_data([], [])
        fig.supxlabel(status[0], fontsize=9)

    def emit(kind, value):
        while not stop.is_set():
            try:
                incoming.put((kind, value), timeout=.2)
                return
            except queue.Full:
                pass

    def reader():
        stream = record = None
        try:
            if args.port:
                import serial
                stream = serial.Serial(
                    port=None, baudrate=args.baud, timeout=.2
                )
                stream.port = args.port
                # Keep the native ESP32 USB CDC port from auto-resetting when
                # the plotter opens it.
                stream.dtr = True
                stream.rts = True
                stream.open()
                if args.record:
                    record = open(args.record, 'x', buffering=1)
            else:
                stream = open(args.file, encoding='utf-8', errors='replace')
            emit('status', 'Reading ' + (args.port or args.file))
            while not stop.is_set():
                line = stream.readline()
                if isinstance(line, bytes):
                    line = line.decode('utf-8', errors='replace')
                if not line:
                    if args.port:
                        stop.wait(.05)
                        continue
                    emit('status', 'Replay finished')
                    break
                if record:
                    record.write(line)
                for row in parse_records(line):
                    emit('row', row)
        except Exception as exc:
            emit('status', f"ERROR: {exc}")
            print(f"ERROR: {exc}", flush=True)
        finally:
            if stream:
                stream.close()
            if record:
                record.close()

    if args.png:
        with open(args.file, encoding='utf-8', errors='replace') as stream:
            for line in stream:
                for row in parse_records(line):
                    accept(row)
        if not any(data.get(channel) for channel in channels):
            parser.error('No matching public touch-button records found')
        status[0] = args.file
        draw()
        fig.savefig(args.png, dpi=150)
        return

    worker = threading.Thread(target=reader, daemon=True)
    worker.start()
    animation = FuncAnimation(
        fig, draw, interval=args.refresh_ms, cache_frame_data=False
    )
    try:
        plt.show()
    finally:
        stop.set()
        worker.join(timeout=2)
    return animation


if __name__ == '__main__':
    main()
