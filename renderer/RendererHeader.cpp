#include "RendererHeader.h"

void RegisterVolumeRes(VmVObjectVolume* vol_obj, VmTObject* tobj, VmLObject* lobj, VmGpuManager* pCGpuManager, ID3D11DeviceContext* pdx11DeviceImmContext,
	map<int, VmObject*>& mapAssociatedObjects, map<int, GpuRes>& mapGpuRes, LocalProgress* progress)
{
	if (vol_obj)
	{
		auto itrVolume = mapAssociatedObjects.find(vol_obj->GetObjectID());
		if (itrVolume != mapAssociatedObjects.end())
		{
			// GPU Resource Check!! : Volume //
			GpuRes gres_vol;
			grd_helper::UpdateVolumeModel(gres_vol, vol_obj, false, progress);
			mapGpuRes[vol_obj->GetObjectID()] = gres_vol;
			//mapGpuRes.insert(pair<int, GpuRes>(vol_obj->GetObjectID(), gres_vol));
		}
	}
	if (tobj)
	{
		auto itrTObject = mapAssociatedObjects.find(tobj->GetObjectID());
		if (itrTObject != mapAssociatedObjects.end())
		{
			bool is_otf_changed = false;
			lobj->GetDstObjValue(tobj->GetObjectID(), ("_bool_IsTfChanged"), &is_otf_changed);
			is_otf_changed |= grd_helper::CheckOtfAndVolBlobkUpdate(vol_obj, tobj);

			// TObject which is not 'preintegrated' one
			map<int, VmTObject*> tobj_map;
			tobj_map.insert(pair<int, VmTObject*>(tobj->GetObjectID(), tobj));

			GpuRes gres_tmap;
			grd_helper::UpdateTMapBuffer(gres_tmap, tobj, tobj_map, NULL, NULL, 0, is_otf_changed);
			mapGpuRes[tobj->GetObjectID()] = gres_tmap;
			//mapGpuRes.insert(pair<int, GpuRes>(tobj->GetObjectID(), gres_tmap));
		}
	}
}