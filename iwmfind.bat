:: �ݒ� ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	@echo off
	cd %~dp0
	%~d0
	cls

	:: �t�@�C�����̓\�[�X�Ɠ���
	set fn=%~n0
	set exec=%fn%.exe
	set op_link=-O2 -lgdi32 -luser32 -lshlwapi
	set lib=lib_iwmutil.a sqlite3.a

	set src=%fn%.c

:: make ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

	echo --- Compile -S ------------------------------------
	for %%s in (%src%) do (
		gcc.exe %%s -S %op_link%
		wc -l %%~ns.s
	)
	echo.

	echo --- Make ------------------------------------------
	for %%s in (%src%) do (
		gcc.exe %%s -c -Wall %op_link%
	)
	gcc.exe *.o %lib% -o %exec% %op_link%
	echo %exec%

	:: �㏈��
	strip -s %exec%
	rm *.o

	:: ���s
	if not exist "%exec%" goto end

	:: ����
	echo.
	pause

:: �e�X�g ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
	cls
	set tm1=%time%
	echo [%tm1%]

	%exec% . -r

	echo.
	echo [%tm1%]
	echo [%time%]

:: �I�� ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:end
	echo.
	pause
	exit
