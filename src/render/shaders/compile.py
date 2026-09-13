import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("shadercross", help="Path to the SDL_shadercross executable")
args = parser.parse_args()
directory = Path(__file__).resolve().parent
lines = ["#pragma once", "", "#include <cstdint>", "",
         "namespace svanes::internal {", ""]
with tempfile.TemporaryDirectory() as temporary:
    for name in ("texture", "extract", "blur", "composite"):
        for format in ("SPIRV", "DXIL", "MSL"):
            output = Path(temporary) / f"{name}.{format.lower()}"
            subprocess.run([args.shadercross,
                            str(directory / f"bloom_{name}.frag.hlsl"),
                            "-s", "HLSL", "-d", format, "-t", "fragment",
                            "-e", "main", "-o", str(output)], check=True)
            data = output.read_bytes()
            if format == "MSL":
                data += b"\0"
            lines.append(f"inline constexpr std::uint8_t {name}_{format.lower()}[]{{")
            for offset in range(0, len(data), 16):
                lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in data[offset:offset+16]) + ",")
            lines.extend(["};", ""])
lines.append("}")
(directory / "bloom_shaders.hpp").write_text("\n".join(lines) + "\n", encoding="utf-8")
