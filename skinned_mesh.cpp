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

	FbxGeometryConverter fbx_converter(fbx_manager);
	if (triangulate)
	{
		fbx_converter.Triangulate(fbx_scene, true/*replace*/, false/*legacy*/);
		fbx_converter.RemoveBadPolygonsFromMeshes(fbx_scene);
	}

	std::function<void(FbxNode*)> traverse{ [&](FbxNode* fbx_node)
		{
			scene::node& node{scene_view.nodes.emplace_back()};
			node.attribute = fbx_node->GetNodeAttribute() ?
				fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;
			node.name = fbx_node->GetName();
			node.unique_id = fbx_node->GetUniqueID();
			node.praent_index = scene_view.indexof(fbx_node->GetParent() ?
				fbx_node->GetParent()->GetUniqueID() : 0);
			for (int child_index = 0; child_index < fbx_node->GetChildCount(); child_index++)
			{
				traverse(fbx_node->GetChild(child_index));
			}
		} };

	traverse(fbx_scene->GetRootNode());	

#if 1
	for (const scene::node& node : scene_view.nodes)
	{
		FbxNode* fbx_node{ fbx_scene->FindNodeByName(node.name.c_str()) };
		// Display node data in the output window as debug
		std::string node_name = fbx_node->GetName();
		uint64_t uid = fbx_node->GetUniqueID();
		uint64_t prant_uid = fbx_node->GetParent() ? fbx_node->GetParent()->GetUniqueID() : 0;
		int32_t type = fbx_node->GetNodeAttribute() ? fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;

		std::stringstream debug_string;
		debug_string << node_name << ":" << uid << ":" << prant_uid << ":" << type << "\n";
		OutputDebugStringA(debug_string.str().c_str());
	}

#endif
    fbx_importer->Destroy();
}