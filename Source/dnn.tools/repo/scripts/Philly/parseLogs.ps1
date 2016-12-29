<#
.SYNOPSIS
Parses CNTK log files.
.DESCRIPTION
Parses CNTK log files from a Philly job to extract training/validation error for each epoch.
.EXAMPLE
parseLogs.ps1 -LogDir "\\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\sys\jobs\application_1475776607997_0191\logs" -Mode "Training"
.EXAMPLE
parseLogs.ps1 -LogDir "\\storage.rr1.philly.selfhost.corp.microsoft.com\anlgvc_scratch\sys\jobs\application_1475776607997_0191\logs" -Mode "Validate" -Metrics Err -Separator `t
.PARAMETER LogDir
Path to Philly job's logs directory.
.PARAMETER OutFile
Output .csv file name.
.PARAMETER Mode
Parse data either from Training or Validate mode.
.PARAMETER Metrics
Names of metrics that we want to extract, default "CE", "Err", "miouError", "pixelwiseError".
It is OK to list metrics that are not present in log files (those will be ignored).
Ordering of columns in output table will be consistent with the ordering in this list.
Typical meanings of default metrics (of course, these can be changed in config files for each training):
    - CE: standard cross-entropy loss value, normalized by the number of valid (not ignored) pixels.
    - Err: Fraction of incorrectly classified images. Relevant only for image classification problems.
    - miouError: 1 - mean intersection-over-union (IoU) score. Relevant only for pixelwise classification problems.
        Mean IoU score is defined as the average IoU score over classes. IoU score for a class is the ratio of sizes
        of intersection and union of pixels labeled by that class in classifier outputs and ground truth labels
        (both intersection and union are accumulated over all images).
    - pixelwiseError: Fraction of incorrectly classified pixels (of any class). Relevant only for pixelwise
        classification problems.
.PARAMETER PercentMetrics
Names of metrics that should be converted to percentages, default "Err", "miouError", "pixelwiseError".
It is OK to list metrics that are not present in log files (those will be ignored).
.PARAMETER Separator
Separator character to use in .csv file, default ",".
#>
[CmdletBinding()]
Param(
    [Parameter(Mandatory = $True, Position = 1)]
    [ValidateNotNullOrEmpty()]
    [string]$LogDir,

    [Parameter(Mandatory = $False, Position = 2)]
    [ValidateNotNullOrEmpty()]
    [string]$OutFile = ".\" + $Mode + "Accuracy.csv",

    [Parameter(Mandatory = $True, Position = 3)]
    [ValidateSet("Training", "Validate")]
    [string]$Mode,

    [Parameter(Mandatory = $False, Position = 4)]
    [ValidateNotNullOrEmpty()]
    [string[]]$Metrics = @("CE", "Err", "miouError", "pixelwiseError"),

    [Parameter(Mandatory = $False, Position = 5)]
    [ValidateNotNullOrEmpty()]
    [string[]]$PercentMetrics = @("Err", "miouError", "pixelwiseError"),

    [Parameter(Mandatory = $False, Position = 6)]
    [ValidateNotNullOrEmpty()]
    [string]$Separator = ","
)

# Stop on any error.
$ErrorActionPreference = 'Stop'

# Include common functions.
. (Join-Path -Path "$PSScriptRoot" -ChildPath "..\common\common.ps1")

# Find all files with the extension logrank0 in the specified directory and its subdirectories.
$logFiles = Get-ChildItem -Recurse -Path $LogDir -Filter *.logrank0

# Map from tasks to arrays of lines containing metric values for those tasks (in all log files).
$taskLines = @{}

foreach ($logFile in $logFiles)
{
    # Extract lines that contain metric values or task titles.
    $lines = ($logFile | Select-String -pattern "(Finished Epoch..*$Mode|# .* command \(train action\))") | Sort-Object LineNumber
    $task = $null
    $startIndex = -1
    for ($lineIndex = 0; $lineIndex -lt $lines.Length; ++$lineIndex)
    {
        if ($lines[$lineIndex] -match "# (.*) command \(train action\)")
        {
            # Transfer previous task's lines.
            if (($task -ne $null) -and ($lineIndex -gt $startIndex))
            {
                $taskLines[$task] += $lines[$startIndex..($lineIndex - 1)]
            }

            # Name of the new task we're tracking now.
            $task = $Matches[1]
            $startIndex = $lineIndex + 1
        }
    }
    if ($startIndex -lt $lines.Length)
    {
        # Transfer remaining.
        Check ($task -ne $null) "Could not find any train tasks"
        $taskLines[$task] += $lines[$startIndex..($lines.Length - 1)]
    }
}

$tasks = [string[]]($taskLines.Keys | % { $_.ToString() })

# Create new output file (overwrite if it exists).
New-Item -ItemType file -Path $OutFile -Force| Out-Null

# This will be the first line of the .csv file which signals to Excel that the delimiter is comma in that file
# This ensures proper separation when file is opened, since by default Excel considers only Tab.
#"sep=," | Out-File -FilePath $OutFile -Append

for ($taskIndex = 0; $taskIndex -lt $tasks.Length; ++$taskIndex)
{
    $task = $tasks[$taskIndex]

    $table = @{}
    $foundEpochs = @()
    $foundMetrics= @()

    $taskLines[$task].ForEach{
        # Line format example
        # "...Finished Epoch[81 of 500]: [Training] CE = 0.01338955 * 70022; pixelwiseError = 0.08412182 * 70022;..."

        # In each line find the parts of the following form "Epoch[ A of" and "B = C"
        # A - epoch number
        # B - metric name
        # C - metric value
        $substrings = $_ -split ';'
        
        # Match epoch.
        Check ($substrings[0] -cmatch "Epoch\[ *(\d+) of") "Could not match epoch field"
        $epoch = [int]$Matches[1]

        Check ($table[$epoch] -eq $null) "Epoch $epoch seen in two different lines"
        $table[$epoch] = @{}
        $foundEpochs += $epoch

        $substrings.ForEach{

            # Match one metric field.
            Check ($_ -cmatch "([a-zA-Z0-9_]+) = (\d+\.\d+) \* (\d+)") "Could not match metric field"
            $metricName = $Matches[1]
            $metricValue = [float]$Matches[2]

            if ($Metrics -contains $metricName)
            {
                # This metric is requested, insert value into table.
                $table[$epoch][$metricName] = $metricValue
            }
        }

        # Collect metrics found in log file, and sort them to be consistent
        # with the list of requested metrics.
        $foundMetricsInCurrentEpoch = @()
        foreach ($metric in $Metrics)
        {
            if ($table[$epoch].ContainsKey($metric))
            {
                $foundMetricsInCurrentEpoch += $metric
            }
        }

        if ($foundEpochs.Length -eq 1)
        {
            # This is the first epoch we're seeing.
            $foundMetrics = $foundMetricsInCurrentEpoch
        }
        else
        {
            # Current epoch needs to have the same metrics as the first one.
            Check ((Compare-Object $foundMetrics $foundMetricsInCurrentEpoch -SyncWindow 0).Length -eq 0) `
                "Epoch $epoch does not have the same metrics as epoch $($foundEpochs[0])"
        }
    }

    # Apply formatting depending on column type.
    foreach ($metric in $foundMetrics)
    {
        $format = "{0:0.00000}"
        if ($PercentMetrics -contains $metric)
        {
            $format = "{0:0.00%}"
        }
        foreach ($epoch in $foundEpochs)
        {
            $table[$epoch][$metric] = ($format -f $table[$epoch][$metric])
        }
    }

    if ($taskIndex -eq 0)
    {
        # Processing first task, write table header to file.
        ("Task${Separator}Epoch$Separator" + ($foundMetrics -join "$Separator")) | Out-File -FilePath $OutFile -Append
        $foundMetricsInFirstTask = $foundMetrics
    }
    else
    {
        # Current task needs to have the same metrics as the first one.
        Check ((Compare-Object $foundMetrics $foundMetricsInFirstTask -SyncWindow 0).Length -eq 0) `
            "Task $task does not have the same metrics as task $($tasks[0])"
    }

    # Write table content to file.
    foreach ($epoch in ($foundEpochs | Sort-Object))
    {
        $line = "$task$Separator$epoch"
        foreach ($metric in $foundMetrics)
        {
            $line += "$Separator$($table[$epoch][$metric])"
        }
        $line | Out-File -FilePath $OutFile -Append
    }
}