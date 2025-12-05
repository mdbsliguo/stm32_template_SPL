# 使用 .NET 方法处理文件，支持所有编码
$files = git ls-files | Where-Object {
    $ext = [System.IO.Path]::GetExtension($_).ToLower()
    $textExts = @('.c', '.h', '.cpp', '.hpp', '.s', '.asm', '.md', '.txt', '.json', '.xml', '.yml', '.yaml', '.py', '.sh', '.bat', '.ps1', '.cmake', '.make', '.mk', '.conf', '.config', '.ini', '.cfg', '.gitignore', '.gitattributes')
    return ($textExts -contains $ext) -or ($ext -eq '')
}

$count = 0
foreach ($file in $files) {
    try {
        if (-not (Test-Path $file)) { continue }
        
        # 读取文件字节
        $bytes = [System.IO.File]::ReadAllBytes($file)
        if ($bytes.Length -eq 0) { continue }
        
        # 尝试检测编码（优先 GB2312，因为项目主要使用它）
        $encodings = @(
            [System.Text.Encoding]::GetEncoding("GB2312"),
            [System.Text.Encoding]::UTF8,
            [System.Text.Encoding]::Default
        )
        
        $content = $null
        $usedEncoding = $null
        
        foreach ($enc in $encodings) {
            try {
                $testContent = $enc.GetString($bytes)
                # 简单验证：检查是否包含太多控制字符（二进制文件特征）
                $controlChars = ([regex]::Matches($testContent, '[\x00-\x08\x0B-\x0C\x0E-\x1F]')).Count
                if ($controlChars -lt $testContent.Length * 0.01) {
                    $content = $testContent
                    $usedEncoding = $enc
                    break
                }
            } catch {
                continue
            }
        }
        
        if ($null -eq $content) { continue }
        
        # 检查文件末尾是否有换行符
        if ($content.Length -gt 0 -and $content[-1] -ne "`n" -and $content[-1] -ne "`r") {
            # 添加换行符
            $content += "`n"
            $newBytes = $usedEncoding.GetBytes($content)
            [System.IO.File]::WriteAllBytes($file, $newBytes)
            Write-Host "已处理: $file" -ForegroundColor Green
            $count++
        }
    } catch {
        # 跳过无法处理的文件
        continue
    }
}

Write-Host "`n总共处理了 $count 个文件" -ForegroundColor Cyan

