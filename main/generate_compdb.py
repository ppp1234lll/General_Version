# -*- coding: utf-8 -*-
import os
import json
import argparse

def generate_compile_commands(project_root, compiler="arm-none-eabi-gcc", extra_defines=None):
    if extra_defines is None:
        # 默认添加一些常用的宏定义，可以根据需要修改
        extra_defines = ["-DGD32F470", "-DUSE_HAL_DRIVER"]

    include_dirs = set()
    source_files = []

    print(f"Scanning directory: {project_root}")

    # 第一遍扫描：找到所有包含头文件的目录，以及所有的源文件
    for root, dirs, files in os.walk(project_root):
        # 跳过一些常见的输出或隐藏目录
        if any(skip in root for skip in ['.git', 'Output', 'Debug', 'Release', 'settings']):
            continue
            
        has_header = False
        for file in files:
            ext = os.path.splitext(file)[1].lower()
            if ext in ['.h', '.hpp', '.inc']:
                has_header = True
            elif ext in ['.c', '.cpp', '.cc', '.cxx', '.s', '.S']:
                # 记录源文件的绝对路径
                source_files.append(os.path.abspath(os.path.join(root, file)))
        
        if has_header:
            # 记录包含头文件的绝对路径
            include_dirs.add(os.path.abspath(root))

    # 构建 -I 参数，同样使用相对路径
    include_flags = []
    for inc in include_dirs:
        rel_inc = os.path.relpath(inc, project_root).replace('\\', '/')
        include_flags.append(f"-I{rel_inc}")
    
    compile_commands = []
    
    for src in source_files:
        # 强制将中文路径可能出现的转义问题直接使用原始路径表示，并转换反斜杠
        # 不使用 abspath，使用相对当前扫描目录的路径
        src_path = os.path.relpath(src, project_root).replace('\\', '/')
        
        # 将 directory 设置为当前工作目录，这里对于 clangd 非常重要
        # clangd 会将相对路径结合 directory 拼成绝对路径
        work_path = os.path.abspath(project_root).replace('\\', '/')

        # 构建编译命令，针对相对路径
        # 在嵌入式开发中，通常需要加上一些编译标志，例如 -mthumb -mcpu=cortex-m4 
        # 为了更精准，这里我加入最基础的 cortex-m4 支持，防止头文件中的汇编指令报错
        extra_compiler_flags = ["-mthumb", "-mcpu=cortex-m4"]
        command_parts = [compiler] + extra_compiler_flags + extra_defines + include_flags + ["-c", src_path]
        command_parts = [part.replace('\\', '/') for part in command_parts]
        command = " ".join(command_parts)
        
        # 修正中文路径在 Python json dump 中由于编码导致被 clangd 解析为乱码的问题
        compile_commands.append({
            "directory": work_path,
            "command": command,
            "file": src_path
        })

    # 输出到 compile_commands.json
    output_path = os.path.join(project_root, "compile_commands.json")
    with open(output_path, 'w', encoding='utf-8') as f:
        # 必须使用 ensure_ascii=False，否则中文路径会被转义为 \uXXXX，导致 clangd 无法识别路径
        json.dump(compile_commands, f, indent=2, ensure_ascii=False)
    
    print(f"Successfully generated {output_path}")
    print(f"Found {len(source_files)} source files and {len(include_dirs)} include directories.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate compile_commands.json for clangd by scanning directory.")
    parser.add_argument("--root", default=".", help="Project root directory (default: current directory)")
    parser.add_argument("--compiler", default="arm-none-eabi-gcc", help="Compiler executable name (default: arm-none-eabi-gcc)")
    args = parser.parse_args()
    
    # 转换为绝对路径
    root_abs = os.path.abspath(args.root)
    generate_compile_commands(root_abs, args.compiler)