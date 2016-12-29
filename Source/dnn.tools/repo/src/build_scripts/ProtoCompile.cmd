set PROTO_FILE_DIR=%~1%
set PROTO_FILE_NAME=%~2%
set PROTOC_DIR=%~3%
set OUTPUT_DIR=%~4%

@rem Perform proto compilation of given proto file and place resulting files (*.pb.cc and *.pb.h) at output directory.
@echo "%PROTOC_DIR%protoc" --proto_path="%PROTO_FILE_DIR%" --cpp_out="%OUTPUT_DIR%" "%PROTO_FILE_DIR%\%PROTO_FILE_NAME%"
"%PROTOC_DIR%protoc" --proto_path="%PROTO_FILE_DIR%" --cpp_out="%OUTPUT_DIR%" "%PROTO_FILE_DIR%\%PROTO_FILE_NAME%"