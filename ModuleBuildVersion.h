// Module build version = "<VimCommon __VERSION>.<build count>", exported as __GetModuleBuildVersion and
// logged by the core module loaders. REWRITTEN BY THE BUILD (tools/bump_module_build.ps1, MSBuild target
// BumpModuleBuildCount) whenever this module's sources changed; the count restarts at 1 when __VERSION
// changes. Do not edit by hand. Tracked on purpose so the count is shared.
#pragma once
#define VM_MODULE_BOUND_CORE "1.80"
#define VM_MODULE_BUILD_COUNT_STR "1"
