param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot '../MechanicsHomework.uproject'
$build = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
if (!(Test-Path -LiteralPath $build)) { throw "Unreal Engine 5.8 not found at $EngineRoot. Pass -EngineRoot with its installation directory." }
& $build MechanicsHomeworkEditor Win64 Development "-Project=$([IO.Path]::GetFullPath($project))" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed ($LASTEXITCODE)." }
