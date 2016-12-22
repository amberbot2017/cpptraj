bash configure --with-netcdf=%PREFIX%\Library ^
               -nobzlib ^
               --with-zlib=%PREFIX%\Library ^
               -shared ^
               -nomathlib ^
               -noreadline ^
               -windows ^
               gnu

:: %PYTHON% %RECIPE_DIR%\make_win_config.py

C:\msys64\usr\bin\make libcpptraj
REM C:\msys64\usr\bin\make

mkdir -p %LIBRARY_INC%\cpptraj\
COPY bin\cpptraj.exe %LIBRARY_BIN%\
COPY lib\libcpptraj.so %LIBRARY_LIB%\
XCOPY src\*.h   %LIBRARY_INC%\cpptraj
