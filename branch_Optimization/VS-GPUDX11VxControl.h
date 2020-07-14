#include "VXEngineGlobalUnit.h"

__vxstatic bool CheckIsAvailableParameters(SVXModuleParameters* pstModuleParameter);
__vxstatic bool InitModule(SVXModuleParameters* pstModuleParameter);
__vxstatic bool DoModule(SVXModuleParameters* pstModuleParameter);
__vxstatic void DeInitModule(map<wstring, wstring>* pmapCustomParamters);
__vxstatic double GetProgress();
__vxstatic void InteropCustomWork(SVXModuleParameters* pstModuleParameter);
__vxstatic void GetModuleSpecification(vector<wstring>* pvtrSpecification);