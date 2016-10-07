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
        throw $Message
    }
}
