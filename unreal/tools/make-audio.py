"""Writes every Grudgebound sound as a WAV, synthesised here from sine waves and filtered noise.

Nothing is sampled or downloaded, so there is no third-party licence to carry. Needs Python 3 with numpy:
  python unreal/tools/make-audio.py
Then import them with the editor closed (see import-audio.py).
"""
from pathlib import Path
import wave
import numpy as np

SR = 44100
OUT = Path(__file__).resolve().parent.parent / "GrudgeBound" / "Content" / "Grudgebound" / "Audio"
rng = np.random.default_rng(1147)


def t(seconds, sr=SR):
    return np.arange(int(seconds * sr)) / sr


def noise(n):
    return rng.standard_normal(n)


def band(x, lo, hi, sr=SR):
    """Brick-wall band filter with soft edges. Circular, so a filtered loop still loops."""
    spectrum = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / sr)
    edge_lo = max(lo * 0.25, 1.0)
    gain = np.clip((f - lo) / edge_lo + 1, 0, 1) * np.clip((hi - f) / (hi * 0.25) + 1, 0, 1)
    return np.fft.irfft(spectrum * gain, len(x))


def fade(x, attack=0.002, release=0.01, sr=SR):
    a, r = max(1, int(attack * sr)), max(1, int(release * sr))
    x = x.copy()
    x[:a] *= np.linspace(0, 1, a)
    x[-r:] *= np.linspace(1, 0, r)
    return x


def norm(x, peak=0.7):
    return x * (peak / max(1e-9, np.max(np.abs(x))))


def sweep(f0, f1, seconds, sr=SR):
    """Sine gliding from f0 to f1."""
    n = int(seconds * sr)
    freq = np.geomspace(f0, f1, n)
    return np.sin(2 * np.pi * np.cumsum(freq) / sr)


def hump(x):
    """Zero at both ends, one in the middle."""
    return np.clip(np.sin(np.pi * x / x[-1]), 0, None)


def bell(freq, seconds, decay=5.0, sr=SR):
    """A small struck bell: a few inharmonic partials, the high ones dying first."""
    x = t(seconds, sr)
    out = np.zeros_like(x)
    for ratio, amp, d in ((1, 1, 1), (2.01, 0.5, 1.6), (2.76, 0.32, 2.3), (4.07, 0.18, 3.2), (5.4, 0.08, 4.5)):
        out += amp * np.sin(2 * np.pi * freq * ratio * x) * np.exp(-x * decay * d)
    return out


def pluck(freq, seconds, sr=SR):
    """A plucked string: harmonics that fade faster the higher they are."""
    x = t(seconds, sr)
    out = np.zeros_like(x)
    for k in range(1, 11):
        if freq * k > sr * 0.45:
            break
        out += (1 / k) * np.sin(2 * np.pi * freq * k * x * (1 + 0.0004 * k * k)) * np.exp(-x * (1.5 + 0.9 * k))
    out[: int(0.004 * sr)] *= np.linspace(0, 1, int(0.004 * sr))
    return out


def place(buffer, sound, at, gain=1.0, sr=SR, wrap=False):
    start = int(at * sr)
    if wrap:
        idx = (start + np.arange(len(sound))) % len(buffer)
        np.add.at(buffer, idx, sound * gain)
    else:
        end = min(len(buffer), start + len(sound))
        buffer[start:end] += sound[: end - start] * gain


def save(name, x, sr=SR):
    x = np.asarray(x)
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    OUT.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT / (name + ".wav")), "wb") as f:
        f.setnchannels(1 if pcm.ndim == 1 else pcm.shape[1])
        f.setsampwidth(2)
        f.setframerate(sr)
        f.writeframes(pcm.tobytes())
    print(f"{name}.wav  {len(pcm) / sr:.2f}s")


# ---------------------------------------------------------------- footsteps
def step_stone(seed):
    x = t(0.11)
    scuff = band(noise(len(x)), 350 + 60 * seed, 2600) * np.exp(-x * 48)
    heel = np.sin(2 * np.pi * (105 + 9 * seed) * x) * np.exp(-x * 60)
    return norm(fade(scuff * 0.8 + heel * 0.7), 0.6)


def step_wood(seed):
    x = t(0.14)
    knock = np.sin(2 * np.pi * (170 + 22 * seed) * x) * np.exp(-x * 42)
    board = np.sin(2 * np.pi * (410 + 35 * seed) * x) * np.exp(-x * 75) * 0.35
    scuff = band(noise(len(x)), 250, 1400) * np.exp(-x * 55) * 0.4
    return norm(fade(knock + board + scuff), 0.6)


def step_grass(seed):
    x = t(0.24)
    swish = band(noise(len(x)), 2200 + 300 * seed, 9000) * hump(x) ** 1.6
    return norm(fade(swish, 0.01, 0.04), 0.45)


