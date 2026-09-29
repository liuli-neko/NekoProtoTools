param(
    [Parameter(Mandatory = $true)]
    [string]$TraceDirectory
)

$ErrorActionPreference = "Stop"
$directory = (Resolve-Path -LiteralPath $TraceDirectory).Path
$eventNames = @(
    "InstantiateFunction",
    "InstantiateClass",
    "PerformPendingInstantiations",
    "ParseClass",
    "EvaluateAsConstantExpr"
)
$needles = @(
    "serializerGetMemberReference",
    "nekoGetMemberReference",
    "ReflectProvider",
    "ReflectModel",
    "forEach",
    "forEachMeta",
    "tag_list_has_impl",
    "tag_list_get_impl",
    "tag_get",
    "tag_has",
    "neko_get_enum_name"
)

$summary = @()
$hotspots = @()
foreach ($file in Get-ChildItem -LiteralPath $directory -Filter "*.json") {
    $trace = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    foreach ($eventName in $eventNames) {
        $matching = @($trace.traceEvents | Where-Object { $_.name -eq $eventName })
        $summary += [pscustomobject]@{
            trace = $file.BaseName
            event = $eventName
            count = $matching.Count
            total_ms = [math]::Round((($matching | Measure-Object -Property dur -Sum).Sum / 1000), 3)
        }
    }
    foreach ($event in $trace.traceEvents) {
        if ($null -eq $event.args -or $null -eq $event.args.detail) {
            continue
        }
        $detail = [string]$event.args.detail
        foreach ($needle in $needles) {
            if ($detail.Contains($needle)) {
                $hotspots += [pscustomobject]@{
                    trace = $file.BaseName
                    needle = $needle
                    event = $event.name
                    duration_ms = [math]::Round(([double]$event.dur / 1000), 3)
                    detail = $detail
                }
                break
            }
        }
    }
}

$summaryPath = Join-Path $directory "trace_summary.csv"
$hotspotPath = Join-Path $directory "trace_hotspots.csv"
$summary | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$hotspots | Sort-Object duration_ms -Descending | Export-Csv -LiteralPath $hotspotPath -NoTypeInformation -Encoding UTF8
Write-Host "Wrote $summaryPath"
Write-Host "Wrote $hotspotPath"
