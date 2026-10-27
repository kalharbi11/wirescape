"""Analysis used by the UI and the offline figures. Pure numpy; never called from the audio thread."""
import numpy as np

SR = 48000


def scope(x, f0, cycles=3, n_points=600):
    """Mono waveform, triggered at a rising zero crossing, ~`cycles` periods long."""
    span = int(SR * cycles / max(f0, 20.0))
    span = min(span, len(x) // 2)
    seg = x[-(span * 2):]
    zc = np.where((seg[:-1] <= 0) & (seg[1:] > 0))[0]
    start = zc[0] if len(zc) and zc[0] + span < len(seg) else 0
    w = seg[start:start + span]
    if len(w) < 2:
        return np.zeros(n_points), np.zeros(n_points)
    idx = np.linspace(0, len(w) - 1, n_points)
    return idx / SR * 1000.0, np.interp(idx, np.arange(len(w)), w)  # ms, amplitude


_HANN = {}


def _win(n):
    if n not in _HANN:
        _HANN[n] = np.hanning(n).astype(np.float32)
    return _HANN[n]


def spectrum(x, n=4096, bins=256, fmin=20.0, fmax=20000.0):
    """Log-frequency magnitude spectrum in dB, resampled to `bins` points."""
    seg = x[-n:]
    if len(seg) < n:
        seg = np.pad(seg, (n - len(seg), 0))
    mag = np.abs(np.fft.rfft(seg * _win(n))) / (n / 4)
    freqs = np.fft.rfftfreq(n, 1 / SR)
    fl = np.geomspace(fmin, fmax, bins)
    db = 20 * np.log10(np.interp(fl, freqs, mag) + 1e-9)
    return fl, np.clip(db, -100, 0)


def spectrogram_column(x, n=2048, rows=160, fmax=8000.0):
    """One column for the scrolling spectrogram: linear Hz to fmax (matches the guide's Fig 1)."""
    seg = x[-n:]
    if len(seg) < n:
        seg = np.pad(seg, (n - len(seg), 0))
    mag = np.abs(np.fft.rfft(seg * _win(n))) / (n / 4)
    freqs = np.fft.rfftfreq(n, 1 / SR)
    fl = np.linspace(0, fmax, rows)
    db = 20 * np.log10(np.interp(fl, freqs, mag) + 1e-9)
    return np.clip((db + 90) / 90, 0, 1)  # 0..1, low rows = low Hz


def roughness(x, fmin=1.0, fmax=300.0, bins=120):
    """Modulation spectrum of the amplitude envelope (the guide's Fig 2B, measured).

    Returns (rates_hz, normalised_energy, scream_band_fraction).
    The scream band is 30-150 Hz (Arnal et al. 2015).
    """
    seg = x[-SR:]  # ~1 s
    if len(seg) < 2048:
        return np.geomspace(fmin, fmax, bins), np.zeros(bins), 0.0
    env = np.abs(seg)
    # crude 300 Hz lowpass by moving average, then decimate to 2 kHz
    k = SR // 600
    env = np.convolve(env, np.ones(k) / k, mode="same")[:: SR // 2000]
    env = env - env.mean()
    m = np.abs(np.fft.rfft(env * np.hanning(len(env))))
    rates = np.fft.rfftfreq(len(env), 1 / 2000)
    sel = (rates >= fmin) & (rates <= fmax)
    total = m[sel].sum() + 1e-12
    band = m[(rates >= 30) & (rates <= 150)].sum()
    rl = np.geomspace(fmin, fmax, bins)
    e = np.interp(rl, rates, m)
    e = e / (e.max() + 1e-12)
    return rl, e, float(band / total)


def beat_rate(f0, cents):
    """Beat frequency between two voices `cents` apart at f0 (the guide's Fig 4B)."""
    return f0 * (2 ** (cents / 1200.0) - 1)


def beat_zone(hz):
    if hz < 8:
        return "chorus"
    if hz < 20:
        return "pulse"
    return "rough"


def midi_to_hz(n):
    return 440.0 * 2 ** ((n - 69) / 12)


def levels(x):
    rms = float(np.sqrt(np.mean(x ** 2)) + 1e-12)
    peak = float(np.max(np.abs(x)) + 1e-12)
    return 20 * np.log10(rms), 20 * np.log10(peak)


_VOWELS = {"i": (270, 2290), "e": (530, 1840), "a": (730, 1090), "o": (570, 840), "u": (300, 870)}


def formants(x, n=4096, fmin=50.0, fmax=12000.0, bins=256, lifter=48):
    """Cepstrally smoothed spectral envelope + crude F1/F2 peak picks (for the Lab readout)."""
    seg = x[-n:]
    if len(seg) < n or np.max(np.abs(seg)) < 1e-5:
        fl = np.geomspace(fmin, fmax, bins)
        return fl, np.full(bins, -100.0), 0.0, 0.0
    spec = np.abs(np.fft.rfft(seg * _win(n))) / (n / 4) + 1e-4  # -80 dB floor
    logm = np.log(spec)
    # bridge the valleys between harmonics (moving max over ~120 Hz) before cepstral smoothing
    from scipy.ndimage import maximum_filter1d
    logm = maximum_filter1d(logm, size=max(3, int(120 / (SR / n))))
    cep = np.fft.irfft(logm)
    cep[lifter:-lifter] = 0
    env = np.fft.rfft(cep).real
    freqs = np.fft.rfftfreq(n, 1 / SR)
    fl = np.geomspace(fmin, fmax, bins)
    edb = np.clip(20 / np.log(10) * np.interp(fl, freqs, env), -100, 0)
    from scipy.signal import find_peaks
    pk, props = find_peaks(env, prominence=0.15)
    def peak(lo, hi):
        sel = [(props["prominences"][i], freqs[k]) for i, k in enumerate(pk) if lo <= freqs[k] <= hi]
        return float(max(sel)[1]) if sel else 0.0
    f1 = peak(200, 1000)
    f2 = peak(max(f1 + 250, 750), 3000) if f1 else 0.0
    return fl, edb, f1, f2


def vowel_guess(f1, f2):
    if f1 <= 0:
        return "-"
    best = min(_VOWELS, key=lambda v: ((f1 - _VOWELS[v][0]) / 300) ** 2 + ((f2 - _VOWELS[v][1]) / 800) ** 2)
    return f"nearest vowel /{best}/"