# ---------------------------------------------------------------- panels and buttons
def ui_click():
    x = t(0.045)
    return norm(fade(sweep(1700, 850, 0.045) * np.exp(-x * 90) + band(noise(len(x)), 3000, 9000) * np.exp(-x * 300) * 0.3, 0.0005, 0.01), 0.5)


def ui_paper(seconds, rising):
    x = t(seconds)
    shape = hump(x) ** (0.7 if rising else 1.8)
    tilt = np.linspace(0.6, 1.0, len(x)) if rising else np.linspace(1.0, 0.5, len(x))
    return norm(fade(band(noise(len(x)), 1400, 7000) * shape * tilt, 0.01, 0.03), 0.4)


def door():
    out = np.zeros(int(0.42 * SR))
    place(out, step_wood(1), 0.0, 1.0)
    place(out, step_wood(4), 0.13, 0.55)
    x = t(0.25)
    creak = np.sin(2 * np.pi * np.cumsum(np.linspace(310, 420, len(x)) * (1 + 0.03 * np.sin(2 * np.pi * 31 * x))) / SR)
    place(out, fade(creak * hump(x) * 0.12, 0.02, 0.05), 0.16)
    return norm(out, 0.6)


def talk():
    out = np.zeros(int(0.2 * SR))
    for at, freq in ((0.0, 523.25), (0.075, 659.25)):
        x = t(0.09)
        place(out, fade(np.sin(2 * np.pi * freq * x) * np.exp(-x * 30), 0.004, 0.02), at)
    return norm(out, 0.45)


# ---------------------------------------------------------------- battle
def whoosh():
    x = t(0.26)
    low = band(noise(len(x)), 300, 1200)
    high = band(noise(len(x)), 1200, 5000)
    mix = np.linspace(0, 1, len(x))
    return norm(fade((low * (1 - mix) + high * mix) * hump(x) ** 2, 0.02, 0.04), 0.5)


def hit(seed):
    x = t(0.22)
    crack = band(noise(len(x)), 500, 3200 + 500 * seed) * np.exp(-x * 34)
    thump = sweep(190 - 20 * seed, 58, 0.22) * np.exp(-x * 17)
    return norm(np.tanh((crack * 0.9 + thump * 1.2) * 1.6), 0.75)


def ward():
    x = t(0.6)
    out = sum(np.sin(2 * np.pi * f * x) * a for f, a in ((880, 1), (1320, 0.6), (1760, 0.35), (2640, 0.15)))
    out *= (0.75 + 0.25 * np.sin(2 * np.pi * 13 * x)) * np.minimum(x / 0.06, 1) * np.exp(-x * 4.5)
    return norm(fade(out, 0.01, 0.08), 0.5)


def heal():
    out = np.zeros(int(0.7 * SR))
    place(out, bell(659.25, 0.5, 7), 0.0)
    place(out, bell(880.0, 0.55, 6), 0.13)
    return norm(fade(out, 0.002, 0.05), 0.5)


def seal_stamp():
    out = np.zeros(int(1.2 * SR))
    x = t(0.3)
    thud = np.sin(2 * np.pi * 72 * x) * np.exp(-x * 20) + band(noise(len(x)), 80, 900) * np.exp(-x * 40) * 0.7
    place(out, np.tanh(thud * 1.5), 0.0, 1.0)
    place(out, bell(587.33, 1.1, 3.2), 0.04, 0.5)
    place(out, bell(880.0, 1.0, 4.0), 0.05, 0.25)
    return norm(fade(out, 0.001, 0.1), 0.75)


def seal_break():
    out = np.zeros(int(0.55 * SR))
    x = t(0.09)
    place(out, band(noise(len(x)), 2500, 9000) * np.exp(-x * 70), 0.0, 1.0)
    x = t(0.5)
    fall = (sweep(520, 300, 0.5) + sweep(552, 311, 0.5)) * np.exp(-x * 7) * 0.4
    place(out, fall, 0.03)
    return norm(fade(out, 0.001, 0.06), 0.6)


def bound():
    out = np.zeros(int(1.6 * SR))
    for i, freq in enumerate((587.33, 739.99, 880.0, 1174.66)):
        place(out, bell(freq, 1.1, 3.5), 0.11 * i, 0.9 if i < 3 else 1.1)
    return norm(fade(out, 0.002, 0.15), 0.6)


def exorcised():
    x = t(1.1)
    air = band(noise(len(x)), 900, 6000) * np.linspace(1, 0, len(x)) ** 2 * np.minimum(x / 0.05, 1)
    out = air * 0.5
    for freq in (440.0, 554.37, 659.25):
        out += np.sin(2 * np.pi * freq * x) * np.exp(-x * 3.2) * 0.25
    return norm(fade(out, 0.01, 0.2), 0.5)


def lost():
    out = np.zeros(int(1.3 * SR))
    for at, freq in ((0.0, 110.0), (0.4, 87.31)):
        x = t(0.85)
        tone = (np.sin(2 * np.pi * freq * x) + 0.4 * np.sin(2 * np.pi * freq * 2 * x) + 0.2 * np.sin(2 * np.pi * freq * 3 * x)) * np.exp(-x * 3.5)
        place(out, fade(tone, 0.02, 0.1), at)
    return norm(out, 0.6)


