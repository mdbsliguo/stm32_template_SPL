#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
工具脚本：写入UTF-8编码的Markdown文件

功能：
- 明确以UTF-8编码写入.md文件
- 避免Windows系统默认编码（GB2312/GBK）导致的表情字符乱码问题

使用方法：
    python tools/write_md_utf8.py <文件路径> <内容>
    
或者作为模块导入：
    from tools.write_md_utf8 import write_md_utf8
    write_md_utf8('path/to/file.md', content)
"""

import sys
import os
from pathlib import Path


def write_md_utf8(file_path, content):
    """
    以UTF-8编码写入Markdown文件
    
    Args:
        file_path: 文件路径（字符串或Path对象）
        content: 文件内容（字符串）
    
    Returns:
        bool: 成功返回True，失败返回False
    """
    try:
        file_path = Path(file_path)
        # 确保目录存在
        file_path.parent.mkdir(parents=True, exist_ok=True)
        
        # 明确以UTF-8编码写入，不使用BOM
        with open(file_path, 'w', encoding='utf-8', newline='') as f:
            f.write(content)
        
        # 验证写入是否成功
        with open(file_path, 'r', encoding='utf-8') as f:
            read_content = f.read()
            if read_content == content:
                print(f"? 成功写入UTF-8编码文件: {file_path}")
                return True
            else:
                print(f"? 验证失败: 写入内容与读取内容不一致")
                return False
                
    except Exception as e:
        print(f"? 写入失败: {file_path} - {str(e)}")
        return False


def main():
    """命令行入口"""
    if len(sys.argv) < 3:
        print("用法: python write_md_utf8.py <文件路径> <内容>")
        print("或者: python write_md_utf8.py <文件路径> < <输入文件>")
        sys.exit(1)
    
    file_path = sys.argv[1]
    
    # 如果内容来自标准输入
    if len(sys.argv) == 2:
        content = sys.stdin.read()
    else:
        # 从命令行参数获取内容（支持多行）
        content = '\n'.join(sys.argv[2:])
    
    success = write_md_utf8(file_path, content)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()










