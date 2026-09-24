# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy", "opencv-python", "pupil-apriltags"]
# ///
"""Show the USB camera feed with AprilTag overlays; optionally average tag
centres over several frames and write them to JSON for coordinate_calib.py.

    tools/apriltag_capture.py                                   # just look
    tools/apriltag_capture.py --size 640x480 --calib-out calib_pixels.json

Capture at 640x480 (what the firmware streams). Keys: q/Esc quit, s saves a frame.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
from collections import defaultdict
from pathlib import Path

try:
    import cv2
    import numpy as np
    from pupil_apriltags import Detector
except ModuleNotFoundError as missing:
    sys.exit(
        f'{missing.name} is missing from {sys.executable}.\n'
        f'  uv run tools/apriltag_capture.py'
    )

COLOUR_EDGE = (0, 255, 0)
COLOUR_FIRST_CORNER = (0, 128, 255)
COLOUR_CENTRE = (0, 255, 255)
COLOUR_TEXT = (255, 255, 255)
COLOUR_CHIP = (0, 0, 0)
CHIP_ALPHA = 0.65

CAPTURE_DIR = Path(__file__).resolve().parent / 'captures'


def parse_size(text: str) -> tuple[int, int]:
    try:
        w, h = text.lower().split('x')
        return int(w), int(h)
    except ValueError:
        raise argparse.ArgumentTypeError(f'expected WIDTHxHEIGHT, got {text!r}')


def open_camera(device: str, size: tuple[int, int], fps: int) -> cv2.VideoCapture:
    target: str | int = device
    if match := re.fullmatch(r'/dev/video(\d+)', device):
        target = int(match.group(1))
    elif device.isdigit():
        target = int(device)

    camera = cv2.VideoCapture(target, cv2.CAP_V4L2)
    if not camera.isOpened():
        raise SystemExit(
            f'cannot open {device}. Check lsusb | grep -i hbv, '
            f'or fuser -v {device} for programs holding the node.'
        )

    # Set fourcc before frame size; V4L2 picks format first and only offers that format's sizes.
    camera.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter.fourcc(*'MJPG'))
    camera.set(cv2.CAP_PROP_FRAME_WIDTH, size[0])
    camera.set(cv2.CAP_PROP_FRAME_HEIGHT, size[1])
    camera.set(cv2.CAP_PROP_FPS, fps)

    actual = (int(camera.get(cv2.CAP_PROP_FRAME_WIDTH)),
              int(camera.get(cv2.CAP_PROP_FRAME_HEIGHT)))
    if actual != size:
        print(f'note: asked for {size[0]}x{size[1]}, got {actual[0]}x{actual[1]}')
    return camera


def draw_label(frame: np.ndarray, text: str, at: tuple[int, int], scale: float,
               colour: tuple[int, int, int] = COLOUR_TEXT, align: str = 'left') -> None:
    font = cv2.FONT_HERSHEY_SIMPLEX
    thickness = max(1, round(scale * 2.4))
    (width, height), baseline = cv2.getTextSize(text, font, scale, thickness)
    pad = max(2, round(scale * 5))

    x, y = at
    if align == 'right':
        x -= width
    at = (x, y)
    left, top = max(x - pad, 0), max(y - height - pad, 0)
    right = min(x + width + pad, frame.shape[1])
    bottom = min(y + baseline + pad, frame.shape[0])
    if right > left and bottom > top:
        chip = frame[top:bottom, left:right]
        cv2.addWeighted(chip, 1.0 - CHIP_ALPHA,
                        np.full_like(chip, COLOUR_CHIP, dtype=np.uint8),
                        CHIP_ALPHA, 0.0, dst=chip)

    cv2.putText(frame, text, at, font, scale, COLOUR_CHIP, thickness + 2, cv2.LINE_AA)
    cv2.putText(frame, text, at, font, scale, colour, thickness, cv2.LINE_AA)


def annotate(frame: np.ndarray, detections: list, coords: str = 'centre') -> None:
    ui = frame.shape[0] / 720.0

    for tag in detections:
        points = np.asarray(tag.corners, dtype=np.float64)
        cx, cy = float(tag.center[0]), float(tag.center[1])
        centre = (int(round(cx)), int(round(cy)))

        cv2.polylines(frame, [points.astype(np.int32)], True, COLOUR_EDGE,
                      max(2, round(2 * ui)), cv2.LINE_AA)
        cv2.circle(frame, tuple(points[0].astype(int)),
                   max(4, round(5 * ui)), COLOUR_FIRST_CORNER, -1)
        cv2.drawMarker(frame, centre, COLOUR_CENTRE, cv2.MARKER_CROSS,
                       round(14 * ui), max(2, round(2 * ui)))

        if coords == 'corners':
            for index, point in enumerate(points):
                direction = point - (cx, cy)
                span = np.hypot(*direction) or 1.0
                anchor = point + direction / span * (14 * ui)
                draw_label(frame, f'{index}:({point[0]:.1f},{point[1]:.1f})',
                           (int(anchor[0]), int(anchor[1])), 0.42 * ui,
                           align='right' if direction[0] < 0 else 'left')
        else:
            draw_label(frame, f'({cx:.1f}, {cy:.1f})',
                       (centre[0] + round(12 * ui), centre[1] + round(20 * ui)),
                       0.52 * ui)

    for tag in detections:
        points = np.asarray(tag.corners, dtype=np.float64)
        centre = (int(round(tag.center[0])), int(round(tag.center[1])))
        if coords == 'corners':
            at = (centre[0], int(points[:, 1].min() - 38 * ui))
        else:
            at = (centre[0] + round(12 * ui), centre[1] - round(6 * ui))
        draw_label(frame, f'id {tag.tag_id}', at, 0.62 * ui)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--device', default='/dev/video0', help='V4L2 node or index')
    parser.add_argument('--size', type=parse_size, default=(640, 480),
                        help='capture size (default 640x480)')
    parser.add_argument('--fps', type=int, default=30)
    parser.add_argument('--decimate', type=float, default=1.0,
                        help='quad_decimate for speed vs small-tag detection')
    parser.add_argument('--coords', choices=('centre', 'center', 'corners'),
                        default='centre', help='label centres or corners')
    parser.add_argument('--print', dest='print_tags', action='store_true',
                        help='print detections to stdout')
    parser.add_argument('--headless', action='store_true',
                        help='no window, implies --print')
    parser.add_argument('--calib-out', type=Path, default=None, metavar='PATH',
                        help='average centres and write JSON, then exit')
    parser.add_argument('--calib-frames', type=int, default=30, metavar='N',
                        help='frames to average (default 30)')
    parser.add_argument('--min-margin', type=float, default=20.0,
                        help='ignore weak detections when sampling')
    args = parser.parse_args(argv)
    if args.headless:
        args.print_tags = True
    if args.coords == 'center':
        args.coords = 'centre'

    detector = Detector(families='tag36h11', nthreads=4,
                        quad_decimate=args.decimate, refine_edges=True,
                        decode_sharpening=0.25)

    camera = open_camera(args.device, args.size, args.fps)
    window = 'apriltag 36h11'
    if not args.headless:
        cv2.namedWindow(window, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(window, *args.size)

    captured_size = (int(camera.get(cv2.CAP_PROP_FRAME_WIDTH)),
                     int(camera.get(cv2.CAP_PROP_FRAME_HEIGHT)))
    samples: dict[int, list[tuple[float, float]]] = defaultdict(list)
    sampled_frames = 0
    frames = 0
    started = time.monotonic()
    measured_fps = 0.0

    try:
        while True:
            ok, frame = camera.read()
            if not ok:
                print('frame read failed', file=sys.stderr)
                return 1

            grey = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            detections = detector.detect(grey)

            if args.calib_out is not None:
                for tag in detections:
                    if tag.decision_margin >= args.min_margin:
                        samples[tag.tag_id].append(
                            (float(tag.center[0]), float(tag.center[1])))
                sampled_frames += 1
                if sampled_frames >= args.calib_frames:
                    break

            if args.print_tags and detections:
                stamp = f'[{time.monotonic() - started:8.2f}]'
                for tag in sorted(detections, key=lambda t: t.tag_id):
                    print(f'{stamp} id {tag.tag_id:3d}  '
                          f'({tag.center[0]:7.1f}, {tag.center[1]:7.1f})  '
                          f'margin {tag.decision_margin:5.1f}')

            frames += 1
            elapsed = time.monotonic() - started
            if elapsed >= 1.0:
                measured_fps = frames / elapsed
                frames, started = 0, time.monotonic()

            if args.headless:
                continue

            annotate(frame, detections, args.coords)
            hud = frame.shape[0] / 720.0
            status = f'{len(detections)} tags   {measured_fps:4.1f} fps'
            if args.calib_out is not None:
                status += f'   sampling {sampled_frames}/{args.calib_frames}'
            draw_label(frame, status, (round(14 * hud), round(34 * hud)), 0.72 * hud)
            cv2.imshow(window, frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord('q'), 27):
                break
            if key == ord('s'):
                CAPTURE_DIR.mkdir(exist_ok=True)
                path = CAPTURE_DIR / f"apriltag_{time.strftime('%Y%m%d_%H%M%S')}.png"
                cv2.imwrite(str(path), frame)
                print(f'wrote {path}')
    except KeyboardInterrupt:
        pass
    finally:
        camera.release()
        cv2.destroyAllWindows()

    if args.calib_out is not None:
        if not samples:
            print('no tags sampled', file=sys.stderr)
            return 1
        centres = {
            tag_id: [
                round(sum(u for u, _ in seen) / len(seen), 2),
                round(sum(v for _, v in seen) / len(seen), 2),
            ]
            for tag_id, seen in sorted(samples.items())
        }
        args.calib_out.write_text(json.dumps({
            'size': list(captured_size),
            'frames': sampled_frames,
            'min_margin': args.min_margin,
            'sightings': {str(k): len(v) for k, v in sorted(samples.items())},
            'centres': {str(k): v for k, v in centres.items()},
        }, indent=2) + '\n')
        print(f'wrote {args.calib_out}  ({len(centres)} tags, '
              f'{captured_size[0]}x{captured_size[1]}, {sampled_frames} frames)')
        for tag_id, (u, v) in centres.items():
            print(f'  id {tag_id:3d}  ({u:7.2f}, {v:7.2f})  '
                  f'{len(samples[tag_id]):3d}/{sampled_frames} frames')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
