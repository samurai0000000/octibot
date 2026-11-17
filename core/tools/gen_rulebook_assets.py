#!/usr/bin/env python3
#
# gen_rulebook_assets.py
#
# Copyright (C) 2026, Charles Chiou
#
# Generates OctiRulebookAssets.hxx from canonical PNGs in assets/rulebook/.
# Supports --stub / --allow-stub so downstream core/ mirrors (such as octibot)
# that set OCTI_BOOTSTRAP_SOUND_ASSETS=ON without carrying
# assets/rulebook/ configure and compile cleanly with zero-byte descriptors.

import argparse
import os
import shutil
import sys

RULEBOOK_IMAGE_IDS = [
    "rule_01_2p_3b_setup",
    "rule_02_2p_1b_setup",
    "rule_03_prongs_8dir",
    "rule_04_step_vs_jump",
    "rule_05_multihop_chain",
    "rule_06_stack_split_loop",
    "rule_07_reserve_liberate",
    "rule_08_edgeless_wrap",
    "rule_09_superprongs",
    "rule_10_4p_partnership",
    "rule_11_ofk_6x7",
    "rule_12_victory_infiltration",
]

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
MAX_PNG_BYTES = 80 * 1024
MAX_TOTAL_BYTES = 1024 * 1024


def format_c_array(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    return "\n".join(lines)


def build_header_text(input_dir: str, force_stub: bool, allow_stub: bool) -> str:
    parts = [
        "/*",
        " * OctiRulebookAssets.hxx",
        " *",
        " * Copyright (C) 2026, Charles Chiou",
        " */",
        "",
        "#ifndef OCTI_RULEBOOK_ASSETS_HXX",
        "#define OCTI_RULEBOOK_ASSETS_HXX",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "namespace octi {",
        "",
    ]

    total_bytes = 0
    for img_id in RULEBOOK_IMAGE_IDS:
        png_path = os.path.join(input_dir, f"{img_id}.png")
        if force_stub or (not os.path.isfile(png_path) and allow_stub):
            parts.append(f"static constexpr std::size_t kOctiRulebookPngSize_{img_id} = 0u;")
            parts.append(f"static constexpr std::uint8_t kOctiRulebookPng_{img_id}[1] = {{ 0x00 }};")
            parts.append("")
            continue

        if not os.path.isfile(png_path):
            raise FileNotFoundError(
                f"Missing canonical rulebook PNG: {png_path} "
                "(enable OCTI_BOOTSTRAP_RULEBOOK_ASSETS=ON for stub builds)"
            )

        with open(png_path, "rb") as f:
            raw = f.read()

        if len(raw) < 8 or raw[:8] != PNG_MAGIC:
            raise ValueError(f"Invalid PNG signature in {png_path}")
        if len(raw) > MAX_PNG_BYTES:
            raise ValueError(
                f"Rulebook PNG exceeds 80 KB budget ({len(raw)} bytes): {png_path}"
            )
        total_bytes += len(raw)
        if total_bytes >= MAX_TOTAL_BYTES:
            raise ValueError(
                f"Total rulebook PNG payload ({total_bytes} bytes) exceeds 1 MB budget"
            )

        parts.append(
            f"static constexpr std::size_t kOctiRulebookPngSize_{img_id} = {len(raw)}u;"
        )
        parts.append(
            f"static constexpr std::uint8_t kOctiRulebookPng_{img_id}[{len(raw)}] = {{"
        )
        parts.append(format_c_array(raw))
        parts.append("};")
        parts.append("")

    parts.extend(
        [
            "} // namespace octi",
            "",
            "#endif // OCTI_RULEBOOK_ASSETS_HXX",
            "",
            "/*",
            " * Local variables:",
            " * mode: C++",
            ' * c-file-style: "BSD"',
            " * c-basic-offset: 4",
            " * tab-width: 4",
            " * indent-tabs-mode: nil",
            " * End:",
            " */",
            "",
        ]
    )
    return "\n".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate OctiRulebookAssets.hxx")
    parser.add_argument("--input-dir", required=True, help="Path to assets/rulebook/")
    parser.add_argument("--output", required=True, help="Output OctiRulebookAssets.hxx path")
    parser.add_argument("--stub", action="store_true", help="Force zero-byte stub descriptors")
    parser.add_argument(
        "--allow-stub",
        action="store_true",
        help="Emit zero-byte stub descriptors when PNG files are absent",
    )
    args = parser.parse_args()

    header_text = build_header_text(args.input_dir, args.stub, args.allow_stub)
    out_dir = os.path.dirname(os.path.abspath(args.output))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    if os.path.isfile(args.output):
        with open(args.output, "r", encoding="utf-8") as existing:
            if existing.read() == header_text:
                return 0

    tmp_path = args.output + ".tmp"
    with open(tmp_path, "w", encoding="utf-8", newline="\n") as out_f:
        out_f.write(header_text)
    shutil.move(tmp_path, args.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
