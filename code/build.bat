@echo off

set CommonCompilerFlags=-MT -nologo -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Zi -FmWin32_Handmade.map
set CommonLinkerFlags= -opt:ref user32.lib gdi32.lib
REM TODO - can we just build both with one exe?

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\AHandmadeHero\code\Win32_HandmadeMain.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
cl %CommonCompilerFlags% ..\AHandmadeHero\code\Win32_HandmadeMain.cpp /link %CommonLinkerFlags%
popd
    