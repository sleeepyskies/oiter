# This script is used to run the flip metric between a blender reference image against all of oiter's OIT methods.
# It assumes running on a linux environment and that the oiter build dir is name "cmake-build-debug"
# Furthermore, blender must be installed and set in the system path

from pathlib import Path
import subprocess

oiter_root = Path(__file__).parent.parent.resolve()

oiter_executable = Path(oiter_root / "cmake-build-debug" / "oiter").absolute()
scene_file = "oiter://scripts/assets/stresstest.glb"
blend_file = Path(oiter_root / "scripts" / "assets" / "stresstest.blend").absolute()
output_dir = Path(oiter_root / "scripts" / "out").absolute()

output_dir.mkdir(parents=True, exist_ok=True)


# first, render the reference using blender
subprocess.run(
    [
        "blender",
        "-b",
        blend_file,
        "-o",
        output_dir / "reference",
        "-f",
        "0",
        "-F",
        "PNG",
        "-E",
        "BLENDER_EEVEE",
    ]
)

# render oiter images for each method
methods = ["dp", "ddp", "ab"]

for method in methods:
    path = f"oiter://scripts/out/{method}.png"
    print("rendering to ", path)
    subprocess.run(
        [
            oiter_executable,
            "render",
            "--scene",
            scene_file,
            "--method",
            method,
            "--camera-position",
            "0,0,5",
            "--camera-lookat",
            "0,0,4",
            "--output",
            path,
            "--width",
            "1920",
            "--height",
            "1080",
        ]
    )
