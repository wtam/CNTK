#
# Daily build of CNTK on Jenkins.
#

# Include shared Jenkins functions.
. (Join-Path -Path $PSScriptRoot -ChildPath "JenkinsJobs.ps1")

# Jenkins build credentials.
$ApiToken = "3d52374111dd244bd0363604ad00f763"
$User = "movasi"
$Domain = "EUROPE"

[int]$TimeOut = 8 * 60 # 8 hours

$fcnBranch = "ascience/master"

# Submit Jenkins job and wait to finish.
$Builds = Submit-JenkinsJob `
            -ApiToken $ApiToken `
            -Repository "Private" `
            -Commit $fcnBranch `
            -OS "Linux" `
            -BuildConfiguration "Release" `
            -TestMode "None" `
            -TargetConfiguration 'GPU and CPU and 1BitSGD' `
            -TimeOut $TimeOut `
            -User $User `
            -Domain $Domain

If (!!$Builds)
{
    # Clear build log.
    # TODO: If last build is broken, older successful build should be used.
    $JenkinsBuildLog = Get-JenkinsBuildLog
    If (Test-Path $JenkinsBuildLog)
    {
        Remove-Item -Path $JenkinsBuildLog -Force
    }

    # Add date property to builds.
    $date = Get-Date
    $Builds | Add-Member -Name Date -Value $date -MemberType NoteProperty -Force

    # Save builds to Jenkins log.
    $Builds | Export-Csv -Path $JenkinsBuildLog -Append -Force
}
# Else: Not waiting for the job to complete or job failed in which case proper message will be displayed inside
#       Submit-JenkinsJob function.