set DS_PROJECT_DIR=%~1%
set PROTO_DIR=%~2%

@rem Compile the proto files.
"%PROTO_DIR%protoc" --proto_path="%DS_PROJECT_DIR%\proto" --cpp_out="%DS_PROJECT_DIR%\proto" "%DS_PROJECT_DIR%\proto\ds_load_params.proto"
"%PROTO_DIR%protoc" --proto_path="%DS_PROJECT_DIR%\proto" --cpp_out="%DS_PROJECT_DIR%\proto" "%DS_PROJECT_DIR%\proto\ds_save_params.proto"

@rem Compare generated load files with exiting.
fc /b "%DS_PROJECT_DIR%\proto\ds_load_params.pb.h" "%DS_PROJECT_DIR%\ds_load_params.pb.h" > NUL

if errorlevel 1 (
    @rem Files differ, copy generated load files.
    move /y "%DS_PROJECT_DIR%\proto\ds_load_params.pb.h" "%DS_PROJECT_DIR%\ds_load_params.pb.h"
    move /y "%DS_PROJECT_DIR%\proto\ds_load_params.pb.cc" "%DS_PROJECT_DIR%\ds_load_params.pb.cc"
)

@rem Delete generated load file.
del "%DS_PROJECT_DIR%\proto\ds_load_params.pb.h"
del "%DS_PROJECT_DIR%\proto\ds_load_params.pb.cc"

@rem Compare generated save files with exiting.
fc /b "%DS_PROJECT_DIR%\proto\ds_save_params.pb.h" "%DS_PROJECT_DIR%\ds_save_params.pb.h" > NUL

if errorlevel 1 (
    @rem Files differ, copy generated save files.
    move /y "%DS_PROJECT_DIR%\proto\ds_save_params.pb.h" "%DS_PROJECT_DIR%\ds_save_params.pb.h"
    move /y "%DS_PROJECT_DIR%\proto\ds_save_params.pb.cc" "%DS_PROJECT_DIR%\ds_save_params.pb.cc"
)

@rem Delete generated save file.
del "%DS_PROJECT_DIR%\proto\ds_save_params.pb.h"
del "%DS_PROJECT_DIR%\proto\ds_save_params.pb.cc"