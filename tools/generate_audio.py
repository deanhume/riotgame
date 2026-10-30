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
    write_fizz(directory)
    (directory / "generated.stamp").write_text("Original Riot Control SFX\n", encoding="ascii")


if __name__ == "__main__":
    main()
