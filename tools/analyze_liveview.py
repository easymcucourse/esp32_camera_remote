"""Summarize an isolated LIVE or SETTINGS UART capture without claiming acceptance.

The firmware reports the last frame's read/display/JPEG values every ~5 seconds;
their percentiles describe log samples, not all frames. FPS is weighted by the
device timestamps. Initial reports and intervals crossing resets are excluded.
"""
import argparse
import json
import math
from pathlib import Path
import re
import statistics

ANSI = re.compile(r'\x1b\[[0-9;]*m')
STAMP = re.compile(r'\b[IEWDV] \((\d+)\)')
FRAME = re.compile(r'LIVEVIEW frames=(\d+) fps=([0-9.]+) JPEG=(\d+) read=(\d+)ms display=(\d+)ms')
MEMORY = re.compile(r'\b(free_internal|free_psram|min_internal|min_psram|largest_internal|largest_psram)=(\d+)')
PHASE = re.compile(r'\b(wait|clear|header|decode|stride|overlay|publish|settings)=(\d+)')


def stats(values):
    if not values:
        return None
    ordered = sorted(values)
    # Nearest-rank P95: retain outliers in small hardware captures.
    return {'samples': len(values), 'mean': statistics.fmean(values),
            'p50': statistics.median(values),
            'p95': ordered[math.ceil(.95 * len(ordered)) - 1],
            'min': ordered[0], 'max': ordered[-1]}


def analyze(text):
    sessions, current, previous = [], None, None
    sampled = {key: [] for key in ('read_ms', 'display_ms', 'jpeg_bytes')}
    heaps = {key: [] for key in ('free_internal', 'free_psram', 'min_internal',
                                'min_psram', 'largest_internal', 'largest_psram')}
    phases = {str(mode): {key: [] for key in ('wait', 'clear', 'header', 'decode',
                                             'stride', 'overlay', 'publish')}
              for mode in (0, 1)}
    incidents = {key: 0 for key in ('no_mem', 'panic', 'stream_exit', 'liveview_200f',
                                   'session_verified', 'display_failure')}
    untimed_reports = 0
    observed_settings = {'0': 0, '1': 0}
    for raw in text.splitlines():
        line = ANSI.sub('', raw)
        for key, value in MEMORY.findall(line):
            heaps[key].append(int(value))
        incidents['no_mem'] += bool(re.search(r'\b(?:ESP_ERR_NO_MEM|NO_MEM)\b', line))
        incidents['panic'] += bool(re.search(r'Guru Meditation|assert failed|Task watchdog got triggered|abort\(\) was called|Brownout detector', line))
        stream_exit = 'Live-view ended:' in line or 'Stream exit' in line
        incidents['stream_exit'] += stream_exit
        incidents['liveview_200f'] += bool(re.search(r'\b(?:0x)?200[fF]\b', line))
        incidents['session_verified'] += 'SESSION VERIFIED' in line
        incidents['display_failure'] += bool(re.search(r'display_failed=1|Display restart:|LCD publication failed:|LCD completion timed out;', line))
        if 'JPEG phases us:' in line:
            values = dict(PHASE.findall(line))
            mode = values.get('settings')
            if mode in phases:
                observed_settings[mode] += 1
                for key in phases[mode]:
                    if key in values:
                        phases[mode][key].append(int(values[key]))
        match = FRAME.search(line)
        if not match:
            if stream_exit or 'Camera task finished' in line or 'rst:0x' in line:
                previous = current = None
            continue
        frames, fps, jpeg, read, display = match.groups()
        frames, fps = int(frames), float(fps)
        stamp = STAMP.search(line)
        if not stamp:
            untimed_reports += 1
            previous = current = None
            continue
        when = int(stamp.group(1))
        if previous is None or frames <= previous[0] or when <= previous[1]:
            current = {'first_report_ms': when, 'last_report_ms': when,
                       'intervals': [], 'coverage_seconds': 0.0}
            sessions.append(current)
        else:
            seconds = (when - previous[1]) / 1000
            current['intervals'].append({'seconds': seconds, 'fps': fps,
                                        'frames': frames - previous[0]})
            current['last_report_ms'] = when
            current['coverage_seconds'] += seconds
        previous = (frames, when)
        # The first-frame report is a partial window and an unrepresentative sample.
        if frames > 1:
            for key, value in zip(sampled, (read, display, jpeg)):
                sampled[key].append(int(value))
    intervals = [interval for session in sessions for interval in session['intervals']]
    duration = sum(item['seconds'] for item in intervals)
    return {
        'schema_version': 1,
        'limitations': ['read/display/JPEG percentiles are sparse last-frame log samples',
                        'heap minima cover the entire input capture, including startup if present',
                        'mode and Wi-Fi environment must be recorded by the operator',
                        'no hardware acceptance is inferred from this summary'],
        'sessions': sessions,
        'observed_settings_phase_reports': observed_settings,
        'mixed_settings': all(observed_settings.values()),
        'fps': {'reports': len(intervals), 'covered_seconds': duration,
                'longest_session_seconds': max((s['coverage_seconds'] for s in sessions), default=0),
                'weighted_mean': sum(i['fps'] * i['seconds'] for i in intervals) / duration if duration else None,
                'frame_count_mean': sum(i['frames'] for i in intervals) / duration if duration else None,
                'minimum_reported_window': min((i['fps'] for i in intervals), default=None),
                'maximum_report_gap_seconds': max((i['seconds'] for i in intervals), default=None),
                'untimed_reports': untimed_reports},
        'sampled_frames': {key: stats(values) for key, values in sampled.items()},
        'heap_bytes': {key: stats(values) for key, values in heaps.items()},
        'jpeg_phases_us_by_settings': {mode: {key: stats(values) for key, values in values.items()}
                                      for mode, values in phases.items()},
        'incident_log_lines': incidents,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--expect-settings', type=int, choices=(0, 1),
                        help='Reject a capture with missing or conflicting page phase reports')
    args = parser.parse_args()
    result = analyze(args.log.read_text(encoding='utf-8', errors='replace'))
    output = json.dumps(result, indent=2, ensure_ascii=False)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output + '\n', encoding='utf-8')
    else:
        print(output)
    if args.expect_settings is not None:
        counts = result['observed_settings_phase_reports']
        if not counts[str(args.expect_settings)] or counts[str(1 - args.expect_settings)]:
            parser.exit(1, 'Page phase reports are missing or inconsistent; capture cannot be used for this page.\n')


if __name__ == '__main__':
    main()
