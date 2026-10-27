# SPDX-License-Identifier: GPL-3.0-only
"""Bounded, dependency-free endpoint comparisons; no audio APIs are imported."""
import math
import random

RATE = 48000
# Endpoint SRC/volume are allowed; non-linear distortion and lost windows are not.
TOLERANCES = dict(correlation=0.95, relative_error=0.15, gain_min=0.5, gain_max=1.5,
                  silence_rms=1e-5, clip_peak=0.999, dropout_ratio=0.05,
                  discontinuity=0.03, timing_ms=20, response_db=6)


def stimulus(kind, seconds=2, rate=RATE):
    if kind not in ('tone', 'pulses', 'sweep') or not 0.1 <= seconds <= 5:
        raise ValueError('unsupported stimulus or duration')
    phase, samples = 0.0, []
    for i in range(round(seconds * rate)):
        hz = 440 if kind != 'sweep' else 100 * (60 ** (i / (seconds * rate)))
        phase += 2 * math.pi * hz / rate
        gate = kind != 'pulses' or i % (rate // 2) < rate // 4
        samples.append(0.03 * math.sin(phase) if gate else 0.0)
    return samples


def pilot(samples):
    rng = random.Random(731)
    knots = [0.01 * rng.uniform(-1, 1) for _ in range(129)]
    return [knots[i // 8] * (1 - i % 8 / 8) + knots[i // 8 + 1] * (i % 8 / 8)
            for i in range(1024)] + [0.0] * 256 + samples


def rms(x):
    return math.sqrt(sum(v * v for v in x) / max(1, len(x)))


def resample(x, source_rate, target_rate=RATE):
    if not 8000 <= source_rate <= 192000:
        raise ValueError('unsupported sample rate')
    n = int((len(x) - 1) * target_rate / source_rate) + 1
    def sample(i):
        at = i * source_rate / target_rate
        j = min(int(at), len(x) - 1)
        return x[j] + (x[min(j + 1, len(x) - 1)] - x[j]) * (at - j)
    return [sample(i) for i in range(n)] if x else []


def align(reference, capture, rate=RATE):
    # Mandatory aperiodic pilot prevents ambiguous cycle alignment on steady tones.
    width = min(1024, len(reference))
    def score(lag, stride):
        a, b = reference[:width:stride], capture[lag:lag + width:stride]
        return sum(u * v for u, v in zip(a, b)) / math.sqrt(
            max(1e-30, sum(u * u for u in a) * sum(v * v for v in b)))
    limit = min(round(rate * 0.75), len(capture) - width)
    if limit < 0:
        raise ValueError('capture shorter than alignment pilot')
    coarse = max(range(0, limit + 1, 8), key=lambda i: score(i, 8))
    lag = max(range(max(0, coarse - 8), min(limit, coarse + 8) + 1), key=lambda i: score(i, 1))
    return lag, score(lag, 1)


def analyse(reference, capture, capture_rate=RATE, rate=RATE):
    if not reference or len(reference) > rate * 6 or len(capture) > capture_rate * 8:
        raise ValueError('empty reference or duration exceeds analysis bounds')
    if any(not math.isfinite(v) for v in reference + capture):
        raise ValueError('non-finite samples')
    capture = resample(capture, capture_rate, rate)
    lag, confidence = align(reference, capture, rate)
    outside = capture[:lag] + capture[lag + len(reference):]
    x = capture[lag:lag + len(reference)]
    complete = len(x) == len(reference)
    x += [0.0] * (len(reference) - len(x))
    energy = sum(v * v for v in reference)
    gain = sum(a * b for a, b in zip(x, reference)) / max(energy, 1e-30)
    error = [a - gain * b for a, b in zip(x, reference)]
    relative = rms(error) / max(rms(x), 1e-30)
    width = max(1, rate // 100)
    expected = [rms(reference[i:i + width]) for i in range(0, len(x), width)]
    observed = [rms(x[i:i + width]) for i in range(0, len(x), width)]
    dropouts = sum(a > 0.001 and b < a * TOLERANCES['dropout_ratio'] for a, b in zip(expected, observed))
    def edges(values):
        threshold = max(values) * 0.25
        return [i for i in range(1, len(values)) if values[i - 1] <= threshold < values[i]]
    a, b = edges(expected), edges(observed)
    timing = max((abs(i - j) * 10 for i, j in zip(a, b)), default=0)
    response = {}
    # Fixed bins are diagnostic response samples, not a full transfer-function estimate.
    for hz in (100, 440, 1000, 5000):
        def magnitude(values):
            data = values[1280:5376]
            return abs(sum(v * complex(math.cos(2 * math.pi * hz * i / rate),
                                       -math.sin(2 * math.pi * hz * i / rate)) for i, v in enumerate(data))) / max(1, len(data))
        ref, got = magnitude(reference), magnitude(x)
        if ref > 1e-5:
            response[str(hz)] = 20 * math.log10(max(1e-12, got / max(abs(gain) * ref, 1e-30)))
    metrics = dict(rms=rms(x), peak=max(map(abs, capture), default=0), dc=sum(x) / len(x),
                   outside_rms=rms(outside), gain=gain, relative_error=relative, correlation=confidence,
                   latency_ms=lag * 1000 / rate, dropout_windows=dropouts,
                   discontinuities=sum(abs(error[i] - error[i - 1]) > 0.03 for i in range(1, len(error))),
                   envelope_clock_edge_count=len(b), expected_edge_count=len(a), timing_error_ms=timing,
                   response_db=response, complete=complete, resampled=capture_rate != rate)
    failures = []
    tests = {'unexpected_audio': rms(outside) < 0.001, 'silence': metrics['rms'] >= 1e-5, 'clipping': metrics['peak'] < 0.999,
             'mismatch': confidence >= 0.95 and relative <= 0.15 and 0.5 <= gain <= 1.5,
             'dropout': dropouts == 0 and complete, 'discontinuity': metrics['discontinuities'] == 0,
             'timing': len(a) == len(b) and timing <= 20,
             'dc': abs(metrics['dc']) < 0.005,
             'response': all(abs(v) <= 6 for v in response.values())}
    failures = [name for name, passed in tests.items() if not passed]
    return dict(status='pass' if not failures else 'signal_mismatch', failures=failures,
                metrics=metrics, tolerances=TOLERANCES)
