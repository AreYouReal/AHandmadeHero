@echo off

REM TODO - can we just build both with one exe?

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -MT -nologo -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Zi -FmWin32_Handmade.map ..\AHandmadeHero\code\Win32_HandmadeMain.cpp /link -opt:ref user32.lib gdi32.lib 
popd
    