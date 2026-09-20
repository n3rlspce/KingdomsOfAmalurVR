param([Parameter(Mandatory)][string]$InputIni, [Parameter(Mandatory)][string]$OutputIni)
$ErrorActionPreference = 'Stop'
if ([IO.Path]::GetFullPath($InputIni) -eq [IO.Path]::GetFullPath($OutputIni)) {
    throw 'Stage a separate output file; do not rewrite a live configuration.'
}
$ini = Get-Content -LiteralPath $InputIni -Raw
$ini = $ini -replace '(?mi)^(reload_config|reload_fixes)\s*=.*$', '$1 = CTRL SHIFT VK_F11'
# Also move the donor SBS output-mode binding, which otherwise changes the view.
$ini = $ini -replace '(?mi)^(key\s*=\s*)no_modifiers\s+(VK_)?F11\s*$', '${1}CTRL ALT VK_F11'
[IO.File]::WriteAllText([IO.Path]::GetFullPath($OutputIni), $ini)
Write-Host 'Staged F11 reservation: shader reload Ctrl+Shift+F11; output mode Ctrl+Alt+F11.'
