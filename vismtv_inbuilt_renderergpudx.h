#include "VimCommon.h" 

__vmstatic bool CheckModuleParameters(fncontainer::VmFnContainer& _fncontainer);
__vmstatic bool InitModule(fncontainer::VmFnContainer& _fncontainer);
__vmstatic bool DoModule(fncontainer::VmFnContainer& _fncontainer);
__vmstatic void DeInitModule(fncontainer::VmFnContainer& _fncontainer);
__vmstatic int GetProgress(std::string& progressTag);
__vmstatic void InteropCustomWork(fncontainer::VmFnContainer& _fncontainer);
__vmstatic void GetModuleSpecification(std::vector<std::string>& requirements);

// customized function
__vmstatic bool GetSharedShaderResView(const int iobjId, const void* dx11devPtr, void** sharedSRV);