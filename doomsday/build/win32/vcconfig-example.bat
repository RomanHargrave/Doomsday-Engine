REM Build configuration (example).
REM Modify and save as "vcconfig.bat".

SET ROOT_DIR=D:\deng\svn\trunk\doomsday

SET BIN_DIR=Bin\Release
SET OBJ_DIR=Obj\Release

REM -=- Requirements for Doomsday.exe -=-
REM
REM ---- Platform SDK:
SET PLATFORM_INC=C:\Program Files\Microsoft SDKs\Windows\v6.0A\Include
SET PLATFORM_LIB=C:\Program Files\Microsoft SDKs\Windows\v6.0A\Lib
SET LIBCI_LIB=C:\Program Files\Microsoft SDKs\Windows\v6.0A\Include

REM ---- Python Interpreter
SET PYTHON_DIR=C:\Python25

REM ---- DirectX:
SET DX_INC=C:\Program Files (x86)\Microsoft DirectX SDK (March 2008)\Include
SET DX_LIB=C:\Program Files (x86)\Microsoft DirectX SDK (March 2008)\Lib

REM ---- Creative Labs EAX:
SET EAX_INC=D:\sdk\Creative Labs\EAX 2.0 Extensions SDK\Include
SET EAX_LIB=D:\sdk\Creative Labs\EAX 2.0 Extensions SDK\Libs

REM ---- SDL:
SET SDL_INC=D:\sdk\SDL-1.2.13\include
SET SDL_LIB=D:\sdk\SDL-1.2.13\lib

REM ---- SDL_net:
SET SDLNET_INC=D:\sdk\SDL_net-1.2.6\include
SET SDLNET_LIB=D:\sdk\SDL_net-1.2.6\lib

REM ---- SDL_mixer:
SET SDLMIXER_INC=D:\sdk\SDL_mixer-1.2.7\include
SET SDLMIXER_LIB=D:\sdk\SDL_mixer-1.2.7\lib

REM -=- Requirements for drOpenGL.dll -=-
REM
REM ---- OpenGL (GL/gl.h, GL/glext.h, GL/glu.h):
SET GL_INC=D:\sdk\OpenGL

REM -=- Requirements for dsOpenAL.dll -=-
REM
REM ---- OpenAL:
SET OPENAL_INC=D:\sdk\OpenAL 1.1 SDK\include
SET OPENAL_LIB=D:\sdk\OpenAL 1.1 SDK\libs
