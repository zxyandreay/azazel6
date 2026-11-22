@echo off
python "%~dp0ffmpeg_windows_make.py" %*
exit /b %ERRORLEVEL%
