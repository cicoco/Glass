#!/usr/bin/env python3
import argparse
import os
import socket
import struct
import sys
import time


HEADER_FMT = "<2sBBBIHI"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

TYPE_NAMES = {
    0x01: "IMAGE",
    0x02: "AUDIO",
    0x10: "CONTROL_WAKE",
    0x11: "CONTROL_SESSION_END",
}


def recv_all(sock: socket.socket, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        piece = sock.recv(size - len(chunks))
        if not piece:
            raise ConnectionError("socket closed")
        chunks.extend(piece)
    return bytes(chunks)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Receive JuneGlass JG stream frames.")
    parser.add_argument("host", help="glasses IP or hostname")
    parser.add_argument("--port", type=int, default=8765, help="TCP port, default 8765")
    parser.add_argument(
        "--save-images",
        default="",
        help="directory to save IMAGE payloads as .jpg files",
    )
    parser.add_argument(
        "--max-images",
        type=int,
        default=0,
        help="stop after saving this many IMAGE frames; 0 means unlimited",
    )
    return parser.parse_args()


def ensure_dir(path: str) -> None:
    if path:
        os.makedirs(path, exist_ok=True)


def read_frame(sock: socket.socket) -> tuple[dict, bytes]:
    raw = recv_all(sock, HEADER_SIZE)
    magic, version, frame_type, flags, timestamp_ms, session_id, payload_len = struct.unpack(
        HEADER_FMT, raw
    )
    if magic != b"JG" or version != 1:
        raise ValueError(f"invalid header magic/version: {magic!r} v={version}")
    if payload_len > 512 * 1024:
        raise ValueError(f"payload too large: {payload_len}")
    payload = recv_all(sock, payload_len) if payload_len else b""
    header = {
        "type": frame_type,
        "flags": flags,
        "ts": timestamp_ms,
        "sid": session_id,
        "len": payload_len,
    }
    return header, payload


def save_image(directory: str, count: int, header: dict, payload: bytes) -> None:
    filename = f"{count:05d}_sid{header['sid']}_ts{header['ts']}.jpg"
    path = os.path.join(directory, filename)
    with open(path, "wb") as fh:
        fh.write(payload)


def main() -> int:
    args = parse_args()
    ensure_dir(args.save_images)

    image_count = 0
    audio_bytes = 0
    wake_count = 0
    start = time.time()

    print(f"[CONNECT] {args.host}:{args.port}")
    with socket.create_connection((args.host, args.port), timeout=10) as sock:
        print("[CONNECTED]")
        while True:
            header, payload = read_frame(sock)
            frame_name = TYPE_NAMES.get(header["type"], hex(header["type"]))
            print(
                f"[FRAME] type={frame_name} flags=0x{header['flags']:02x} "
                f"sid={header['sid']} ts={header['ts']} len={header['len']}"
            )

            if header["type"] == 0x01:
                image_count += 1
                if args.save_images:
                    save_image(args.save_images, image_count, header, payload)
                if args.max_images and image_count >= args.max_images:
                    break
            elif header["type"] == 0x02:
                audio_bytes += len(payload)
            elif header["type"] == 0x10:
                wake_count += 1
                if len(payload) == 8:
                    mode, _, width, height, fps_x10 = struct.unpack("<BBHHH", payload)
                    print(
                        f"[WAKE] mode={mode} width={width} height={height} fps={fps_x10 / 10:.1f}"
                    )
            elif header["type"] == 0x11:
                print("[SESSION] end")

    elapsed = max(time.time() - start, 0.001)
    print(
        f"[SUMMARY] images={image_count} wake={wake_count} "
        f"audio_bytes={audio_bytes} elapsed_s={elapsed:.2f}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\n[STOP] interrupted", file=sys.stderr)
        raise SystemExit(130)
