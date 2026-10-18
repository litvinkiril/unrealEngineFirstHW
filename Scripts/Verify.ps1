param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$Render
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'Build.ps1') -EngineRoot $EngineRoot
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$report = Join-Path $root ('Saved/Verification/' + [Guid]::NewGuid().ToString('N'))
$arguments = @(
    (Join-Path $root 'MechanicsHomework.uproject'),
    '-unattended', '-nop4', '-nosound', '-nosplash',
    '-ExecCmds=Automation RunTests Homework.Gameplay',
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$report", '-stdout', '-UTF8Output'
)
if ($Render) { $arguments += @('-RenderOffscreen', '-HomeworkScreenshots', '-ResX=1280', '-ResY=720') }
else { $arguments += '-NullRHI' }
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') @arguments
if ($LASTEXITCODE -ne 0) { throw "Unreal exited with code $LASTEXITCODE." }
# Unreal's process can exit 0 even when an automation assertion fails: inspect the report.
$result = Get-Content -LiteralPath (Join-Path $report 'index.json') -Raw | ConvertFrom-Json
if ($result.failed -gt 0 -or ($result.succeeded + $result.succeededWithWarnings) -lt 1) {
    throw "Gameplay verification failed. See $report/index.json"
}
Write-Host "PASS: Homework.Gameplay.EndToEnd. Report: $report/index.json"
