# -*- coding: utf-8 -*-
import os
import json
import argparse


def should_skip_dir(dir_path, project_root):
    rel = os.path.relpath(dir_path, project_root)
    parts = rel.replace('\\', '/').split('/')

    skip_names = {'.git', 'Output', 'Debug', 'Release', 'settings',
                  '.history', 'backup', '.cursor', 'Doc'}
    if parts[0] in skip_names:
        return True

    if 'FreeRTOS' in parts and 'portable' in parts:
        # Allow the portable/, RVDS/ directories themselves so we walk into children
        if parts[-1] in ('portable', 'RVDS'):
            return False
        if 'MemMang' in parts:
            return False
        if 'ARM_CM4F' not in parts:
            return True

    if 'lwip' in parts and 'src' in parts and 'apps' in parts:
        return True

    if 'makefsdata-4g' in parts or 'makefsdata' in parts:
        return True

    return False


def should_skip_file(filepath, project_root):
    filename = os.path.basename(filepath)
    lower = filename.lower()
    if 'copy' in lower or '.bak' in lower:
        return True
    if lower.startswith('heap_') and lower != 'heap_4.c':
        return True
    if 'startup_gd32f' in lower:
        if '450_470' not in lower:
            return True
        # Only keep GCC version (.S uppercase), skip ARM (.s) and IAR (.s)
        if not filename.endswith('.S'):
            return True
    if 'fsdata.c' in lower:
        return True
    return False


def generate_compile_commands(project_root, compiler="arm-none-eabi-gcc", extra_defines=None):
    if extra_defines is None:
        extra_defines = [
            "-DGD32F470",
            "-DUSE_HAL_DRIVER",
            "-D__CC_ARM",
            "-DUSE_STDPERIPH_DRIVER",
        ]

    include_dirs = set()
    source_files = []

    print(f"Scanning directory: {project_root}")

    for root, dirs, files in os.walk(project_root):
        if should_skip_dir(root, project_root):
            dirs[:] = []
            continue

        has_header = False
        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext in ['.h', '.hpp', '.inc']:
                has_header = True
            elif ext in ['.c', '.cpp', '.cc', '.cxx', '.s', '.S']:
                full_path = os.path.join(root, file)
                if not should_skip_file(full_path, project_root):
                    source_files.append(os.path.abspath(full_path))

        if has_header:
            include_dirs.add(os.path.abspath(root))

    include_flags = []
    for inc in sorted(include_dirs):
        rel_inc = os.path.relpath(inc, project_root).replace('\\', '/')
        include_flags.append(f"-I{rel_inc}")

    compile_commands = []

    extra_compiler_flags = [
        "-mthumb",
        "-mcpu=cortex-m4",
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16",
        "-fsingle-precision-constant",
        "-Wno-all",
        "-Wno-extra",
        "-std=gnu11",
    ]

    for src in source_files:
        src_path = os.path.relpath(src, project_root).replace('\\', '/')
        work_path = os.path.abspath(project_root).replace('\\', '/')

        command_parts = [compiler] + extra_compiler_flags + extra_defines + include_flags + ["-c", src_path]
        command_parts = [part.replace('\\', '/') for part in command_parts]
        command = " ".join(command_parts)

        compile_commands.append({
            "directory": work_path,
            "command": command,
            "file": src_path
        })

    output_path = os.path.join(project_root, "compile_commands.json")
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(compile_commands, f, indent=2, ensure_ascii=False)

    print(f"Successfully generated {output_path}")
    print(f"Found {len(source_files)} source files and {len(include_dirs)} include directories.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate compile_commands.json for clangd by scanning directory.")
    parser.add_argument("--root", default=".", help="Project root directory (default: current directory)")
    parser.add_argument("--compiler", default="arm-none-eabi-gcc", help="Compiler executable name (default: arm-none-eabi-gcc)")
    args = parser.parse_args()

    root_abs = os.path.abspath(args.root)
    generate_compile_commands(root_abs, args.compiler)
