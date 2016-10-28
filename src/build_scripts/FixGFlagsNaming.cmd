:: Glog nuget package has dependency on GFlags nuget package
:: Converter also has direct dependency on GFlags
:: Unfortunately in GLog nuget package, dependency to GFlags dll was incorrectly set (naming is wrong)
:: For this reasons Converter needs gflags.dll/gflagsd.dll in release/debug
:: and GLog needs libgflags.dll/libgflags-debug.dll in release/debug
:: This scripts is a workaround for this issue.

set OUTPUT_DIR=%~1%
set BUILD_CONFIG=%2%

if %BUILD_CONFIG% == Release (
    set originalDllName=gflags.dll
    set newDllName=libgflags.dll
) else if %BUILD_CONFIG% == Debug (
    set originalDllName=gflagsd.dll
    set newDllName=libgflags-debug.dll
) else (
    echo FixGFlagsNaming.cmd : error: Unknown build configuration %BUILD_CONFIG%
    exit 1
)

if exist "%OUTPUT_DIR%\%newDllName%" (
    echo FixGFlagsNaming.cmd : "%newDllName%" already exists
) else if not exist "%OUTPUT_DIR%\%originalDllName%" (
    echo FixGFlagsNaming.cmd : error: "%originalDllName%" missing
    exit 1
) else (
    echo FixGFlagsNaming.cmd : mklink /H "%OUTPUT_DIR%\%newDllName%" "%OUTPUT_DIR%\%originalDllName%"
    mklink /H "%OUTPUT_DIR%\%newDllName%" "%OUTPUT_DIR%\%originalDllName%"
)