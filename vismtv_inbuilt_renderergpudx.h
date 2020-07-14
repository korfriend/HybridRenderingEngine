#include "../../EngineCores/CommonUnits/VimCommon.h"

__vmstatic bool CheckModuleParameters(fncontainer::VmFnContainer& _fncontainer);
__vmstatic bool InitModule(fncontainer::VmFnContainer& _fncontainer);
__vmstatic bool DoModule(fncontainer::VmFnContainer& _fncontainer);
__vmstatic void DeInitModule(fncontainer::VmFnContainer& _fncontainer);
__vmstatic double GetProgress();
__vmstatic void InteropCustomWork(fncontainer::VmFnContainer& _fncontainer);
__vmstatic void GetModuleSpecification(std::vector<std::string>& requirements);