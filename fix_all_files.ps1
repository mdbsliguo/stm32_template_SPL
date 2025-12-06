# 完美解决方案：统一所有文件的末尾换行符和行尾符
# 使用 .NET 方法，支持所有编码

Write-Host "开始处理所有文件..." -ForegroundColor Cyan

# 获取所有 Git 跟踪的文本文件
$files = git ls-files | Where-Object {
    $ext = [System.IO.Path]::GetExtension($_).ToLower()
    $binaryExts = @('.bin', '.hex', '.axf', '.elf', '.o', '.obj', '.map', '.lst', '.crf', '.dep', '.sct', '.lnp', '.htm', '.iex', '.tra', '.omf', '.plg', '.rpt', '.ddk', '.edk', '.mpf', '.mpj', '.db', '.jlink', '.bak', '.tmp', '.swp', '.swo', '.orig', '.rej', '.log', '.png', '.jpg', '.jpeg', '.gif', '.ico', '.dbgconf')
    $textExts = @('.c', '.h', '.cpp', '.hpp', '.s', '.asm', '.md', '.txt', '.json', '.xml', '.yml', '.yaml', '.py', '.sh', '.bat', '.ps1', '.cmake', '.make', '.mk', '.conf', '.config', '.ini', '.cfg', '.gitignore', '.gitattributes')
    
    # 排除二进制文件
    if ($binaryExts -contains $ext) { return $false }
    
    # 排除特定目录
    $excludeDirs = @('Build', 'DebugConfig', 'AI', '.git')
    foreach ($dir in $excludeDirs) {
        if ($_ -like "*\$dir\*") { return $false }
    }
    
    # 只处理文本文件
    return ($textExts -contains $ext) -or ($ext -eq '')
}

$processed = 0
$skipped = 0
$errors = 0

foreach ($file in $files) {
    try {
        if (-not (Test-Path $file)) { 
            $skipped++
            continue 
        }
        
        # 读取文件字节
        $bytes = [System.IO.File]::ReadAllBytes($file)
        if ($bytes.Length -eq 0) { 
            $skipped++
            continue 
        }
        
        # 检测编码（优先 GB2312，因为项目主要使用它）
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
                # 验证：检查是否包含太多控制字符（二进制文件特征）
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
        
        if ($null -eq $content) { 
            $skipped++
            continue 
        }
        
        $needsFix = $false
        $newContent = $content
        
        # 1. 统一行尾符为 LF（移除所有 CRLF）
        if ($content -match "`r`n") {
            $newContent = $newContent -replace "`r`n", "`n"
            $needsFix = $true
        }
        # 移除单独的 CR
        if ($newContent -match "`r") {
            $newContent = $newContent -replace "`r", "`n"
            $needsFix = $true
        }
        
        # 2. 确保文件末尾有且仅有一个换行符
        $trimmed = $newContent.TrimEnd("`n", "`r")
        if ($newContent -ne ($trimmed + "`n")) {
            $newContent = $trimmed + "`n"
            $needsFix = $true
        }
        
        if ($needsFix) {
            $newBytes = $usedEncoding.GetBytes($newContent)
            [System.IO.File]::WriteAllBytes($file, $newBytes)
            Write-Host "已修复: $file" -ForegroundColor Green
            $processed++
        }
    } catch {
        Write-Host "错误: $file - $($_.Exception.Message)" -ForegroundColor Red
        $errors++
    }
}

Write-Host "`n处理完成！" -ForegroundColor Cyan
Write-Host "修复: $processed 个文件" -ForegroundColor Green
Write-Host "跳过: $skipped 个文件" -ForegroundColor Yellow
Write-Host "错误: $errors 个文件" -ForegroundColor $(if ($errors -gt 0) { "Red" } else { "Green" })