def encounter():
    out = np.zeros(int(0.9 * SR))
    x = t(0.4)
    place(out, np.tanh((np.sin(2 * np.pi * 62 * x) * np.exp(-x * 11) + band(noise(len(x)), 60, 500) * np.exp(-x * 30) * 0.6) * 1.4), 0.0)
    place(out, bell(311.13, 0.8, 4.0), 0.06, 0.35)
    place(out, bell(440.0, 0.75, 4.0), 0.16, 0.35)
    return norm(fade(out, 0.001, 0.1), 0.7)


# ---------------------------------------------------------------- the night market, a loop
def market_loop():
    sr, seconds = 22050, 32.0
    n = int(sr * seconds)
    left, right = np.zeros(n), np.zeros(n)
    local = np.random.default_rng(88)

    def slow(rate_hz):
        """A smooth random curve between 0 and 1 that also loops."""
        curve = band(local.standard_normal(n), 0.0, rate_hz, sr)
        curve -= curve.min()
        return curve / max(1e-9, curve.max())

    # People a street away: bands of voice-range noise that swell and fall on their own.
    for lo, hi, pan in ((180, 420, 0.3), (300, 700, 0.7), (450, 950, 0.45), (240, 560, 0.6)):
        voice = band(local.standard_normal(n), lo, hi, sr) * (0.25 + 0.75 * slow(0.35)) * (0.6 + 0.4 * slow(2.5))
        voice = voice / np.max(np.abs(voice)) * 0.11
        left += voice * (1 - pan)
        right += voice * pan
    # Night air over the canal.
    air = band(local.standard_normal(n), 2500, 8000, sr) * (0.5 + 0.5 * slow(0.2))
    air = air / np.max(np.abs(air)) * 0.018
    left += air
    right += np.roll(air, sr // 7)

    # A slow tune on a plucked string: D minor pentatonic, one bar every four seconds.
    D3, F3, G3, A3, C4, D4, F4, G4, A4, C5, D5 = 146.83, 174.61, 196.0, 220.0, 261.63, 293.66, 349.23, 392.0, 440.0, 523.25, 587.33
    tune = [
        (0.0, D4), (1.0, F4), (1.5, G4), (2.0, A4), (3.0, G4),
        (4.0, F4), (5.0, D4), (6.0, C4), (7.0, D4),
        (8.0, A4), (9.0, C5), (9.5, A4), (10.0, G4), (11.0, F4),
        (12.0, G4), (13.0, A4), (14.0, D4),
        (16.0, D5), (17.0, C5), (17.5, A4), (18.0, G4), (19.0, A4),
        (20.0, F4), (21.0, G4), (22.0, D4), (23.0, C4),
        (24.0, F4), (25.0, G4), (25.5, A4), (26.0, C5), (27.0, A4),
        (28.0, G4), (29.0, F4), (30.0, D4),
    ]
    for at, freq in tune:
        note = pluck(freq, 2.6, sr)
        place(left, note, at, 0.085, sr, wrap=True)
        place(right, note, at + 0.012, 0.07, sr, wrap=True)
    for bar, freq in enumerate((D3, D3, F3, G3, D3, A3, F3, C4)):
        note = pluck(freq, 3.6, sr)
        place(left, note, bar * 4.0, 0.075, sr, wrap=True)
        place(right, note, bar * 4.0, 0.085, sr, wrap=True)

    # Wind chimes on the lantern strings, now and then.
    for _ in range(11):
        at = local.uniform(0, seconds)
        freq = local.choice([D5, 698.46, 783.99, 880.0, 1046.5, 1174.66])
        pan = local.uniform(0.15, 0.85)
        chime = bell(freq, 2.2, 2.4, sr) * local.uniform(0.02, 0.045)
        place(left, chime, at, 1 - pan, sr, wrap=True)
        place(right, chime, at, pan, sr, wrap=True)

    stereo = np.stack([left, right], axis=1)
    return stereo * (0.6 / np.max(np.abs(stereo))), sr


if __name__ == "__main__":
    for i in range(3):
        save(f"step_stone_{i + 1}", step_stone(i))
        save(f"step_wood_{i + 1}", step_wood(i))
    for i in range(2):
        save(f"step_grass_{i + 1}", step_grass(i))
        save(f"hit_{i + 1}", hit(i))
    save("ui_click", ui_click())
    save("ui_open", ui_paper(0.2, True))
    save("ui_close", ui_paper(0.15, False))
    save("door", door())
    save("talk", talk())
    save("whoosh", whoosh())
    save("ward", ward())
    save("heal", heal())
    save("seal_stamp", seal_stamp())
    save("seal_break", seal_break())
    save("bound", bound())
    save("exorcised", exorcised())
    save("lost", lost())
    save("encounter", encounter())
    loop, loop_sr = market_loop()
    save("market_loop", loop, loop_sr)
