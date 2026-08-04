#!/usr/bin/env python3
"""ループ書き出しモードのWAV 18本から stems_data.h を生成する。

使い方:
    python3 gen_stems_header.py <techno_kick.wav などが入ったディレクトリ>

WAVは track.html の「M5Stack用ループ書き出しモード」で書き出したもの
（22.05kHz / 16bit / モノラル）であること。
"""
import os
import struct
import sys

SONGS = ["techno", "lofi", "phonk"]
PARTS = ["hihat", "snare", "kick", "bass", "lead", "pad"]  # スケッチの表示順と同じ
RATE = 22050


def read_wav(path):
    with open(path, "rb") as f:
        if f.read(4) != b"RIFF":
            raise ValueError(f"{path}: not RIFF")
        f.read(4)
        if f.read(4) != b"WAVE":
            raise ValueError(f"{path}: not WAVE")
        fmt = None
        while True:
            hdr = f.read(8)
            if len(hdr) < 8:
                break
            cid, sz = hdr[:4], struct.unpack("<I", hdr[4:])[0]
            if cid == b"fmt ":
                fmt = f.read(sz)
            elif cid == b"data":
                data = f.read(sz)
                ch, rate = struct.unpack("<HI", fmt[2:8])
                bits = struct.unpack("<H", fmt[14:16])[0]
                if (ch, bits, rate) != (1, 16, RATE):
                    raise ValueError(
                        f"{path}: ch={ch} bits={bits} rate={rate} — "
                        "ループ書き出しモード（22.05kHz/16bit/mono）で書き出してください"
                    )
                return struct.unpack(f"<{sz // 2}h", data)
            else:
                f.seek(sz + (sz & 1), 1)
    raise ValueError(f"{path}: no data chunk")


def main():
    indir = sys.argv[1] if len(sys.argv) > 1 else "."
    outpath = os.path.join(os.path.dirname(os.path.abspath(__file__)), "stems_data.h")
    total = 0
    with open(outpath, "w") as out:
        out.write("// gen_stems_header.py により自動生成。手で編集しない。\n")
        out.write("#pragma once\n#include <stdint.h>\n\n")
        for song in SONGS:
            for part in PARTS:
                samples = read_wav(os.path.join(indir, f"{song}_{part}.wav"))
                total += len(samples) * 2
                name = f"STEM_{song.upper()}_{part.upper()}"
                out.write(f"const int16_t {name}[{len(samples)}] = {{\n")
                for i in range(0, len(samples), 16):
                    out.write(",".join(str(v) for v in samples[i : i + 16]) + ",\n")
                out.write("};\n\n")
        out.write("struct StemRef { const int16_t* data; uint32_t len; };\n")
        out.write(f"const StemRef STEMS[{len(SONGS)}][{len(PARTS)}] = {{\n")
        for song in SONGS:
            refs = ", ".join(
                f"{{ STEM_{song.upper()}_{p.upper()}, "
                f"sizeof(STEM_{song.upper()}_{p.upper()}) / 2 }}"
                for p in PARTS
            )
            out.write(f"  {{ {refs} }},\n")
        out.write("};\n")
    print(f"wrote {outpath} ({total / 1e6:.2f} MB of sample data)")


if __name__ == "__main__":
    main()
