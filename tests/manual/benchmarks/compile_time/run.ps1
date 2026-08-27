param(
    [string]$Label = "local",
    [ValidateRange(1, 20)]
    [int]$Repeats = 3,
    [switch]$Trace,
    [string]$Compiler = "clang++",
    [string]$RapidJsonInclude = "",
    [string]$ReflectCppInclude = "",
    [string]$FmtInclude = "",
    [string]$ProjectRoot = "",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
} else {
    $root = (Resolve-Path -LiteralPath $ProjectRoot).Path
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $outputRoot = Join-Path $root "build\compile_time\$Label"
} else {
    $outputRoot = $OutputRoot
}
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

$pkgBase = Join-Path $env:LOCALAPPDATA ".xmake\packages"
if (-not (Test-Path -LiteralPath $pkgBase) -and $env:USERPROFILE) {
    $pkgBase = Join-Path $env:USERPROFILE ".xmake\packages"
}

if ([string]::IsNullOrWhiteSpace($RapidJsonInclude) -and (Test-Path -LiteralPath $pkgBase)) {
    $header = Get-ChildItem -LiteralPath $pkgBase -Recurse -Filter rapidjson.h -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($null -ne $header) {
        $RapidJsonInclude = Split-Path (Split-Path $header.FullName -Parent) -Parent
    }
}

if ([string]::IsNullOrWhiteSpace($FmtInclude) -and (Test-Path -LiteralPath $pkgBase)) {
    $header = Get-ChildItem -LiteralPath $pkgBase -Recurse -Filter format.h -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($null -ne $header) {
        $FmtInclude = Split-Path (Split-Path $header.FullName -Parent) -Parent
    }
}

if ([string]::IsNullOrWhiteSpace($ReflectCppInclude) -and (Test-Path -LiteralPath $pkgBase)) {
    $header = Get-ChildItem -LiteralPath $pkgBase -Recurse -Filter rfl.hpp -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending | Select-Object -First 1
    if ($null -ne $header) {
        $ReflectCppInclude = Split-Path $header.FullName -Parent
    }
}

$commonNeko = @("-std=c++23", "-O0", "-c", "-I$($root)\include", "-I$PSScriptRoot")
if (-not [string]::IsNullOrWhiteSpace($FmtInclude)) {
    $commonNeko += "-I$FmtInclude"
}
$commonRfl = @("-std=c++20", "-O0", "-c", "-I$PSScriptRoot")
if (-not [string]::IsNullOrWhiteSpace($ReflectCppInclude)) {
    $commonRfl += "-I$ReflectCppInclude"
}
if ($Trace) {
    $commonNeko += "-ftime-trace"
    $commonRfl  += "-ftime-trace"
}

$cases = @(
    @{ Name = "include_cost"; Source = "include_reflection.cpp"; Fields = 0; RflSource = "rfl_include.cpp" },
    @{ Name = "json_write_4"; Source = "json_write.cpp"; Fields = 4; Json = $true; RflSource = "rfl_json_write.cpp" },
    @{ Name = "json_read_4"; Source = "json_read.cpp"; Fields = 4; Json = $true; RflSource = "rfl_json_read.cpp" },
    @{ Name = "json_write_16"; Source = "json_write.cpp"; Fields = 16; Json = $true; RflSource = "rfl_json_write.cpp" },
    @{ Name = "json_read_16"; Source = "json_read.cpp"; Fields = 16; Json = $true; RflSource = "rfl_json_read.cpp" },
    @{ Name = "json_write_32"; Source = "json_write.cpp"; Fields = 32; Json = $true; RflSource = "rfl_json_write.cpp" },
    @{ Name = "json_read_32"; Source = "json_read.cpp"; Fields = 32; Json = $true; RflSource = "rfl_json_read.cpp" },
    @{ Name = "json_write_64"; Source = "json_write.cpp"; Fields = 64; Json = $true; RflSource = "rfl_json_write.cpp" },
    @{ Name = "json_read_64"; Source = "json_read.cpp"; Fields = 64; Json = $true; RflSource = "rfl_json_read.cpp" },
    @{ Name = "binary_write_64"; Source = "binary_write.cpp"; Fields = 64 },
    @{ Name = "binary_read_64"; Source = "binary_read.cpp"; Fields = 64 },
    @{ Name = "foreach_4"; Source = "foreach.cpp"; Fields = 4 },
    @{ Name = "foreach_16"; Source = "foreach.cpp"; Fields = 16 },
    @{ Name = "foreach_32"; Source = "foreach.cpp"; Fields = 32 },
    @{ Name = "foreach_64"; Source = "foreach.cpp"; Fields = 64 },
    @{ Name = "schema_64"; Source = "schema.cpp"; Fields = 64 },
    @{ Name = "enum_heavy"; Source = "enum_heavy.cpp"; Fields = 0 }
)

