"""Generate original, deterministic little arcade noises using only the standard library."""

import math
from pathlib import Path
import random
import struct
import sys
import wave


def write_sound(directory: Path, name: str, seconds: float, frequencies: list[float],
                noise: float = 0.0, slide: float = 0.0) -> None:
    rate = 22050
    rng = random.Random(73)
    samples = []
    phase = 0.0
    for i in range(int(rate * seconds)):
        t = i / rate
        progress = t / seconds
        frequency = frequencies[min(len(frequencies) - 1, int(progress * len(frequencies)))]
        phase += (frequency + slide * progress) / rate
        tone = math.sin(phase * math.tau)
        tone = 0.65 * tone + 0.20 * (1 if tone >= 0 else -1)
        envelope = min(1.0, t / 0.007) * (1 - progress) ** 1.6
        sample = (tone * (1 - noise) + rng.uniform(-1, 1) * noise) * envelope * 0.65
        samples.append(struct.pack("<h", int(max(-1, min(1, sample)) * 32767)))
    with wave.open(str(directory / f"{name}.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(b"".join(samples))


def write_samples(directory: Path, name: str, rate: int, values: list[float]) -> None:
    with wave.open(str(directory / f"{name}.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(rate)
        output.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, v)) * 32767)) for v in values))


def write_fizz(directory: Path, seconds: float = 2.6) -> None:
    """A smoke bomb going off: a little pop, then a fluttering, crackling hiss like a flare."""
    rate = 22050
    rng = random.Random(29)
    values = []
    previous_noise = 0.0
    crackle = 0.0
    for i in range(int(rate * seconds)):
        t = i / rate
        progress = t / seconds
        noise = rng.uniform(-1, 1)
        hiss = noise - previous_noise  # crude high-pass: keeps it hissy, not rumbly
        previous_noise = noise
        flutter = 0.75 + 0.25 * math.sin(t * math.tau * 13) * math.sin(t * math.tau * 5.3 + 1.1)
        if rng.random() < 0.0016:
            crackle = rng.uniform(0.5, 1.0) * (1 if rng.random() < 0.5 else -1)
        crackle *= 0.985
        pop = math.sin(t * math.tau * (140 - 300 * t)) * max(0.0, 1 - t / 0.06)
        fade = min(1.0, t / 0.03) * min(1.0, (1 - progress) / 0.4)
        values.append((0.22 * hiss * flutter + 0.35 * crackle) * fade + 0.6 * pop)
    write_samples(directory, "fizz", rate, values)

