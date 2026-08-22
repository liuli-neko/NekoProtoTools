param(
    [string]$Label = "local",
    [ValidateRange(1, 20)]
    [int]$Repeats = 1,
    [switch]$Trace,
    [string]$Compiler = "C:\msys64\clang64\bin\clang++.exe",
    [string]$RapidJsonInclude = "",
    [string]$ProjectRoot = "",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
} else {
    $root = (Resolve-Path -LiteralPath $ProjectRoot).Path
}
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $outputRoot = Join-Path $root "build\compile_time\$Label"
} else {
    $outputRoot = $OutputRoot
}
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

if (-not (Test-Path -LiteralPath $Compiler)) {
    throw "Clang compiler not found: $Compiler"
}

if ([string]::IsNullOrWhiteSpace($RapidJsonInclude)) {
    $packageRoot = Join-Path $env:LOCALAPPDATA ".xmake\packages\r\rapidjson"
    if (Test-Path -LiteralPath $packageRoot) {
        $header = Get-ChildItem -LiteralPath $packageRoot -Recurse -Filter rapidjson.h |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($null -ne $header) {
            $RapidJsonInclude = Split-Path (Split-Path $header.FullName -Parent) -Parent
        }
    }
}

$common = @("-std=c++23", "-O0", "-c", "-I$($root)\include")
if ($Trace) {
    $common += "-ftime-trace"
}

$cases = @(
    @{ Name = "include_reflection"; Source = "include_reflection.cpp"; Fields = 0; Tags = 0 },
    @{ Name = "foreach_4"; Source = "foreach.cpp"; Fields = 4; Tags = 0 },
    @{ Name = "foreach_16"; Source = "foreach.cpp"; Fields = 16; Tags = 0 },
    @{ Name = "foreach_32"; Source = "foreach.cpp"; Fields = 32; Tags = 0 },
    @{ Name = "foreach_64"; Source = "foreach.cpp"; Fields = 64; Tags = 0 },
    @{ Name = "foreach_64_sparse_tags"; Source = "foreach.cpp"; Fields = 64; Tags = 1 },
    @{ Name = "foreach_64_dense_tags"; Source = "foreach.cpp"; Fields = 64; Tags = 2 },
    @{ Name = "binary_write"; Source = "binary_write.cpp"; Fields = 64; Tags = 2 },
    @{ Name = "binary_read"; Source = "binary_read.cpp"; Fields = 64; Tags = 2 },
    @{ Name = "schema"; Source = "schema.cpp"; Fields = 64; Tags = 2 },
    @{ Name = "enum_heavy"; Source = "enum_heavy.cpp"; Fields = 0; Tags = 0 }
)

if (-not [string]::IsNullOrWhiteSpace($RapidJsonInclude)) {
    $cases += @{ Name = "json_write"; Source = "json_write.cpp"; Fields = 64; Tags = 2; Json = $true }
    $cases += @{ Name = "json_read"; Source = "json_read.cpp"; Fields = 64; Tags = 2; Json = $true }
} else {
    Write-Warning "RapidJSON was not found; JSON workloads will be skipped. Pass -RapidJsonInclude to enable them."
}

$rows = @()
foreach ($case in $cases) {
    $durations = @()
    for ($repeat = 1; $repeat -le $Repeats; ++$repeat) {
        $object = Join-Path $outputRoot "$($case.Name)_$repeat.o"
        $arguments = @($common)
        if ($case.Fields -gt 0) {
            $arguments += "-DNEKO_BENCH_FIELD_COUNT=$($case.Fields)"
            $arguments += "-DNEKO_BENCH_TAG_DENSITY=$($case.Tags)"
        }
        if ($case.Json) {
            $arguments += "-DNEKO_PROTO_ENABLE_RAPIDJSON"
            $arguments += "-I$RapidJsonInclude"
        }
        $arguments += (Join-Path $PSScriptRoot $case.Source)
        $arguments += @("-o", $object)

        $watch = [System.Diagnostics.Stopwatch]::StartNew()
        & $Compiler @arguments
        $exitCode = $LASTEXITCODE
        $watch.Stop()
        if ($exitCode -ne 0) {
            throw "Compilation failed for $($case.Name) (exit $exitCode)"
        }
        $durations += $watch.Elapsed.TotalMilliseconds
    }

    $stats = $durations | Measure-Object -Average -Minimum -Maximum
    $rows += [pscustomobject]@{
        case = $case.Name
        fields = $case.Fields
        tag_density = $case.Tags
        repeats = $Repeats
        average_ms = [math]::Round($stats.Average, 2)
        minimum_ms = [math]::Round($stats.Minimum, 2)
        maximum_ms = [math]::Round($stats.Maximum, 2)
    }
    Write-Host ("{0,-28} {1,10:N2} ms" -f $case.Name, $stats.Average)
}

$csv = Join-Path $outputRoot "timings.csv"
$rows | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8
Write-Host "Wrote $csv"
