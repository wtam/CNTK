# Include common functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "..\common\common.ps1")

function Get-JenkinsJobDetails
{
    <#
    .SYNOPSIS
    Gets Jenkins job details.
    .DESCRIPTION
    Gets Jenkins job details: URL to Jenkins job JSON API and URL to HTML page that contains job console output in plain text.
    .EXAMPLE
    Get-JenkinsJobDetails -JobLocation http://jenkins.ccp.philly.selfhost.corp.microsoft.com/queue/item/27890
    .PARAMETER JobLocation
    Location of a job in Jenkins queue after submission. It is part of response header after submitting job to Jenkins. E.g. if $response contains Jenkins response, $response.Headers["Location"] will contain job location.
    #>
    [CmdLetBinding()]
    Param(
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$JobLocation
    )

    # e.g. http://jenkins.ccp.philly.selfhost.corp.microsoft.com/queue/item/27890/api/json
    $QueuedJobApi = $JobLocation + 'api/json'
    $QueuedJob = Invoke-WebRequest -Uri $QueuedJobApi
    $QueuedJobContent = ConvertFrom-Json -InputObject $QueuedJob.Content
    # e.g. http://jenkins.ccp.philly.selfhost.corp.microsoft.com/job/CNTK-Build-And-Test-Workflow-Private/357/consoleText
    $ExecutableUrl = $QueuedJobContent.executable.url
    $ConsoleOutput = $ExecutableUrl + "consoleText"
    $JsonApi = $ExecutableUrl + "api/json"
    return @{
        JsonApi = $JsonApi
        ConsoleOutput = $ConsoleOutput
    }
}

function HasFinished-JenkinsJob
{
    <#
    .SYNOPSIS
    Determines whether Jenkins job has finished.
    .DESCRIPTION
    Determines whether Jenkins job has finished based on its console output.
    .EXAMPLE
    HasFinished-JenkinsJob -JobJsonApiUrl http://jenkins.ccp.philly.selfhost.corp.microsoft.com/queue/item/27890/api/json
    .PARAMETER JobJsonApiUrl
    URL to Jenkins job JSON API.
    #>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$JobJsonApiUrl
        )
    $JobDetails = Invoke-WebRequest -Uri $JobJsonApiUrl
    $IsFinished = $JobDetails.ParsedHtml.body.innerText -cmatch '"building":false'
    If (!$IsFinished)
    {
        $IsBuilding = $JobDetails.ParsedHtml.body.innerText -cmatch '"building":true'
        $ErrorMessage =
            "Unknown Jenkins job status. JSON response doesn't contain field in format ""building"":true|false"
        Check `
            -Condition $IsBuilding `
            -Message $ErrorMessage
    }

    return $IsFinished
}

function GetBuildsFrom-JenkinsJob
{    <#
    .SYNOPSIS
    Extracts build name, build id and build status for all builds in Jenkins job.
    .DESCRIPTION
    Based on Jenkins job console output, extracts build name, build id and build status for all finished builds in Jenkins job.
    .EXAMPLE
    GetBuildsFrom-JenkinsJob -JenkinsJobConsoleOutput <plain_text_content_of_jenkins_job_console_output>
    .PARAMETER JenkinsJobConsoleOutput
    Plain text content of Jenkins build console output. E.g. text content of a http://jenkins.ccp.philly.selfhost.corp.microsoft.com/job/CNTK-Build-And-Test-Workflow-Private/357/consoleText
    #>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$JenkinsJobConsoleOutput
    )
    # Build description table with three columns: build name, build id, build status. Each row contains a single build.
    $Builds = @()

    # Extract build status lines from console output. These lines look like
    #   [<job_status>] (<elapsed_time>) <name> - <description>: <url>/console
    # e.g.:
    #   [ABORTED] ( 10.16 min) BWDG - CNTK-Build-Windows: http://jenkins.ccp.philly.selfhost.corp.microsoft.com/job/CNTK-Build-Windows/24738/console
    $JobDetails = ($JenkinsJobConsoleOutput |
                    Select-String -CaseSensitive -Pattern "\[[A-Z]+\] [^\[\]]*\/console" -AllMatches)

    # For all build status lines, extract build name, build id and build status.
    $JobDetails.Matches |
    %{
        # Extract build name and build number.
        if ($_ -cmatch "\[([A-Z]+)\] *\([^\)]+\) ([^ ]+) .+[^\r\n]\/(\d+)\/console")
        {
            $BuildStatus = $Matches[1]
            $BuildName = $Matches[2]
            $BuildID = $Matches[3]
            if (($Builds | Select -ExpandProperty Name) -notcontains $BuildName)
            {
                # Builds are added only once. E.g. failed jobs are logged twice in console output, but will be added
                # only once.
                $Builds += New-Object psobject -Property @{Name=$BuildName;ID=$BuildID;Status=$BuildStatus}
            }
        }
    }

    return $Builds
}

