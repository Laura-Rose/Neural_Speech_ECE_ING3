import wave
import struct

INPUT_FILE = "audio.txt"
OUTPUT_FILE = "electronique.wav"
SAMPLE_RATE = 8000

samples = []

with open(INPUT_FILE, "r") as f:
    for line in f:
        line = line.strip()

        if not line:
            continue
        if line == "START" or line == "END":
            continue

        value = int(line)

        # ADC Arduino Due : 0 a 4095
        # on recentre autour de 0
        value = value - 2048

        # mise a l echelle vers int16
        value = value * 16

        # saturation de securite
        if value > 32767:
            value = 32767
        if value < -32768:
            value = -32768

        samples.append(value)

with wave.open(OUTPUT_FILE, "w") as wav_file:
    wav_file.setnchannels(1)        # mono
    wav_file.setsampwidth(2)        # 16 bits
    wav_file.setframerate(SAMPLE_RATE)

    for s in samples:
        wav_file.writeframes(struct.pack("<h", s))

print("Fichier cree :", OUTPUT_FILE)