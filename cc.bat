@ECHO OFF

CALL vars.bat

SET CL_FLAGS=/nologo 
SET CL_FLAGS=%CL_FLAGS% /std:c++17 /Zi
SET LINK_LIB=user32.lib mincore.lib gdi32.lib usp10.lib

PUSHD "%BUILD_DIR%"

"%VC_CL%" %CL_FLAGS% "%PROJECT_DIR%\main.cpp" %LINK_LIB%

POPD