function Submit-JenkinsJob
{
    <#
    .SYNOPSIS
    Submits a build job to Jenkins.
    .DESCRIPTION
    Submits a CNTK build job to Jenkins build for CNTK branch from private or public repository. Returns list of build drops built by Jenkins - build type, build id and build status (e.g. BLR1 23909 SUCCESS).
    .EXAMPLE
    Submit-JenkinsJob -ApiToken "5xq8tvv2b7i46whykuyjl9voopebz5rt" -Repository Private -Commit "ascience/fcn" -OS "Linux" -BuildConfiguration "Release" -TestMode "None" -TargetConfiguration "GPU"
    .PARAMETER ApiToken
    API token can be found at http://jenkins.ccp.philly.selfhost.corp.microsoft.com/user/<username>@<domain>/configure
    .PARAMETER Repository
    Git repository to fetch commit from: public or private CNTK repository (https://github.com/Microsoft/CNTK or https://github.com/Microsoft/CNTK-exp-private, respectively).
    .PARAMETER Commit
    Commit hash/branch that the build will use.
    .PARAMETER OS
    Operating system(s) to build CNTK for.
    .PARAMETER BuildConfiguration
    Configuration(s) to build.
    .PARAMETER TestMode
    Specifies how to test the build: BVT - is a build verification test (check-in gate) minimal test matrix for check-in validation. Nightly - run all the tests which are running every night, None - don't run any tests
    .PARAMETER TargetConfiguration
    Specifies which target to build CNTK for, GPU, CPU , 1BitSGD or combination of those.
    .PARAMETER TimeOut
    Specifies how long to wait for Jenkins to finish build job (in minutes). If 0, the script will just submit job to Jenkins and exit.
    #>
    [CmdletBinding()]
    Param (
        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$ApiToken,

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("Private", "Public")]
        [string]$Repository = "Public",

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [string]$Commit = "master",

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("Linux", "Windows", "Linux and Windows")]
        [string]$OS = "Linux",

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("Debug", "Release", "Debug and Release")]
        [string]$BuildConfiguration = "Release",

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("BVT", "Nightly", "None")]
        [string]$TestMode = "BVT",

        [Parameter(Mandatory = $True)]
        [ValidateNotNullOrEmpty()]
        [ValidateSet("GPU and CPU and 1BitSGD", "GPU and CPU", "GPU", "CPU", "1BitSGD")]
        [string]$TargetConfiguration = "GPU and CPU and 1BitSGD",

        [Parameter(Mandatory = $True)]
        [int]$TimeOut = 0
    )

    $JenkinsUsername = $env:USERNAME + "@" + $env:USERDOMAIN.ToLower()
    $JobNames = @{
        "Private" = "CNTK-Build-And-Test-Workflow-Private";
        "Public"  = "CNTK-Build-And-Test-Workflow"
    }
    $GitRepos = @{
        "Private" = "https://github.com/Microsoft/CNTK-exp-private";
        "Public"  = "https://github.com/Microsoft/CNTK"
    }
    $JobName = $JobNames[$Repository]
    $GitRepo = $GitRepos[$Repository]

    # Create job submission URL.
    # TODO: Generalize URL so we can reuse the script across Jenkins servers.
    $url = "http://jenkins.ccp.philly.selfhost.corp.microsoft.com/job/$JobName/buildWithParameters?delay=0sec"
    $url = $url + "&REPO_URL=$GitRepo"
    $url = $url + "&COMMIT=$Commit"
    $url = $url + "&OS=$OS"
    $url = $url + "&BUILD_CONFIGURATION=$BuildConfiguration"
    $url = $url + "&TEST_MODE=$TestMode"
    $url = $url + "&TARGET_CONFIGURATION=$TargetConfiguration"

    # Prepare HTTP header for basic authentication.
    $Credentials = "$($JenkinsUsername):$($ApiToken)"
    $EncodedCredentials = [System.Convert]::ToBase64String([System.Text.Encoding]::ASCII.GetBytes($Credentials))
    $headers = @{
        Authorization = "Basic $EncodedCredentials";
        Accept = "application/json"
    }

    # Submit Jenkins job.
    Write-Host "Submitting Jenkins job..."
    $response = Invoke-WebRequest -Headers $headers -Method Post -Uri $url
    if ($response.StatusCode -eq 201)
    {
        Write-Host -ForegroundColor Green "Success: Jenkins job submitted."
    }
    else
    {
        $ErrorMessage = "Failure: Jenkins job submission failed. Error code: {0:D}" -f $response.StatusCode
        Write-Error $ErrorMessage
        return
    }

    If ($TimeOut -le 0)
    {
        # Don't wait for Jenkins job to finish, just exit.
        return
    }

    Write-Host Waiting for job to finish...
    $JobLocation = $response.Headers["Location"]
    $JobDetails = Get-JenkinsJobDetails -JobLocation $JobLocation

    # Wait for Jenkins job to finish.
    $StartTime = Get-Date
    [bool]$HasFinished = $False
    While (!$HasFinished)
    {
        [bool]$HasFinished = HasFinished-JenkinsJob -JobJsonApiUrl $JobDetails.JsonApi
        If (!$HasFinished)
        {
            Start-Sleep -Seconds 60
        }
        $ElapsedTime = (Get-Date) - $StartTime
        $ElapsedMinutes = $ElapsedTime.Hours * 60 + $ElapsedTime.Minutes
        if ($ElapsedMinutes -gt $TimeOut)
        {
            Break
        }
        Write-Host -ForegroundColor Yellow ("Elapsed {0:D2}h:{1:D2}m" -f $ElapsedTime.Hours,$ElapsedTime.Minutes)
    }

    If (!$HasFinished)
    {
        Write-Error -Category OperationTimeout "Jenkins build not finished in specified time."
    }
    Else
    {
        $JobConsoleOutputHTML = Invoke-WebRequest -Uri $JobDetails.ConsoleOutput
        $JobConsoleOutput = $JobConsoleOutputHTML.ParsedHtml.body.innerText
        return GetBuildsFrom-JenkinsJob -JenkinsJobConsoleOutput $JobConsoleOutput
    }
}