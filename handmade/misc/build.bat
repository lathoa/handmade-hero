@echo off

set CommonCompilerFlags=-nologo -Oi -fp:fast -MTd -Gm- -GR- -EHa -Od -Oi -WX -W4 -wd4505 -wd4201 -wd4456 -wd4100 -wd4189 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -Z7 -FC
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\..\build\ mkdir ..\..\build\
pushd ..\..\build\

REM 32-bit build
REM cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags% 

REM 64-bit build
del *.pdb > NUL 2> nul
cl %CommonCompilerFlags% ..\handmade\code\handmade.cpp -LD /link -incremental:no -PDB:handmade_%random%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp /link %CommonLinkerFlags%
popd
