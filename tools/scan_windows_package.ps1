param(
    [Parameter(Mandatory = $true)][string]$PackagePath,
    [Parameter(Mandatory = $true)][string]$ReportPath
)
$ErrorActionPreference = 'Stop'
$target = (Resolve-Path -LiteralPath $PackagePath).Path
$status = Get-MpComputerStatus
if (-not $status.AMServiceEnabled -or -not $status.AntivirusEnabled) {
    throw 'Microsoft Defender must be available for the Windows package scan.'
}
$platformRoot = Join-Path $env:ProgramData 'Microsoft/Windows Defender/Platform'
$scanner = Get-ChildItem -LiteralPath $platformRoot -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'MpCmdRun.exe' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $scanner) { throw 'Cannot find Microsoft Defender MpCmdRun.exe.' }
$output = & $scanner -Scan -ScanType 3 -File $target -DisableRemediation 2>&1
$scanExitCode = $LASTEXITCODE
$report = @(
    "scanner=Microsoft Defender"
    "definitions=$($status.AntivirusSignatureVersion)"
    "scanned_utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))"
    "target=$(Split-Path -Leaf $target)"
    "exit_code=$scanExitCode"
) + @($output | ForEach-Object { $_.ToString() })
$report | Set-Content -LiteralPath $ReportPath -Encoding utf8
$output | ForEach-Object { Write-Output $_ }
if ($scanExitCode -ne 0) { throw "Defender package scan failed (exit $scanExitCode). See $ReportPath." }
