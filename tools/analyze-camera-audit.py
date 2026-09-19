"""Read-only analysis of 560-byte native camera/draw/Present audit records."""
import argparse
import collections
import json
import math
from pathlib import Path
import struct

RECORD = struct.Struct('<4I2Q132f')


def angular(a, b):
    norm = math.sqrt(sum(x*x for x in a) * sum(x*x for x in b))
    if not norm or not math.isfinite(norm):
        return None
    return math.degrees(2 * math.acos(min(1.0, abs(sum(x*y for x, y in zip(a, b))) / norm)))


def residual(w, observed, vp):
    if not all(math.isfinite(x) for x in (*w, *observed, *vp)):
        return math.inf
    return max(abs(sum(w[r*4+k]*vp[k*4+c] for k in range(4))-observed[r*4+c]) /
               max(1.0, abs(observed[r*4+c])) for r in range(4) for c in range(4))


def analyze(path):
    raw = path.read_bytes()
    records = []
    cameras = collections.defaultdict(list)
    threads = collections.defaultdict(collections.Counter)
    for offset in range(0, len(raw)-RECORD.size+1, RECORD.size):
        v = RECORD.unpack_from(raw, offset)
        rec = dict(kind=v[0], frame=v[1], thread=v[2], identity=v[3], tick=v[4],
                   pose=v[5], q=v[6:10], values=v[10:], index=len(records))
        records.append(rec)
        threads[str(v[0])][str(v[2])] += 1
        if v[0] == 1 and v[5] and 0 <= v[4]-v[5] <= 250:
            cameras[v[1]].append(rec)
    following = {}
    next_present = None
    for rec in reversed(records):
        if rec['kind'] == 3:
            next_present = rec
        elif rec['kind'] == 2 and next_present is not None:
            following[rec['index']] = next_present
    stats = collections.Counter()
    lag_frames, lag_ms, viewports = collections.Counter(), collections.Counter(), collections.Counter()
    objects = set()
    max_angle = 0.0
    max_error = 0.0
    for draw in (r for r in records if r['kind'] == 2):
        stats['draws_total'] += 1
        candidates = [c for f in range(max(0, draw['frame']-3), draw['frame']+4) for c in cameras[f]]
        if not candidates:
            stats['outside_tracked_interval'] += 1
            continue
        present = following.get(draw['index'])
        if present is None:
            stats['missing_next_present'] += 1
            continue
        stats['eligible_draws'] += 1
        w, wvp = draw['values'][:16], draw['values'][32:48]
        scored = [(residual(w, wvp, c['values'][65:81]), c) for c in candidates]
        error, camera = min(scored, key=lambda pair: (pair[0], abs(pair[1]['index']-draw['index'])))
        if error >= 1e-4:
            stats['unmatched_draws'] += 1
            continue
        stats['matched_draws'] += 1
        max_error = max(max_error, error)
        objects.add(w)
        viewports[str(tuple(draw['values'][66:68]))] += 1
        lag_frames[str(draw['frame']-camera['frame'])] += 1
        lag_ms[str(present['pose']-camera['pose'])] += 1
        angle = angular(camera['q'], present['q'])
        tick_mismatch = camera['pose'] != present['pose']
        angle_mismatch = angle is None or angle > .01
        stats['pose_tick_mismatch'] += tick_mismatch
        stats['orientation_mismatch_over_0.01deg'] += angle_mismatch
        stats['either_mismatch'] += tick_mismatch or angle_mismatch
        stats['missing_present_pose'] += not present['pose']
        if angle is not None:
            max_angle = max(max_angle, angle)
    return dict(file=str(path), record_size=RECORD.size, records=len(records),
                ignored_trailing_bytes=len(raw)%RECORD.size, counts=dict(stats),
                coverage=stats['matched_draws']/stats['eligible_draws'] if stats['eligible_draws'] else None,
                max_angular_error_degrees=max_angle, max_matrix_residual=max_error,
                draw_minus_camera_frame=dict(lag_frames), present_minus_camera_pose_ms=dict(lag_ms),
                unique_object_matrices=len(objects), viewports=dict(viewports),
                threads_by_kind={k: dict(v) for k, v in threads.items()},
                note='Tracked inferred from nonzero camera pose age <=250ms; audit has no valid bit. Present is next kind3 in file order.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', nargs='+', type=Path)
    args = parser.parse_args()
    for capture in args.captures:
        print(json.dumps(analyze(capture), indent=2))
