# This script is used to run the flip metric between a blender reference image
# against all of oiter's OIT methods.
# It assumes Oiter was built in Release mode through its Conan CMake preset.
# Furthermore, blender must be installed and set in the system path

from pathlib import Path
import struct
import subprocess

OITER_ROOT = Path(__file__).resolve().parent.parent
OITER_EXECUTABLE = OITER_ROOT / "build" / "build" / "Release" / "oiter"
SCENE_FILE = "oiter://scripts/assets/stresstest.glb"
BLEND_FILE = OITER_ROOT / "scripts" / "assets" / "stresstest.blend"
OUTPUT_DIR = OITER_ROOT / "scripts" / "out"
REFERENCE_IMAGE = OUTPUT_DIR / "reference0000.png"
METHODS = ("dp", "ddp", "ab")
SEPARATOR = "-" * 65
WIDTH = 1920
HEIGHT = 1080
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def run(command: list[str | Path]) -> None:
    subprocess.run(
        [str(argument) for argument in command],
        cwd=OITER_ROOT,
        check=True,
    )


def require_png(path: Path) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise RuntimeError(f"Expected output file was not created: {path}")

    with path.open("rb") as image:
        header = image.read(24)

    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise RuntimeError(f"Output is not a valid PNG: {path}")

    dimensions = struct.unpack(">II", header[16:24])
    if dimensions != (WIDTH, HEIGHT):
        raise RuntimeError(
            f"Unexpected PNG dimensions for {path}: "
            f"expected {WIDTH}x{HEIGHT}, got {dimensions[0]}x{dimensions[1]}"
        )


def print_step(message: str) -> None:
    print(SEPARATOR)
    print(message)
    print(SEPARATOR)


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # first, render the reference using blender
    REFERENCE_IMAGE.unlink(missing_ok=True)
    run(
        [
            "blender",
            "-b",
            BLEND_FILE,
            "-o",
            OUTPUT_DIR / "reference",
            "-f",
            "0",
            "-F",
            "PNG",
            "-E",
            "BLENDER_EEVEE",
        ]
    )
    require_png(REFERENCE_IMAGE)

    # render oiter images for each method
    for method in METHODS:
        output_path = OUTPUT_DIR / f"{method}.png"
        virtual_output_path = f"oiter://scripts/out/{method}.png"
        output_path.unlink(missing_ok=True)
        print_step(f"rendering to {virtual_output_path}")
        run(
            [
                OITER_EXECUTABLE,
                "render",
                "--scene",
                SCENE_FILE,
                "--method",
                method,
                "--camera-position",
                "0,0,2",
                "--camera-lookat",
                "-1,0,2",
                "--output",
                virtual_output_path,
                "--width",
                str(WIDTH),
                "--height",
                str(HEIGHT),
                "--loglevel",
                "debug",
            ]
        )
        require_png(output_path)

    # then, run flip on the images
    for method in METHODS:
        output_path = OUTPUT_DIR / f"{method}.png"
        print_step(f"performing flip on {method}")
        run(["flip", "-r", REFERENCE_IMAGE, "-t", output_path, "-d", OUTPUT_DIR])


if __name__ == "__main__":
    main()
