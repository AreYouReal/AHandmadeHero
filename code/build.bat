@echo off

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Zi ..\AHandmadeHero\code\Win32_HandmadeMain.cpp user32.lib gdi32.lib
popd
    