Write-Host "==========================================================================" -ForegroundColor Cyan
Write-Host "                NekoProto Compile-Time Benchmark Runner                   " -ForegroundColor Cyan
Write-Host "==========================================================================" -ForegroundColor Cyan
Write-Host ("{0,-22} | {1,12} | {2,12} | {3,16}" -f "Workload", "Neko (ms)", "rfl-cpp (ms)", "Speedup")
Write-Host "-----------------------+--------------+--------------+--------------------"

$rows = @()
foreach ($case in $cases) {
    # 1. Compile Neko
    $nekoDurations = @()
    for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
        $object = Join-Path $outputRoot "$($case.Name)_neko_$repeat.o"
        $arguments = @($commonNeko)
        if ($case.Fields -gt 0) {
            $arguments += "-DNEKO_BENCH_FIELD_COUNT=$($case.Fields)"
            $arguments += "-DNEKO_BENCH_TAG_DENSITY=0"
        }
        if ($case.Json) {
            $arguments += "-DNEKO_PROTO_ENABLE_RAPIDJSON"
            if (-not [string]::IsNullOrWhiteSpace($RapidJsonInclude)) {
                $arguments += "-I$RapidJsonInclude"
            }
        }
        $arguments += (Join-Path $PSScriptRoot $case.Source)
        $arguments += @("-o", $object)

        $watch = [System.Diagnostics.Stopwatch]::StartNew()
        & $Compiler @arguments
        $exitCode = $LASTEXITCODE
        $watch.Stop()
        if ($exitCode -ne 0) {
            throw "Compilation failed for $($case.Name) (Neko, exit $exitCode)"
        }
        $nekoDurations += $watch.Elapsed.TotalMilliseconds
    }
    $nekoAvg = ($nekoDurations | Measure-Object -Average).Average

    # 2. Compile rfl if available
    $rflAvg = $null
    $speedupStr = "N/A"
    if (-not [string]::IsNullOrWhiteSpace($case.RflSource) -and -not [string]::IsNullOrWhiteSpace($ReflectCppInclude)) {
        $rflDurations = @()
        for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
            $object = Join-Path $outputRoot "$($case.Name)_rfl_$repeat.o"
            $arguments = @($commonRfl)
            if ($case.Fields -gt 0) {
                $arguments += "-DNEKO_BENCH_FIELD_COUNT=$($case.Fields)"
            }
            $arguments += (Join-Path $PSScriptRoot $case.RflSource)
            $arguments += @("-o", $object)

            $watch = [System.Diagnostics.Stopwatch]::StartNew()
            & $Compiler @arguments
            $exitCode = $LASTEXITCODE
            $watch.Stop()
            if ($exitCode -ne 0) {
                throw "Compilation failed for $($case.Name) (rfl, exit $exitCode)"
            }
            $rflDurations += $watch.Elapsed.TotalMilliseconds
        }
        $rflAvg = ($rflDurations | Measure-Object -Average).Average
        $diffPct = (($rflAvg - $nekoAvg) / $rflAvg) * 100.0
        $ratio = $rflAvg / $nekoAvg
        $speedupStr = ("{0:+0.0;-0.0}% ({1:N2}x)" -f $diffPct, $ratio)
    }

    $rflDisplay = if ($null -ne $rflAvg) { "{0,9:N1} ms" -f $rflAvg } else { "N/A" }
    Write-Host ("{0,-22} | {1,9:N1} ms | {2,12} | {3,16}" -f $case.Name, $nekoAvg, $rflDisplay, $speedupStr)

    $rows += [pscustomobject]@{
        workload = $case.Name
        fields = $case.Fields
        repeats = $Repeats
        neko_avg_ms = [math]::Round($nekoAvg, 2)
        rfl_avg_ms = if ($null -ne $rflAvg) { [math]::Round($rflAvg, 2) } else { "" }
        speedup = $speedupStr
    }
}

$csv = Join-Path $outputRoot "timings.csv"
$rows | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8
Write-Host "`nWrote results to $csv" -ForegroundColor Green
