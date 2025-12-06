# 为所有文本文件添加末尾换行符
# 排除二进制文件和 Git 忽略的文件

$files = Get-ChildItem -Recurse -File | Where-Object {
    $ext = $_.Extension.ToLower()
    $name = $_.Name.ToLower()
    
    # 排除二进制文件
    $binaryExts = @('.bin', '.hex', '.axf', '.elf', '.o', '.obj', '.map', '.lst', '.crf', '.dep', '.sct', '.lnp', '.htm', '.iex', '.tra', '.omf', '.plg', '.rpt', '.ddk', '.edk', '.mpf', '.mpj', '.db', '.jlink', '.bak', '.tmp', '.swp', '.swo', '.orig', '.rej', '.log', '.png', '.jpg', '.jpeg', '.gif', '.ico', '.dbgconf')
    
    # 排除特定目录
    $excludeDirs = @('Build', 'DebugConfig', 'AI', '.git', 'node_modules')
    
    $isExcluded = $false
    foreach ($dir in $excludeDirs) {
        if ($_.FullName -like "*\$dir\*") {
            $isExcluded = $true
            break
        }
    }
    
    if ($isExcluded) { return $false }
    if ($binaryExts -contains $ext) { return $false }
    
    # 只处理文本文件（常见源代码和文档）
    $textExts = @('.c', '.h', '.cpp', '.hpp', '.s', '.asm', '.md', '.txt', '.json', '.xml', '.yml', '.yaml', '.py', '.sh', '.bat', '.ps1', '.cmake', '.make', '.mk', '.conf', '.config', '.ini', '.cfg', '.gitignore', '.gitattributes')
    
    return ($textExts -contains $ext) -or ($ext -eq '')
}

function Get-FileEncoding {
    param([string]$FilePath)
    
    $bytes = [System.IO.File]::ReadAllBytes($FilePath)
    if ($bytes.Length -eq 0) { return $null }
    
    # 尝试检测编码
    $encodings = @(
        [System.Text.Encoding]::UTF8,
        [System.Text.Encoding]::GetEncoding("GB2312"),
        [System.Text.Encoding]::Default
    )
    
    foreach ($encoding in $encodings) {
        try {
            $decoded = $encoding.GetString($bytes)
            # 简单验证：如果解码后没有太多乱码，可能是正确的编码
            if ($decoded -match '[\x00-\x08\x0B-\x0C\x0E-\x1F]' -and $decoded.Length -lt $bytes.Length * 0.9) {
                continue
            }
            return $encoding
        } catch {
            continue
        }
    }
    
    # 默认返回 GB2312（因为项目主要使用 GB2312）
    return [System.Text.Encoding]::GetEncoding("GB2312")
}

$count = 0
foreach ($file in $files) {
    try {
        # 检测文件编码
        $encoding = Get-FileEncoding -FilePath $file.FullName
        if ($null -eq $encoding) { continue }
        
        # 读取文件内容
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        if ($bytes.Length -eq 0) { continue }
        
        $content = $encoding.GetString($bytes)
        
        # 检查文件末尾是否有换行符
        if ($content.Length -gt 0 -and $content[-1] -ne "`n" -and $content[-1] -ne "`r") {
            # 添加换行符
            $content += "`n"
            $newBytes = $encoding.GetBytes($content)
            [System.IO.File]::WriteAllBytes($file.FullName, $newBytes)
            Write-Host "已处理: $($file.FullName.Replace((Get-Location).Path + '\', ''))" -ForegroundColor Green
            $count++
        }
    } catch {
        # 跳过无法处理的文件
        continue
    }
}

Write-Host "`n总共处理了 $count 个文件" -ForegroundColor Cyan
