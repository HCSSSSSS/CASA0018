import os
import wave
import struct
import random

src = r"C:\Users\Chaos\Desktop\DvC\Birds"
dst = r"C:\Users\Chaos\Desktop\DvC\Birds_trimmed"
os.makedirs(dst, exist_ok=True)

def find_loudest_5s(filepath):
    with wave.open(filepath) as w:
        sr = w.getframerate()
        n_channels = w.getnchannels()
        sampwidth = w.getsampwidth()
        params = w.getparams()
        raw = w.readframes(w.getnframes())
    
    fmt = {1: 'B', 2: 'h', 4: 'i'}[sampwidth]
    samples = struct.unpack(f'{len(raw)//sampwidth}{fmt}', raw)
    
    window = sr * 5 * n_channels
    if len(samples) <= window:
        return raw, params
    
    best_start = 0
    best_energy = 0
    step = sr * n_channels
    
    for i in range(0, len(samples) - window, step):
        chunk = samples[i:i+window]
        energy = sum(s*s for s in chunk[::100])
        if energy > best_energy:
            best_energy = energy
            best_start = i
    
    best_frames = struct.pack(f'{window}{fmt}', *samples[best_start:best_start+window])
    return best_frames, params

files = [f for f in os.listdir(src) if f.endswith(".wav")]
random.shuffle(files)
selected = files[:80]

for i, f in enumerate(selected):
    print(f"Processing {i+1}/80: {f}")
    frames, params = find_loudest_5s(os.path.join(src, f))
    out_path = os.path.join(dst, f)
    with wave.open(out_path, 'w') as out:
        out.setparams(params)
        out.writeframes(frames)

print(f"Done. Output saved to {dst}")
