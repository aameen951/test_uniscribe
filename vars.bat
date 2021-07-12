@SET PROJECT_DIR=%~dp0
@IF "%PROJECT_DIR:~-1%" EQU "\" SET PROJECT_DIR=%PROJECT_DIR:~0,-1%

@IF NOT EXIST "%M_BUILD_DIR%" SET BUILD_DIR=%PROJECT_DIR%\build

@IF "%BUILD_DIR%" NEQ "" GOTO :skip_build_dir_setup

@IF NOT EXIST "%M_BUILD_DIR%" GOTO :build_dir_not_defined

@SET BUILD_DIR=%M_BUILD_DIR%\%PROJECT_DIR::=%

:skip_build_dir_setup

@if NOT EXIST "%BUILD_DIR%" MKDIR %BUILD_DIR%


@ EXIT /B 0

:build_dir_not_defined
echo.
echo ** Error: build directory is not defined or doesn't exists.
echo **  - Please set BUILD_DIR first and run the script again.
echo.
@call :crash 2>nul

:crash
()