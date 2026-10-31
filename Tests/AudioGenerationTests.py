import array
import importlib.util
import math
from pathlib import Path
import tempfile
import unittest
import wave


spec = importlib.util.spec_from_file_location(
    "generate_audio", Path(__file__).resolve().parents[1] / "tools" / "generate_audio.py"
)
audio = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audio)


class IntroMusicTests(unittest.TestCase):
    def test_theme_is_playable_with_headroom_and_a_quiet_loop_seam(self):
        with tempfile.TemporaryDirectory() as folder:
            audio.write_intro_music(Path(folder))
            with wave.open(str(Path(folder) / "intro_music.wav"), "rb") as track:
                self.assertEqual((track.getnchannels(), track.getsampwidth()), (1, 2))
                duration = track.getnframes() / track.getframerate()
                self.assertGreater(duration, 10)
                self.assertLess(duration, 23)
                samples = array.array("h", track.readframes(track.getnframes()))
            peak = max(abs(value) for value in samples)
            self.assertLess(peak, 31000, "Music must leave headroom for comedy effects")
            self.assertGreater(peak, 8000, "The music must not be silent or inaudible")
            self.assertEqual((samples[0], samples[-1]), (0, 0), "The loop seam must avoid clicks")
            for offset in range(0, len(samples) - 22050, 22050):
                second = samples[offset:offset + 22050]
                rms = math.sqrt(sum(value * value for value in second) / len(second))
                self.assertGreater(rms, 800, "No unexpected silent gaps in the backing track")


if __name__ == "__main__":
    unittest.main()
