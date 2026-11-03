# -*- coding: utf-8 -*-
import os
import re
import glob
import json
import shutil
import argparse
import subprocess


def find_arm_gcc(explicit=None):
    """定位 arm-none-eabi-gcc：显式参数 > PATH > 常见安装目录"""
    if explicit and os.path.isfile(explicit):
        return explicit
    found = shutil.which("arm-none-eabi-gcc")
    if found:
        return found
    patterns = [
        r"C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe",
        r"C:\Program Files\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe",
        r"C:\Program Files (x86)\GNU Arm Embedded Toolchain\*\bin\arm-none-eabi-gcc.exe",
        r"C:\gcc-arm-none-eabi*\bin\arm-none-eabi-gcc.exe",
    ]
    hits = []
    for pattern in patterns:
        hits.extend(glob.glob(pattern))
    return sorted(hits)[-1] if hits else None


def get_toolchain_sysincludes(gcc_path):
    """通过 gcc 自身查询 sysroot 头文件目录，供 clangd 解析 stdio.h 等系统头"""
    if not gcc_path:
        return []

    def run(args):
        try:
            out = subprocess.check_output([gcc_path] + args, stderr=subprocess.DEVNULL)
            return out.decode("utf-8", "ignore").strip()
        except Exception:
            return ""

    dirs = []
    inc = run(["-print-file-name=include"])
    if inc and os.path.isdir(inc):
        dirs.append(inc)

    sysroot = run(["-print-sysroot"])
    if sysroot:
        # 不同发行版 sysroot 布局不同：<sysroot>/include 或 <sysroot>/arm-none-eabi/include
        for candidate in ("include", os.path.join("arm-none-eabi", "include")):
            newlib = os.path.normpath(os.path.join(sysroot, candidate))
            if os.path.isdir(newlib):
                dirs.append(newlib)
                break
    return dirs


def get_keil_include_dirs(project_root):
    """从 Keil 工程(.uvprojx)提取 IncludePath。

    项目里存在 #include "./cjson/cJSON.h" 这类相对包含，依赖 ..\\..\\Middlewares 等
    上级目录，单靠"扫描含头文件的目录"无法覆盖，必须以工程实际配置为准。
    """
    dirs = []
    proj_files = glob.glob(os.path.join(project_root, 'Projects', '**', '*.uvprojx'), recursive=True)
    for proj in proj_files:
        try:
            with open(proj, encoding='utf-8', errors='ignore') as f:
                text = f.read()
        except OSError:
            continue
        base = os.path.dirname(proj)
        for match in re.finditer(r'<IncludePath>(.*?)</IncludePath>', text, re.S):
            for part in match.group(1).strip().split(';'):
                part = part.strip()
                if not part:
                    continue
                path = os.path.normpath(os.path.join(base, part.replace('\\', os.sep)))
                if os.path.isdir(path):
                    dirs.append(os.path.abspath(path))
    return dirs


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


def generate_compile_commands(project_root, compiler="arm-none-eabi-gcc", extra_defines=None, gcc_path=None):
    if extra_defines is None:
        # 注意: 不要定义 __CC_ARM。clangd 使用 clang 前端，定义 __CC_ARM 而不定义
        # __ARMCC_VERSION 会让 CMSIS core_cmFunc.h 走 Keil 分支并触发
        # #error "Please use ARM Compiler Toolchain V4.0.677 or later!"，导致整个文件解析失败。
        extra_defines = [
            "-DGD32F470",
            "-DUSE_HAL_DRIVER",
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

    # 合并 Keil 工程配置的包含路径(含 ..\..\Middlewares 等上级目录)
    keil_dirs = get_keil_include_dirs(project_root)
    include_dirs.update(keil_dirs)
    print(f"Keil include paths: {len(keil_dirs)}")

    include_flags = []
    for inc in sorted(include_dirs):
        rel_inc = os.path.relpath(inc, project_root).replace('\\', '/')
        include_flags.append(f"-I{rel_inc}")

    # 工具链系统头文件目录(stdio.h/newlib 等)：clangd 默认找不到，需显式加入
    sys_include_flags = []
    for sys_inc in get_toolchain_sysincludes(gcc_path):
        sys_include_flags.append("-isystem")
        sys_include_flags.append(sys_inc.replace('\\', '/'))

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

    # clangd(clang 前端)专用兼容参数:
    # 工程的 FreeRTOS 移植层是 Keil(RVDS)版本, 使用 MS 风格内联汇编 __asm{...} 与 __forceinline,
    # clang 需要 -fasm-blocks 才能解析, __forceinline 需映射为 GNU inline。
    clangd_compat_flags = [
        "-fasm-blocks",
        "-D__forceinline=inline",
        # newlib 的 sys/time.h 已提供 struct timeval，lwip 再定义会与之重定义冲突
        "-DLWIP_TIMEVAL_PRIVATE=0",
    ]

    for src in source_files:
        src_path = os.path.relpath(src, project_root).replace('\\', '/')
        work_path = os.path.abspath(project_root).replace('\\', '/')

        command_parts = ([compiler] + extra_compiler_flags + clangd_compat_flags + extra_defines
                         + include_flags + sys_include_flags + ["-c", src_path])
        command_parts = [part.replace('\\', '/') for part in command_parts]
        # 含空格的路径(如 "Program Files (x86)")需要加引号，否则会被拆成多个参数
        command_parts = [f'"{part}"' if ' ' in part else part for part in command_parts]
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
    parser.add_argument("--gcc", default=None, help="arm-none-eabi-gcc 绝对路径(用于提取系统头目录)，默认自动查找")
    args = parser.parse_args()

    root_abs = os.path.abspath(args.root)
    gcc_path = find_arm_gcc(args.gcc)
    if gcc_path:
        print(f"Using toolchain: {gcc_path}")
    else:
        print("WARNING: arm-none-eabi-gcc not found, system headers may be unresolved in clangd")
    generate_compile_commands(root_abs, args.compiler, gcc_path=gcc_path)
