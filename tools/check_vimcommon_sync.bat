@echo off
REM ---------------------------------------------------------------------------------------------
REM Fails the build when this repo's VimCommon.h has drifted from the canonical one in CoreAPIs.
REM
REM WHY A COPY EXISTS AT ALL. This renderer is distributed as its own open-source repository, so it
REM cannot reach into a sibling CoreAPIs checkout by relative path the way the in-tree native modules
REM do (`#include "../../CoreAPIs/CommonUnits/VimCommon.h"`). It carries its own copy, and the
REM include path resolves '.' first, so THIS copy is what the DLL is compiled against.
REM
REM WHY THAT NEEDS A GATE. __VERSION and the VimCommon layout are the contract the loader refuses on
REM (GpuManager / VmModuleArbiter). If CoreAPIs bumps the header and this copy is not synced, the
REM build SUCCEEDS and produces a DLL stamped with the OLD version — which is then refused at load,
REM far from the cause. That happened on the 1.72 -> 1.73 bump: the canonical header was edited, the
REM solution rebuilt clean, and the renderer DLL still reported 1.72 until someone scanned the binary.
REM
REM INERT WHEN THE CANONICAL COPY IS ABSENT, deliberately. A standalone clone of this repository has
REM nothing to compare against and must still build; there is no drift to detect when there is only
REM one copy. The gate is for the workspace layout, where both are present and can disagree.
REM
REM Usage: check_vimcommon_sync.bat <this_repo_root>
REM Exit:  0 = in sync, or canonical absent (standalone clone)
REM        1 = drifted, or the local copy is missing
REM ---------------------------------------------------------------------------------------------
setlocal
set "REPO=%~1"
if "%REPO%"=="" set "REPO=%~dp0.."

set "MINE=%REPO%\VimCommon.h"
set "CANON=%REPO%\..\..\CoreAPIs\CommonUnits\VimCommon.h"

if not exist "%MINE%" (
	echo vimcommon-sync: FAIL - this repo's VimCommon.h is missing at "%MINE%"
	exit /b 1
)

if not exist "%CANON%" (
	echo vimcommon-sync: SKIP - no CoreAPIs checkout beside this repo; nothing to compare against.
	exit /b 0
)

REM /B = binary compare. The file is the ABI contract, so "differs by whitespace" is still differs:
REM the two must be the same bytes, because that is what makes "compiled from the same header" true.
fc /B "%MINE%" "%CANON%" >nul 2>&1
if errorlevel 1 (
	echo vimcommon-sync: FAIL - VimCommon.h has DRIFTED from the canonical copy.
	echo   this repo : "%MINE%"
	echo   canonical : "%CANON%"
	echo.
	echo   This copy is what the DLL compiles against, so building now would stamp it with the
	echo   version in the STALE header and the loader would refuse the DLL at runtime, far from here.
	echo   Fix: copy the canonical file over this one, then rebuild ALL configurations of this
	echo   project - including Release_DX11.0 and Release_DX10.0, which a Release/Debug-only build
	echo   leaves behind.
	exit /b 1
)

echo vimcommon-sync: OK ^(VimCommon.h matches CoreAPIs^)
exit /b 0
