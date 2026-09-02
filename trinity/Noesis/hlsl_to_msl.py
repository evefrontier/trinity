#!/usr/bin/env python3
# Copyright (c) 2026 CCP ehf.
"""Translate Noesis D3D12 ShaderVS/ShaderPS.hlsl into MSL for TrinityAL Metal.

Keeps the vendor preprocessor switches. Vertex attribute indices follow
Tr2NoesisRenderDevice::AddVertexAttributes (packed sequentially among present
HAS_* inputs). Constant buffers land at TrinityAL's CBUFFER(i) = buffer(4+i).
Entry points are mainVS / mainPS, matching Tr2ShaderProgramALMetal.
"""
from __future__ import print_function

import argparse
import re
import sys


CBUFFER_OFFSET_COMMENT = """#include <metal_stdlib>
using namespace metal;

// Must match trinityal/metal/MetalWorkQueue.h METAL_CONST_BUFFER_OFFSET.
#define CBUFFER( i ) buffer( 4 + i )

// Packed vertex attribute indices, same order as VERTEX_ATTRS / AddVertexAttributes.
#define ATTR_POSITION 0
#if HAS_COLOR
#define ATTR_COLOR ( ATTR_POSITION + 1 )
#else
#define ATTR_COLOR ATTR_POSITION
#endif
#if HAS_UV0
#define ATTR_UV0 ( ATTR_COLOR + 1 )
#else
#define ATTR_UV0 ATTR_COLOR
#endif
#if HAS_UV1
#define ATTR_UV1 ( ATTR_UV0 + 1 )
#else
#define ATTR_UV1 ATTR_UV0
#endif
#if HAS_COVERAGE
#define ATTR_COVERAGE ( ATTR_UV1 + 1 )
#else
#define ATTR_COVERAGE ATTR_UV1
#endif
#if HAS_RECT
#define ATTR_RECT ( ATTR_COVERAGE + 1 )
#else
#define ATTR_RECT ATTR_COVERAGE
#endif
#if HAS_TILE
#define ATTR_TILE ( ATTR_RECT + 1 )
#else
#define ATTR_TILE ATTR_RECT
#endif
#if HAS_IMAGE_POSITION
#define ATTR_IMAGE_POS ( ATTR_TILE + 1 )
#else
#define ATTR_IMAGE_POS ATTR_TILE
#endif

"""


def replace_mul(source):
    """HLSL mul(v, M) -> MSL v * M. Nested parens are matched, not regex-greedy."""
    out = []
    i = 0
    while i < len(source):
        if source.startswith("mul(", i) and (i == 0 or not (source[i - 1].isalnum() or source[i - 1] == "_")):
            depth = 1
            j = i + 4
            comma = None
            while j < len(source) and depth:
                c = source[j]
                if c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                    if depth == 0:
                        break
                elif c == "," and depth == 1 and comma is None:
                    comma = j
                j += 1
            if depth == 0 and comma is not None:
                left = source[i + 4 : comma].strip()
                right = source[comma + 1 : j].strip()
                out.append("(%s) * (%s)" % (left, right))
                i = j + 1
                continue
        out.append(source[i])
        i += 1
    return "".join(out)


def replace_sample_grad(source):
    """pattern.SampleGrad(s, uv, ddx(x), ddy(x)) -> pattern.sample(s, uv, gradient2d(dfdx(x), dfdy(x)))."""

    def repl(match):
        obj = match.group(1)
        args = match.group(2)
        parts = []
        depth = 0
        current = []
        for c in args:
            if c == "," and depth == 0:
                parts.append("".join(current).strip())
                current = []
            else:
                if c == "(":
                    depth += 1
                elif c == ")":
                    depth -= 1
                current.append(c)
        parts.append("".join(current).strip())
        if len(parts) != 4:
            return match.group(0)
        sampler, uv, ddx_arg, ddy_arg = parts
        return "%s.sample(%s, %s, gradient2d(%s, %s))" % (obj, sampler, uv, ddx_arg, ddy_arg)

    return re.sub(r"(\w+)\.SampleGrad\s*\((.*)\)", repl, source)


