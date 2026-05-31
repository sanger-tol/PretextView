@ECHO OFF


REM Expecting the Torch path as the first argument
set "cmake_prefix_path_tmp=subprojects/libtorch/share/cmake"
echo cmake_prefix_path_tmp: %cmake_prefix_path_tmp%
REM ... use %TORCH_PATH% as needed ...


SET CC=clang-cl
SET CXX=clang-cl
SET WINDRES=rc

rem --- check architecture ---
set "ARCH=%PROCESSOR_ARCHITECTURE%"
if /I "%ARCH%"=="x86" (
    if defined PROCESSOR_ARCHITEW6432 (
        set "ARCH=x86_64"
    ) else (
        set "ARCH=i386"
    )
) else if /I "%ARCH%"=="AMD64" (
    set "ARCH=x86_64"
) else if /I "%ARCH%"=="ARM64" (
    set "ARCH=ARM64"
)
echo Detected architecture: %ARCH%


REM ========= pull git repo =========
git submodule update --init --recursive


REM ========= fmt =========
cd subprojects\fmt
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=%ARCH% -S . -B build
cmake --build build --config Release --target fmt 
if errorlevel 1 (
    echo "CMake fmt failed."
    goto :error
) else (
    echo "CMake fmt completed successfully."
)
cd ..\..\


REM ========= deflate =========
cd subprojects\libdeflate
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=%ARCH% -S . -B build
cmake --build build --config Release --target libdeflate_static 
if errorlevel 1 (
    echo "CMake delfate failed."
    goto :error
) else (
    echo "CMake delfate completed successfully."
)
cd ..\..\

REM ========= libtorch =========
REM remove libtorch from current version 
REM download_libtorch_win.bat

REM ========= blas =========
@REM REM Install OpenBLAS if its build directory does not exist
@REM if not exist "subprojects\OpenBLAS\build" (
@REM     cd subprojects\OpenBLAS
@REM     cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNOFORTRAN=1
@REM     cmake --build build --config Release -j 8
@REM     if errorlevel 1 (
@REM         echo "OpenBLAS build failed."
@REM         goto :error_blas
@REM     )
@REM     cd ..\..
@REM )


REM ========= Build the project =========
if exist build_cmake (
    rmdir /s /q build_cmake
    echo "Removed existing build directory."
)

if exist app (
    rmdir /s /q app
    echo "Removed existing app directory."
)

cmake -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=OFF -DCMAKE_INSTALL_PREFIX=app -DCMAKE_PREFIX_PATH=%cmake_prefix_path_tmp% -S . -B build_cmake
if errorlevel 1 (
    echo "CMake configuration failed."
    goto :error
) else (
    echo "CMake configuration completed successfully."
)

@REM cmake --build build_cmake --config Release
@REM if errorlevel 1 (
@REM     echo "Build failed."
@REM     goto :error
@REM ) else (
@REM     echo "Build completed successfully."
@REM )

@REM cmake --install build_cmake --config Release
@REM if errorlevel 1 (
@REM     echo "Install failed."
@REM     goto :error
@REM ) else (
@REM     echo "Install completed successfully."
@REM )
@REM echo Build and installation completed successfully.

cmake --build build_cmake --target package --config Release
if errorlevel 1 (
    echo "Package failed."
    goto :error
) else (
    echo "Package completed successfully."
)
echo Finished successfully.
goto :eof

:error
echo An error occurred during the build process.
exit /b 1
goto :eof
