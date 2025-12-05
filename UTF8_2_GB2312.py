#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
安全转换脚本 - 明确转换范围
功能：只转换脚本所在目录及其子目录
"""

import os
import sys
from pathlib import Path

# 脚本所在目录（而不是执行时的当前目录）
BASE_DIR = Path(__file__).parent.resolve()
TARGET_EXTS = ['.c', '.h']

def is_gb2312_file(file_path):
    try:
        with open(file_path, 'rb') as f:
            raw = f.read()
        raw.decode('gb2312')
        return sum(1 for b in raw if b > 0x7F) > 0
    except:
        return False

def is_utf8_file(file_path):
    try:
        with open(file_path, 'rb') as f:
            raw = f.read()
        if raw.startswith(b'\xef\xbb\xbf') or b'\xe4' in raw:
            raw.decode('utf-8')
            return True
        return False
    except:
        return False

def convert_file(src_path):
    try:
        if is_gb2312_file(src_path):
            print(f"跳过(已是GB2312): {src_path.relative_to(BASE_DIR)}")
            return False
            
        if not is_utf8_file(src_path):
            print(f"跳过(非UTF-8): {src_path.relative_to(BASE_DIR)}")
            return False
            
        with open(src_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()
            
        with open(src_path, 'w', encoding='gb2312', errors='ignore') as f:
            f.write(content)
            
        print(f"转换成功: {src_path.relative_to(BASE_DIR)}")
        return True
        
    except Exception as e:
        print(f"转换失败: {src_path.relative_to(BASE_DIR)} - {str(e)}")
        return False

def scan_and_convert():
    print(f"\n{'='*60}")
    print(f"转换范围: {BASE_DIR}")
    print(f"目标类型: {TARGET_EXTS}")
    print(f"{'='*60}\n")
    
    # 先扫描，显示要转换的文件列表
    files_to_convert = []
    for ext in TARGET_EXTS:
        for file_path in BASE_DIR.rglob(f'*{ext}'):
            if is_utf8_file(file_path) and not is_gb2312_file(file_path):
                files_to_convert.append(file_path)
    
    if not files_to_convert:
        print("未找到需要转换的文件")
        return 0, 0
        
    print(f"发现 {len(files_to_convert)} 个需要转换的文件:")
    for f in files_to_convert:
        print(f"  - {f.relative_to(BASE_DIR)}")
    
    try:
        input("\n按Enter开始转换，Ctrl+C取消...")
    except KeyboardInterrupt:
        print("\n已取消")
        sys.exit(0)
    
    # 开始转换
    success = 0
    for file_path in files_to_convert:
        if convert_file(file_path):
            success += 1
            
    return len(files_to_convert), success

if __name__ == '__main__':
    print("\n" + "="*60)
    print("UTF-8转GB2312转换工具")
    print("="*60)
    
    total, success = scan_and_convert()
    
    print("\n" + "="*60)
    print(f"完成！总计: {total} | 成功: {success}")
    print("="*60)
    
    input("\n按Enter键退出...")