def rewrite_semantics(line, is_vs_input):
    """Rewrite `: SEMANTIC` on struct members to Metal attributes."""
    stripped = line.lstrip()
    if stripped.startswith("#") or ":" not in line:
        return line

    match = re.search(
        r"^(\s*)(nointerpolation\s+)?(.+?)\s+(\w+)\s*:\s*([A-Za-z_]+)(\d*)\s*;(.*)$",
        line,
    )
    if not match:
        return line

    indent, flat, typ, name, semantic, index, tail = match.groups()
    flat = "[[flat]] " if flat else ""
    semantic = semantic.upper()
    attrs = []

    if semantic == "POSITION" and is_vs_input:
        attrs.append("attribute(ATTR_POSITION)")
    elif semantic == "COLOR" and is_vs_input:
        attrs.append("attribute(ATTR_COLOR)")
    elif semantic == "TEXCOORD" and is_vs_input:
        tex_map = {
            "0": "ATTR_UV0",
            "1": "ATTR_UV1",
            "2": "ATTR_COVERAGE",
            "3": "ATTR_RECT",
            "4": "ATTR_TILE",
            "5": "ATTR_IMAGE_POS",
        }
        attrs.append("attribute(%s)" % tex_map.get(index or "0", "ATTR_UV0"))
    elif semantic == "COVERAGE" and is_vs_input:
        attrs.append("attribute(ATTR_COVERAGE)")
    elif semantic == "RECT" and is_vs_input:
        attrs.append("attribute(ATTR_RECT)")
    elif semantic == "TILE" and is_vs_input:
        attrs.append("attribute(ATTR_TILE)")
    elif semantic == "IMAGE_POSITION" and is_vs_input:
        attrs.append("attribute(ATTR_IMAGE_POS)")
    elif semantic == "SV_POSITION":
        attrs.append("position")
    elif semantic.startswith("SV_TARGET"):
        target = index or (semantic[len("SV_TARGET") :] or "0")
        attrs.append("color(%s)" % target)

    if flat and "flat" not in "".join(attrs):
        attrs.insert(0, "flat")

    attr_src = ""
    if attrs:
        attr_src = " [[" + ", ".join(attrs) + "]]"
    return "%s%s%s %s%s;%s\n" % (indent, flat if "flat" not in attr_src else "", typ.strip(), name, attr_src, tail)


def convert_cbuffers(source):
    pattern = re.compile(
        r"cbuffer\s+(\w+)\s*:\s*register\s*\(\s*b(\d+)\s*\)\s*\{",
        re.MULTILINE,
    )
    cbuffers = []
    out = []
    last = 0
    for match in pattern.finditer(source):
        out.append(source[last : match.start()])
        name, slot = match.group(1), int(match.group(2))
        cbuffers.append((name, slot))
        depth = 1
        i = match.end()
        while i < len(source) and depth:
            if source[i] == "{":
                depth += 1
            elif source[i] == "}":
                depth -= 1
            i += 1
        body = source[match.end() : i - 1]
        out.append("struct %s\n{%s};\n" % (name, body))
        last = i
    out.append(source[last:])
    return "".join(out), cbuffers


def strip_texture_decls(source):
    source = re.sub(r"^Texture2D\s+\w+\s*:\s*register\s*\(\s*t\d+\s*\)\s*;\s*$", "", source, flags=re.MULTILINE)
    source = re.sub(r"^SamplerState\s+\w+\s*:\s*register\s*\(\s*s\d+\s*\)\s*;\s*$", "", source, flags=re.MULTILINE)
    return source


def vs_params(cbuffers):
    lines = ["    In i [[stage_in]]"]
    for name, slot in cbuffers:
        if slot == 0:
            lines.append("    , constant %s& cb0 [[CBUFFER(0)]]" % name)
        elif slot == 1:
            lines.append("#if SDF")
            lines.append("    , constant %s& cb1 [[CBUFFER(1)]]" % name)
            lines.append("#endif")
    return "\n".join(lines)


def ps_params(cbuffers):
    lines = ["    In i [[stage_in]]"]
    names = {slot: name for name, slot in cbuffers}
    if 0 in names:
        lines.append("#if EFFECT_RGBA || PAINT_LINEAR || PAINT_RADIAL || PAINT_PATTERN")
        lines.append("    , constant %s& cb0 [[CBUFFER(0)]]" % names[0])
        lines.append("#endif")
    if 1 in names:
        lines.append("#if EFFECT_BLUR || EFFECT_SHADOW")
        lines.append("    , constant %s& cb1 [[CBUFFER(1)]]" % names[1])
        lines.append("#endif")
    lines += [
        "#if PAINT_PATTERN || EFFECT_DOWNSAMPLE || EFFECT_UPSAMPLE",
        "    , texture2d<float> pattern [[texture(0)]]",
        "    , sampler patternSampler [[sampler(0)]]",
        "#endif",
        "#if PAINT_LINEAR || PAINT_RADIAL",
        "    , texture2d<float> ramps [[texture(1)]]",
        "    , sampler rampsSampler [[sampler(1)]]",
        "#endif",
        "#if EFFECT_OPACITY || EFFECT_SHADOW || EFFECT_BLUR || EFFECT_UPSAMPLE",
        "    , texture2d<float> image [[texture(2)]]",
        "    , sampler imageSampler [[sampler(2)]]",
        "#endif",
        "#if EFFECT_SDF || EFFECT_SDF_LCD",
        "    , texture2d<float> glyphs [[texture(3)]]",
        "    , sampler glyphsSampler [[sampler(3)]]",
        "#endif",
        "#if EFFECT_SHADOW || EFFECT_BLUR",
        "    , texture2d<float> shadow [[texture(4)]]",
        "    , sampler shadowSampler [[sampler(4)]]",
        "#endif",
    ]
    return "\n".join(lines)


