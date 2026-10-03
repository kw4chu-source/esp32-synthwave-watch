# Post-build: patch esp_app_desc_t + eksport samego app .bin do dist/
#
# Arduino-ESP32 linkuje prekompilowany esp_app_format, wiec w obrazie siedzi
# project_name "arduino-lib-builder" i wersja IDF zamiast naszej. Cardputer
# (etap 3) czyta te pola do menu, wiec wpisujemy tu custom_app_name/version,
# przeliczamy sume XOR obrazu i dolaczony SHA-256.
#
# Patch idzie w miejscu na firmware.bin, wiec `pio run -t upload` wgrywa juz
# poprawiony obraz. Kopia trafia do dist/<nazwa>_<wersja>.bin (gotowa do
# /esp32bin na Cardputerze - NIE merged, sam obraz aplikacji).
Import("env")

import hashlib
import shutil
import struct
import subprocess
import sys
from pathlib import Path

IMAGE_MAGIC = 0xE9
APP_DESC_MAGIC = 0xABCD5432
CHIP_ID_ESP32 = 0x0000
HEADER_LEN = 24
SEG_HDR_LEN = 8
CHECKSUM_SEED = 0xEF

# Offsety pol w esp_app_desc_t (wzgledem poczatku struktury)
DESC_VERSION = 16       # char version[32]
DESC_PROJECT = 48       # char project_name[32]


def _field(value: str) -> bytes:
    raw = value.encode()
    if len(raw) > 31:
        raise ValueError(f"za dlugie pole app_desc: {value!r}")
    return raw.ljust(32, b"\0")


def patch_image(data: bytearray, name: str, version: str) -> None:
    if data[0] != IMAGE_MAGIC:
        raise ValueError(f"zly magic obrazu: 0x{data[0]:02X}")
    seg_count = data[1]
    chip_id = struct.unpack_from("<H", data, 12)[0]
    if chip_id != CHIP_ID_ESP32:
        raise ValueError(f"obraz nie dla ESP32 (chip_id=0x{chip_id:04X})")
    hash_appended = data[23] == 1

    # Przejscie po segmentach: pozycja app_desc (poczatek 1. segmentu) i koniec danych
    pos = HEADER_LEN
    first_seg_data = pos + SEG_HDR_LEN
    for _ in range(seg_count):
        seg_len = struct.unpack_from("<I", data, pos + 4)[0]
        pos += SEG_HDR_LEN + seg_len
    segments_end = pos

    if struct.unpack_from("<I", data, first_seg_data)[0] != APP_DESC_MAGIC:
        raise ValueError("brak esp_app_desc_t na poczatku pierwszego segmentu")

    # Suma XOR liczona tylko po danych segmentow - zmiana bajtow w segmencie
    # zmienia ja o XOR(stare ^ nowe), wiec aktualizujemy roznicowo.
    checksum_pos = segments_end + (15 - (segments_end % 16))
    checksum = data[checksum_pos]
    for off, value in ((DESC_VERSION, version), (DESC_PROJECT, name)):
        start = first_seg_data + off
        new = _field(value)
        for i, b in enumerate(new):
            checksum ^= data[start + i] ^ b
            data[start + i] = b
    data[checksum_pos] = checksum

    if hash_appended:
        digest_pos = checksum_pos + 1
        data[digest_pos:digest_pos + 32] = hashlib.sha256(data[:digest_pos]).digest()


def write_wokwi_image(build_dir: Path, app: bytes) -> None:
    # Obraz tylko do Wokwi: bootloader + partycje + app w FACTORY (0x10000),
    # bez otadata. Wokwi zapetla sie przy app tylko w ota_0 (QEMU i sprzet nie),
    # wiec logike testujemy w tym ukladzie.
    img = bytearray(b"\xff" * (0x10000 + len(app)))
    for off, name in ((0x1000, "bootloader.bin"), (0x8000, "partitions.bin")):
        part = (build_dir / name).read_bytes()
        img[off:off + len(part)] = part
    img[0x10000:] = app
    (build_dir / "wokwi.bin").write_bytes(img)


def after_bin(source, target, env):
    name = env.GetProjectOption("custom_app_name")
    version = env.GetProjectOption("custom_app_version")
    bin_path = Path(str(target[0]))

    data = bytearray(bin_path.read_bytes())
    patch_image(data, name, version)
    bin_path.write_bytes(data)

    # Kontrola rozmiaru wzgledem ota_0 (0x2E0000) - to samo sprawdzi Cardputer
    ota0_size = 0x2E0000
    if len(data) > ota0_size:
        raise SystemExit(f"obraz {len(data)} B > ota_0 {ota0_size} B")

    write_wokwi_image(bin_path.parent, data)

    dist = Path(env.subst("$PROJECT_DIR")) / "dist"
    dist.mkdir(exist_ok=True)
    out = dist / f"{name}_{version}.bin"
    shutil.copyfile(bin_path, out)
    print(f"[post_build] app_desc: {name} {version}; "
          f"{len(data)} B ({len(data) * 100 // ota0_size}% ota_0) -> {out}")

    # bez kabla: jesli Cardputer ma otwarty Upload, obraz trafia od razu do /esp32bin
    push = Path(env.subst("$PROJECT_DIR")).parent / "cardputer-hub" / "tools" / "push.py"
    if push.exists():
        subprocess.run([sys.executable, str(push), str(out), "--quiet"], timeout=120)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_bin)
