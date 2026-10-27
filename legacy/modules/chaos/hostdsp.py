"""Post-Surge DSP that runs on numpy blocks: roughness AM, Oneiroi-style looper, safety limiter.

Everything here is block-based and allocation-light so it can run inside the audio callback.
Blocks are float32 arrays shaped (2, n) — stereo.
"""
import numpy as np

SR = 48000


class RoughAM:
    """Amplitude modulation for the 'scream band' (Fig 2). rate in Hz, depth 0..1."""

    def __init__(self, sr=SR):
        self.sr = sr
        self.phase = 0.0
        self.rate = 70.0
        self.depth = 0.0

    def process(self, x):
        n = x.shape[1]
        if self.depth <= 1e-4:
            self.phase = (self.phase + self.rate * n / self.sr) % 1.0
            return x
        ph = self.phase + self.rate * np.arange(n) / self.sr
        self.phase = float((ph[-1] + self.rate / self.sr) % 1.0)
        # gain swings between (1 - depth) and 1: depth=1 is full on/off chopping
        g = 1.0 - self.depth * (0.5 - 0.5 * np.cos(2 * np.pi * ph))
        return x * g.astype(np.float32)


def softclip(x):
    return np.tanh(x)


class Looper:
    """5 s stereo ring buffer, after Befaco Oneiroi's looper.

    - always writes the incoming signal (scaled by `rec`) plus its own playback scaled by
      `feedback` (sound-on-sound), soft-clipped so it saturates instead of exploding
    - reads at `speed` (-2..+2, interpolated): slower = lower pitch, negative = reverse
    - because the overdub is the *varispeed* playback, every pass shifts pitch again:
      a falling cascade at speed < 1, a sound mass when feedback is high
    """

    def __init__(self, seconds=5.0, sr=SR):
        self.n = int(seconds * sr)
        self.buf = np.zeros((2, self.n), np.float32)
        self.w = 0
        self.r = 0.0
        self.speed = 1.0
        self.feedback = 0.0
        self.mix = 0.0
        self.rec = 1.0
        self.frozen = False

    def clear(self):
        self.buf[:] = 0.0

    def process(self, x):
        if self.mix <= 1e-4 and self.feedback <= 1e-4 and self.rec <= 1e-4:
            self.w = (self.w + x.shape[1]) % self.n
            return x
        m = x.shape[1]
        pos = (self.r + self.speed * np.arange(m)) % self.n
        i0 = pos.astype(np.int64)
        frac = (pos - i0).astype(np.float32)
        i1 = (i0 + 1) % self.n
        out = self.buf[:, i0] * (1 - frac) + self.buf[:, i1] * frac
        self.r = float((self.r + self.speed * m) % self.n)

        if not self.frozen:
            widx = (self.w + np.arange(m)) % self.n
            new = softclip(self.rec * x + self.feedback * out) + 1e-12  # tiny DC kills denormals
            self.buf[:, widx] = new.astype(np.float32)
        self.w = (self.w + m) % self.n
        return x * (1.0 - 0.5 * self.mix) + out * self.mix


class Output:
    """Master gain + DC blocker + hard ceiling at -1 dBFS (hearing safety)."""

    CEIL = 10 ** (-1 / 20)

    def __init__(self):
        self.gain_db = -18.0
        self.prev_x = np.zeros(2, np.float32)
        self.prev_y = np.zeros(2, np.float32)

    def process(self, x):
        g = 10 ** (self.gain_db / 20)
        y = x * g
        # one-pole DC blocker, vectorised with a short python loop only over 2 channels
        r = 0.995
        out = np.empty_like(y)
        for c in range(2):
            xc = y[c]
            dx = np.diff(xc, prepend=self.prev_x[c])
            # y[n] = dx[n] + r*y[n-1]; solve with lfilter-free recursion via cumulative trick
            yc = _one_pole(dx, r, self.prev_y[c])
            out[c] = yc
            self.prev_x[c] = xc[-1]
            self.prev_y[c] = yc[-1]
        # soft knee then hard ceiling
        out = np.tanh(out / self.CEIL) * self.CEIL
        return np.clip(out, -self.CEIL, self.CEIL).astype(np.float32)


try:
    from scipy.signal import lfilter

    def _one_pole(dx, r, y0):
        y, _ = lfilter([1.0], [1.0, -r], dx, zi=[r * y0])
        return y.astype(np.float32)
except ImportError:  # pragma: no cover
    def _one_pole(dx, r, y0):
        y = np.empty_like(dx)
        prev = y0
        for i, v in enumerate(dx):
            prev = v + r * prev
            y[i] = prev
        return y
