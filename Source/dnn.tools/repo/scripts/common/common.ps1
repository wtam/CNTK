function Check()
{
    <#
    .SYNOPSIS
    Throws exception with given message if given condition is false.
    .DESCRIPTION
    Throws exception with given message if given condition is false.
    .EXAMPLE
    Check -Condition ($a.Length -eq $b.Length) -Message "Inputs should have the same length."
    .EXAMPLE
    Check ($a.Length -eq $b.Length) "Inputs should have the same length."
    .PARAMETER Condition
    Condition to check.
    .PARAMETER Message
    Exception message.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True, Position = 1)]
        [ValidateNotNullOrEmpty()]
        [bool]$Condition,

        [Parameter(Mandatory = $True, Position = 2)]
        [ValidateNotNullOrEmpty()]
        [string]$Message
        )

    if (-not $Condition)
    {
        Fail $Message
    }
}

function LogMessage()
{
    <#
    .SYNOPSIS
    Logs message.
    .DESCRIPTION
    Logs given message to console.
    .EXAMPLE
    LogMessage -Message "Done part 1."
    .EXAMPLE
    LogMessage -Message "Done all." -Highlight true
    .PARAMETER Message
    Message to log.
    .PARAMETER Highlight
    Indicates if message is to be highlighted with different color.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True, Position = 1)]
        [ValidateNotNullOrEmpty()]
        [string]$Message,

        [Parameter(Mandatory = $false, Position = 2)]
        [ValidateNotNullOrEmpty()]
        [bool]$Highlight = $false
        )

    if ($Highlight)
    {
        Write-Host -ForegroundColor Green $Message
    }
    else
    {
        Write-Host $Message
    }
}

function LogWarning()
{
    <#
    .SYNOPSIS
    Logs warning.
    .DESCRIPTION
    Logs warning with given message to console.
    .EXAMPLE
    LogWarning -Message "Part 1 done partially."
    .PARAMETER Message
    Message to log.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True, Position = 1)]
        [ValidateNotNullOrEmpty()]
        [string]$Message
        )
    Write-Host -ForegroundColor Yellow $Message
}

function ConvertToLinux-Path()
{
    <#
    .SYNOPSIS
    Converts given path to Linux compatible one.
    .DESCRIPTION
    Replaces backslash with forward slash in a given path.
    .EXAMPLE
    ConvertToLinux-Path -Path \hdfs\anlgvc\datasets\ids\v3\Capture-70k-original-png"
    .PARAMETER Path
    Windows (or Linux) file/folder path.
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Path
        )

    $Path -replace "\\","/"
}

function Fail
{
    <#
    .SYNOPSIS
    Aborts execution with a given message.
    .DESCRIPTION
    Aborts execution by throwing a message with a given message.
    .EXAMPLE
    Fail -Message "Unknown file type."
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Message
        )
    Write-Error $Message
    throw $Message
}

function EnsureExists-Item
{
    <#
    .SYNOPSIS
    Ensures that item exists.
    .DESCRIPTION
    If the item doesn't exist, creates it.
    .EXAMPLE
    EnsureExists-Item -Path D:\dir\subdir -ItemType Directory
    .PARAMETER Path
    File/folder path.
    .PARAMETER Item-Type
    Specifies type of the item to be created (file or directory).
    #>
    [CmdletBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Path,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("File", "Directory")]
        [string]$ItemType
        )
    If (!(Test-Path $Path))
    {
        New-Item -Path $Path -ItemType $ItemType -Force | Out-Null
    }
}