def write_intro_music(directory: Path) -> None:
    """Original '80s retro rock: crunchy power chords, guitar hooks and big drums."""
    rate = 22050
    bpm = 128
    beat = 60.0 / bpm
    bars = 8
    seconds = bars * 4 * beat
    mix = [0.0] * round(seconds * rate)
    rng = random.Random(1985)
    roots = (45, 41, 48, 43, 45, 41, 48, 43)
    melody = [
        ((0, 76, 1), (1.5, 79, 0.5), (2, 81, 1), (3, 79, 0.5), (3.5, 76, 0.5)),
        ((0, 77, 1.5), (2, 76, 0.5), (2.5, 72, 0.5), (3, 69, 1)),
        ((0, 72, 0.5), (0.5, 76, 0.5), (1, 79, 1), (2, 84, 1.5)),
        ((0, 83, 1), (1, 81, 0.5), (1.5, 79, 0.5), (2, 74, 1.5)),
        ((0, 81, 1.5), (1.5, 84, 0.5), (2, 88, 1), (3, 86, 0.5), (3.5, 84, 0.5)),
        ((0, 81, 1), (1, 77, 1), (2, 76, 0.5), (2.5, 72, 0.5), (3, 69, 1)),
        ((0, 79, 0.5), (0.5, 84, 0.5), (1, 88, 1), (2, 86, 0.5), (2.5, 84, 1)),
        ((0, 83, 1), (1, 81, 0.5), (1.5, 79, 0.5), (2, 74, 0.5), (2.5, 76, 0.5),
         (3, 79, 0.5), (3.5, 76, 0.5)),
    ]

    def note(midi: int, start: float, length: float, volume: float, voice: str) -> None:
        frequency = 440.0 * 2 ** ((midi - 69) / 12)
        first = round(start * rate)
        count = round(length * rate)
        filtered = 0.0
        for i in range(min(count, len(mix) - first)):
            t = i / rate
            phase = (frequency * t) % 1.0
            if voice == "bass":
                tone = (math.sin(math.tau * phase) +
                        0.45 * math.sin(math.tau * phase * 2) * math.exp(-t * 12) +
                        0.20 * math.sin(math.tau * phase * 3) * math.exp(-t * 20))
            else:
                lead = voice == "lead"
                vibrato = 0.015 * math.sin(math.tau * 5.5 * t) * min(1.0, t / 0.15) if lead else 0.0
                phase_a = math.tau * (frequency * t + vibrato)
                phase_b = math.tau * (frequency * 1.003 * t + vibrato)
                decay = math.exp(-t * (1.8 if lead else 12))
                picked = (math.sin(phase_a) + 0.5 * math.sin(phase_b) +
                          0.45 * math.sin(phase_a * 2) * math.exp(-t * 6) +
                          0.28 * math.sin(phase_b * 3) * math.exp(-t * 9) +
                          0.18 * math.sin(phase_a * 4) * math.exp(-t * 14))
                # Overdrive followed by a cabinet-like low-pass rounds off the guitar's harsh edges.
                driven = math.tanh(picked * decay * (2.8 if lead else 3.8))
                filtered += 0.32 * (driven - filtered)
                tone = filtered
            attack, release = (0.004, 0.06)
            envelope = min(1.0, t / attack) * min(1.0, (length - t) / release)
            mix[first + i] += tone * envelope * volume

    def drum(start: float, kind: str) -> None:
        length = {"kick": 0.24, "snare": 0.28, "hat": 0.055,
                  "open_hat": 0.22, "crash": 0.8, "tom": 0.22}[kind]
        first = round(start * rate)
        previous_noise = 0.0
        for i in range(min(round(length * rate), len(mix) - first)):
            t = i / rate
            noise = rng.uniform(-1, 1)
            if kind == "kick":
                tone = math.sin(math.tau * (52 * t + 6 * (1 - math.exp(-t * 35))))
                value = tone * math.exp(-t * 20) * 0.42
            elif kind == "snare":
                body = math.sin(math.tau * 185 * t) * math.exp(-t * 30)
                gated_tail = noise * (0.6 * math.exp(-t * 20) + 0.18) * min(1.0, (length - t) / 0.018)
                value = (gated_tail + body * 0.55) * 0.42
            elif kind == "tom":
                value = math.sin(math.tau * (110 * t + 2 * (1 - math.exp(-t * 22)))) * math.exp(-t * 17) * 0.32
            else:
                high = noise - previous_noise
                previous_noise = noise
                decay = {"hat": 95, "open_hat": 18, "crash": 6}[kind]
                value = high * math.exp(-t * decay) * (0.13 if kind == "crash" else 0.075)
            mix[first + i] += value

    for bar, root in enumerate(roots):
        start = bar * 4 * beat
        if bar in (0, 4):
            drum(start, "crash")
        for offset, pitch, length in melody[bar]:
            when = start + offset * beat
            note(pitch, when, beat * length * 0.95, 0.20, "lead")
            note(pitch, when + beat * 0.75, beat * length * 0.95, 0.05, "lead")
        for step in range(8):
            when = start + step * beat / 2
            accent = step in (0, 3, 6)
            for interval in (0, 7, 12):
                note(root + interval, when, beat * (0.65 if accent else 0.24),
                     0.105 if accent else 0.075, "rhythm")
            bass = root - 12 if step != 7 else root - 5
            note(bass, when, beat * 0.42, 0.27, "bass")
            drum(when, "open_hat" if step == 7 else "hat")
        for pulse in range(4):
            when = start + pulse * beat
            if pulse in (0, 2):
                drum(when, "kick")
            if pulse % 2 == 1:
                drum(when, "snare")
        drum(start + 2.5 * beat, "kick")
        if bar in (3, 7):
            drum(start + 3.75 * beat, "snare")
            drum(start + 3.5 * beat, "tom")

    # Taper the loop seam, retaining headroom for the intro's comedy effects.
    for i, value in enumerate(mix):
        edge = min(1.0, i / (rate * 0.008), (len(mix) - 1 - i) / (rate * 0.012))
        mix[i] = math.tanh(value * 1.25) * 0.82 * edge
    write_samples(directory, "intro_music", rate, mix)


def main() -> None:
    directory = Path(sys.argv[1])
    directory.mkdir(parents=True, exist_ok=True)
    write_sound(directory, "place", 0.10, [440, 660])
    write_sound(directory, "repair", 0.22, [392, 523, 784])
    write_sound(directory, "bonk", 0.13, [150], noise=0.16, slide=-85)
    write_sound(directory, "trip", 0.34, [510], slide=-420)
    write_sound(directory, "break", 0.30, [85], noise=0.76, slide=-60)
    write_sound(directory, "whistle", 0.40, [1175, 1568, 1175])
    write_sound(directory, "win", 0.85, [523, 659, 784, 1047])
    write_sound(directory, "lose", 0.80, [392, 330, 262, 131])
    write_sound(directory, "clonk", 0.14, [330, 190], noise=0.3, slide=-120)
    write_sound(directory, "thud", 0.10, [110], noise=0.45, slide=-40)
    write_sound(directory, "news", 1.2, [392, 523, 659, 784, 659, 1047])
    write_sound(directory, "sting", 0.65, [262, 392, 523, 784], noise=0.08)
    write_sound(directory, "murmur", 1.3, [120, 145, 105, 160, 130, 110], noise=0.65, slide=25)
    write_fizz(directory)
    write_intro_music(directory)
    (directory / "generated.stamp").write_text("Original Riot Control SFX\n", encoding="ascii")


if __name__ == "__main__":
    main()
