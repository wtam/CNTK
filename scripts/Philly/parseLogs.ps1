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
Metrics name we want to extract, default "Err".
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
    [string]$Metrics = "Err",

    [Parameter(Mandatory = $False, Position = 5)]
    [ValidateNotNullOrEmpty()]
    [string]$Separator = ","
)

# Include common functions.
. (Join-Path -Path "$PSScriptRoot" -ChildPath "..\common\common.ps1")

# Find all files with the extension logrank0 in the specified directory and its subdirectories
# and extract lines with a specific pattern
$res = (Get-ChildItem -Recurse -Path $LogDir -Filter *.logrank0 | Select-String -pattern "Finished Epoch..*$Mode")

# This will be the first line of the .csv file which signals to Excel that the delimiter is comma in that file
# This ensures proper separation when file is opened, since by default Excel considers only Tab.
#$matchesFound = "sep=," + "`r`n"

$res.ForEach{
    # Line format example
    # "...Finished Epoch[12 of 16]: [Training] CE = 0.99644038 * 10249728; Err = 0.25155936 * 10249728;..."

    # In each line find the parts of the following form "Epoch[ A of" and "Err = B"
    # A - epoch number
    # B - error
    $lineMatched = $_ -cmatch "Epoch\[ *(\d+) of..*$Metrics = (\d+\.\d+)"

    # Check whether matching above regex returned anything
    Check -Condition ($lineMatched) -Message "No line matched specified pattern"

    # Write the output in format: epochNumber,errorInPercentage
    $matchesFound += $Matches[1] + $Separator
    $matchesFound += "{0:0.00%}" -f [float] $Matches[2] # format error as percentage with 2 decimals
    $matchesFound += "`r`n"
}

$matchesFound | Out-File -filepath $OutFile
