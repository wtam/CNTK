#
# Example script for submitting job to Philly cluster.
#

# Include functions from shared.ps1
. (Join-Path -Path $PSScriptRoot -ChildPath "shared.ps1")

$Job = SubmitFromLocal-PhillyJob `
            -Name FCN32 `
            -ConfigFile "D:\dnn_training_scripts\train\cntk\fcn32_7ch_init_random\fcn32_7ch_init_random.cntk" `
            -DataSet BD2 `
            -GpuCount 2

$Job
