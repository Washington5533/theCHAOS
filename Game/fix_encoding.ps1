$files = @(
    'c:\Users\wst\Desktop\Game\Game\Pendulum.h',
    'c:\Users\wst\Desktop\Game\Game\Pendulum.cpp',
    'c:\Users\wst\Desktop\Game\Game\Player.cpp',
    'c:\Users\wst\Desktop\Game\Game\Player.h',
    'c:\Users\wst\Desktop\Game\Game\Game.cpp',
    'c:\Users\wst\Desktop\Game\Game\Game.h'
)
$utf8Bom = New-Object System.Text.UTF8Encoding $true
foreach ($f in $files) {
    if (Test-Path $f) {
        $content = [System.IO.File]::ReadAllText($f, [System.Text.Encoding]::UTF8)
        [System.IO.File]::WriteAllText($f, $content, $utf8Bom)
        Write-Host "Converted: $f"
    }
}
