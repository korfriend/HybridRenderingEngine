@echo off
REM ---------------------------------------------------------------------------------------------
REM Fails the build when this repo's VimCommon.h or VimHelpers.h has drifted from the canonical
REM copies in CoreAPIs.
REM
REM WHY A COPY EXISTS AT ALL. This renderer is distributed as its own open-source repository, so it
REM cannot reach into a sibling CoreAPIs checkout by relative path the way the in-tree native modules
REM do (`#include "../../CoreAPIs/CommonUnits/VimCommon.h"`). It carries its own copy, and the
REM include path resolves '.' first, so THIS copy is what the DLL is compiled against.
REM
REM WHY THAT NEEDS A GATE. __VERSION and the VimCommon layout are the contract the loader refuses on
REM (GpuManager / VmModuleArbiter). If CoreAPIs bumps the header and this copy is not synced, the
REM build SUCCEEDS and produces a DLL stamped with the OLD version -- which is then refused at load,
REM far from the cause. That happened on the 1.72 -> 1.73 bump: the canonical header was edited, the
REM solution rebuilt clean, and the renderer DLL still reported 1.72 until someone scanned the binary.
REM
REM WHY TWO FILES. VimCommon.h includes VimHelpers.h, so the helper header is part of the same
REM compiled contract and is copied by the same batch. A gate that watches only one of the two lets
REM the other drift for weeks with nothing to notice; both must match their canonical copies.
REM
REM INERT WHEN THE CANONICAL COPY IS ABSENT, deliberately. A standalone clone of this repository has
REM nothing to compare against and must still build; there is no drift to detect when there is only
REM one copy. The gate is for the workspace layout, where both are present and can disagree.
REM
REM CRLF AND 7-BIT ASCII ON PURPOSE. While this file was LF-only and one REM line held a UTF-8 em
REM dash, cmd.exe printed eleven "'M' is not recognized" lines on every build; either change alone
REM silences it, and the fix keeps both so a re-normalised checkout cannot bring it back.
REM
REM Usage: check_vimcommon_sync.bat <this_repo_root>
REM Exit:  0 = in sync, or canonical absent (standalone clone)
REM        1 = drifted, or a local copy is missing
REM ---------------------------------------------------------------------------------------------
setlocal
set "REPO=%~1"
if "%REPO%"=="" set "REPO=%~dp0.."

set "CANON_DIR=%REPO%\..\..\CoreAPIs\CommonUnits"

if not exist "%REPO%\VimCommon.h" (
	echo vimcommon-sync: FAIL - this repo's VimCommon.h is missing at "%REPO%\VimCommon.h"
	exit /b 1
)
if not exist "%REPO%\VimHelpers.h" (
	echo vimcommon-sync: FAIL - this repo's VimHelpers.h is missing at "%REPO%\VimHelpers.h"
	exit /b 1
)

if not exist "%CANON_DIR%\VimCommon.h" (
	echo vimcommon-sync: SKIP - no CoreAPIs checkout beside this repo; nothing to compare against.
	exit /b 0
)
REM A CoreAPIs checkout that has VimCommon.h but not VimHelpers.h is a broken checkout, not a
REM standalone clone, so from here on a missing canonical file is a failure rather than a skip.
if not exist "%CANON_DIR%\VimHelpers.h" (
	echo vimcommon-sync: FAIL - canonical VimHelpers.h is missing at "%CANON_DIR%\VimHelpers.h"
	exit /b 1
)

REM Both files are compared before the verdict, so one run names every file that drifted.
set "DRIFT="
call :compare VimCommon.h
call :compare VimHelpers.h
if defined DRIFT (
	echo.
	echo   These copies are what the DLL compiles against. A VimCommon.h drift stamps the DLL with
	echo   the version in the STALE header and the loader refuses it at runtime, far from here. A
	echo   VimHelpers.h drift moves nothing the loader checks: the DLL loads, and its inline math
	echo   and helper bodies silently differ from the core's.
	echo   Fix: copy the canonical files over these, then rebuild ALL configurations of this
	echo   project - including Release_DX11.0 and Release_DX10.0, which a Release/Debug-only build
	echo   leaves behind.
	exit /b 1
)

echo vimcommon-sync: OK ^(VimCommon.h and VimHelpers.h match CoreAPIs^)
exit /b 0

REM /B = binary compare. The file is the ABI contract, so "differs by whitespace" is still differs:
REM the two must be the same bytes, because that is what makes "compiled from the same header" true.
:compare
fc /B "%REPO%\%~1" "%CANON_DIR%\%~1" >nul 2>&1
if errorlevel 1 (
	echo vimcommon-sync: FAIL - %~1 has DRIFTED from the canonical copy.
	echo   this repo : "%REPO%\%~1"
	echo   canonical : "%CANON_DIR%\%~1"
	set "DRIFT=1"
)
exit /b 0