def vs_aliases():
    return """
#if STEREO_RENDERING
    constant float4x4* projectionMtx = cb0.projectionMtx;
#else
    float4x4 projectionMtx = cb0.projectionMtx;
#endif
#if SDF
    float2 textureSize = cb1.textureSize;
#endif
"""


def ps_aliases():
    return """
#if EFFECT_RGBA
    float4 rgba = cb0.rgba;
#endif
#if PAINT_LINEAR || PAINT_PATTERN
    float opacity = cb0.opacity;
#endif
#if PAINT_RADIAL
    float4 radialGrad0 = cb0.radialGrad0;
    float3 radialGrad1 = cb0.radialGrad1;
#endif
#if EFFECT_BLUR
    float blend = cb1.blend;
#endif
#if EFFECT_SHADOW
    float4 shadowColor = cb1.shadowColor;
    float2 shadowOffset = cb1.shadowOffset;
    float blend = cb1.blend;
#endif
"""


def _insert_after_open_brace(source, marker, insertion):
    idx = source.find(marker)
    if idx < 0:
        raise SystemExit("hlsl_to_msl: missing marker %r" % marker)
    brace = source.find("{", idx)
    if brace < 0:
        raise SystemExit("hlsl_to_msl: missing '{' after %r" % marker)
    return source[: brace + 1] + insertion + source[brace + 1 :]


def rewrite_mains(source, stage, cbuffers):
    if stage == "vs":
        signature = "vertex Out mainVS(\n%s\n)" % vs_params(cbuffers)
        source, n = re.subn(
            r"void\s+main\s*\(\s*in\s+In\s+i\s*,\s*out\s+Out\s+o\s*\)",
            signature,
            source,
            count=1,
        )
        if n != 1:
            raise SystemExit("hlsl_to_msl: could not find VS main(in In, out Out)")
        source = _insert_after_open_brace(source, "vertex Out mainVS(", "\n    Out o;" + vs_aliases())
        last = source.rfind("}")
        source = source[:last] + "    return o;\n" + source[last:]
        return source

    signature = "fragment Out mainPS(\n%s\n)" % ps_params(cbuffers)
    source, n = re.subn(
        r"Out\s+main\s*\(\s*in\s+In\s+i\s*\)",
        signature,
        source,
        count=1,
    )
    if n != 1:
        raise SystemExit("hlsl_to_msl: could not find PS Out main(in In)")
    return _insert_after_open_brace(source, "fragment Out mainPS(", ps_aliases())


def rewrite_struct_block(source, struct_name, is_vs_input):
    pattern = re.compile(r"(struct\s+" + struct_name + r"\s*\{)(.*?)(\n\};)", re.DOTALL)
    match = pattern.search(source)
    if not match:
        return source
    body_lines = []
    for line in match.group(2).splitlines(True):
        if not line.endswith("\n"):
            line += "\n"
        body_lines.append(rewrite_semantics(line, is_vs_input))
    body = "".join(body_lines)
    return source[: match.start()] + match.group(1) + body + match.group(3) + source[match.end() :]


def translate(source, stage):
    source = re.sub(r"^#pragma warning.*$", "", source, flags=re.MULTILINE)
    source, cbuffers = convert_cbuffers(source)
    source = re.sub(r"^float4\s+main_brush\s*\(\s*float2\s+uv\s*\)\s*;\s*$", "", source, flags=re.MULTILINE)
    source = strip_texture_decls(source)
    source = rewrite_struct_block(source, "In", is_vs_input=(stage == "vs"))
    source = rewrite_struct_block(source, "Out", is_vs_input=False)
    source = replace_sample_grad(source)
    source = source.replace(".Sample(", ".sample(")
    source = replace_mul(source)
    source = re.sub(r"\blerp\s*\(", "mix(", source)
    source = re.sub(r"\bfrac\s*\(", "fract(", source)
    source = re.sub(r"\bddx\s*\(", "dfdx(", source)
    source = re.sub(r"\bddy\s*\(", "dfdy(", source)
    source = rewrite_mains(source, stage, cbuffers)
    # float4x4 * vector is column-vector; Noesis uses row-vector mul(v, M) which
    # we already rewrote to (v) * (M).
    return CBUFFER_OFFSET_COMMENT + source


def main():
    parser = argparse.ArgumentParser(description="Translate Noesis HLSL to MSL for TrinityAL")
    parser.add_argument("--stage", choices=("vs", "ps"), required=True)
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    with open(args.input, "r") as handle:
        source = handle.read()
    translated = translate(source, args.stage)
    with open(args.output, "w") as handle:
        handle.write(translated)


if __name__ == "__main__":
    sys.exit(main())
