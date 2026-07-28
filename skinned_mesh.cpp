#include <sstream>
#include <functional>
#include "misc.h"
#include "skinned_mesh.h"

using namespace DirectX;

// コンストラクタ：FBXファイルのインポートとノードツリー走査
skinned_mesh::skinned_mesh(ID3D11Device* device, const char* fbx_filename, bool triangulate)
{
    // 1. FBX SDK全体の管理マネージャーを作成
    FbxManager* fbx_manager{ FbxManager::Create() };

    // 2. シーンデータを格納するコンテナを作成
    FbxScene* fbx_scene{ FbxScene::Create(fbx_manager, "") };

    // 3. ファイルを読み込むためのインポータを作成・初期化
    FbxImporter* fbx_importer{ FbxImporter::Create(fbx_manager, "") };
    bool import_status{ false };

    import_status = fbx_importer->Initialize(fbx_filename);
    _ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

    // 4. シーンデータへFBXファイルの内容をインポート
    import_status = fbx_importer->Import(fbx_scene);
    _ASSERT_EXPR_A(import_status, fbx_importer->GetStatus().GetErrorString());

    // 5. 必要に応じてポリゴンを三角形化する変換処理
    FbxGeometryConverter fbx_converter(fbx_manager);
    if (triangulate)
    {
        fbx_converter.Triangulate(fbx_scene, true/*replace*/, false/*legacy*/);
        fbx_converter.RemoveBadPolygonsFromMeshes(fbx_scene);
    }

    // 6. ルートノードから子ノードを再帰的に巡回するラムダ式
    std::function<void(FbxNode*)> traverse{ [&](FbxNode* fbx_node)
    {
        // ノード情報を格納する要素をリストの末尾に追加
        scene::node& node{ scene_view.nodes.emplace_back() };

        // ノードの属性タイプ（メッシュ、ボーン、ライトなど）を取得
        node.attribute = fbx_node->GetNodeAttribute() ?
            fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;

        // ノード名と一意の識別IDを取得
        node.name = fbx_node->GetName();
        node.unique_id = fbx_node->GetUniqueID();

        // 親ノードが存在する場合は、シーン内の親インデックスを特定して設定
        node.parent_index = scene_view.indexof(fbx_node->GetParent() ?
            fbx_node->GetParent()->GetUniqueID() : 0);

        // 子ノードの数だけ再帰的に呼び出し
        for (int child_index = 0; child_index < fbx_node->GetChildCount(); ++child_index)
        {
            traverse(fbx_node->GetChild(child_index));
        }
    } };

    // シーンのルートノードからトラバースを開始
    traverse(fbx_scene->GetRootNode());

    // 7. デバッグ用：読み込んだノードツリー情報を出力ウィンドウに表示
#if 1
    for (const scene::node& node : scene_view.nodes)
    {
        FbxNode* fbx_node{ fbx_scene->FindNodeByName(node.name.c_str()) };
        // Display node data in the output window as debug 
        std::string node_name = fbx_node->GetName();
        uint64_t uid = fbx_node->GetUniqueID();
        uint64_t parent_uid = fbx_node->GetParent() ? fbx_node->GetParent()->GetUniqueID() : 0;
		int32_t type = fbx_node->GetNodeAttribute() ? fbx_node->GetNodeAttribute()->GetAttributeType() : FbxNodeAttribute::EType::eUnknown;
            std::stringstream debug_string;
        debug_string << node_name << ":" << uid << ":" << parent_uid << ":" << type << "\n";
        OutputDebugStringA(debug_string.str().c_str());
    }
#endif
    // 8. マネージャーを破棄することで、すべてのFBXオブジェクトを一括解放
    fbx_manager->Destroy();
}