#include <sstream>
#include <functional>
#include "misc.h"
#include "skinned_mesh.h"

using namespace DirectX;

skinned_mesh::skinned_mesh(ID3D11Device* device, const char* fbx_filename, bool triangulate)
{
	FbxManager* fbx_manager{ FbxManager::Create() };
	FbxScene* fbx_scene{ FbxScene::Create(fbx_manager,"") };

	FbxImporter* fbx_importer{ FbxImporter::Create(fbx_manager,"") };
	bool import_status{ false };
	import_status = fbx_importer->Initialize(fbx_filename);
	_ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

	import_status = fbx_importer->Import(fbx_scene);
	_ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

	





}
