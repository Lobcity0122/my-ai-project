#include <sstream>
#include <functional>
#include <algorithm>
#include "misc.h"
#include "shader.h"
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
    traverse(fbx_scene->GetRootNode());

    fetch_meshes(fbx_scene, meshes);

    // 8. マネージャーを破棄することで、すべてのFBXオブジェクトを一括解放
    fbx_manager->Destroy();

    create_com_objects(device, fbx_filename);
}

// FBXシーンからメッシュ情報を抽出する関数
void skinned_mesh::fetch_meshes(FbxScene* fbx_scene, std::vector<mesh>& meshes)
{
    for (const scene::node& node : scene_view.nodes)
    {
        if (node.attribute != FbxNodeAttribute::EType::eMesh)
        {
            continue;
        }

        FbxNode * fbx_node{ fbx_scene->FindNodeByName(node.name.c_str()) };
        FbxMesh * fbx_mesh{ fbx_node->GetMesh() };
        
        mesh & mesh{ meshes.emplace_back() };
        mesh.unique_id = fbx_node->GetUniqueID();
        mesh.name = fbx_node->GetName();
        mesh.node_index = scene_view.indexof(mesh.unique_id);
        
        const int polygon_count{ fbx_mesh->GetPolygonCount() };
        mesh.vertices.resize(polygon_count * 3LL);
        mesh.indices.resize(polygon_count * 3LL);
        
        FbxStringList uv_names;
        fbx_mesh->GetUVSetNames(uv_names);
        const FbxVector4 * control_points{ fbx_mesh->GetControlPoints() };
        for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
        {
           for (int position_in_polygon = 0; position_in_polygon < 3; ++position_in_polygon)
           {
               const int vertex_index{ polygon_index * 3 + position_in_polygon };

               vertex vertex;
               const int polygon_vertex{ fbx_mesh->GetPolygonVertex(polygon_index, position_in_polygon) };
               vertex.position.x = static_cast<float>(control_points[polygon_vertex][0]);
               vertex.position.y = static_cast<float>(control_points[polygon_vertex][1]);
               vertex.position.z = static_cast<float>(control_points[polygon_vertex][2]);
               
			   // 読み込んだ頂点座標からバウンディングボックスの最小・最大座標を更新する
               bounding_box_min.x = (std::min)(bounding_box_min.x, vertex.position.x);
               bounding_box_min.y = (std::min)(bounding_box_min.y, vertex.position.y);
               bounding_box_min.z = (std::min)(bounding_box_min.z, vertex.position.z);

               bounding_box_max.x = (std::max)(bounding_box_max.x, vertex.position.x);
               bounding_box_max.y = (std::max)(bounding_box_max.y, vertex.position.y);
               bounding_box_max.z = (std::max)(bounding_box_max.z, vertex.position.z);

               if (fbx_mesh->GetElementNormalCount() > 0)
               {
                   FbxVector4 normal;
                   fbx_mesh->GetPolygonVertexNormal(polygon_index, position_in_polygon, normal);
                   vertex.normal.x = static_cast<float>(normal[0]);
                   vertex.normal.y = static_cast<float>(normal[1]);
                   vertex.normal.z = static_cast<float>(normal[2]);
               }

               if (fbx_mesh->GetElementUVCount() > 0)
               {
                   FbxVector2 uv;
                   bool unmapped_uv;
                   fbx_mesh->GetPolygonVertexUV(polygon_index, position_in_polygon,
                       uv_names[0], uv, unmapped_uv);
                   vertex.texcoord.x = static_cast<float>(uv[0]);
                   vertex.texcoord.y = 1.0f - static_cast<float>(uv[1]);
               }

               mesh.vertices.at(vertex_index) = std::move(vertex);
               mesh.indices.at(vertex_index) = vertex_index;
           }
        }
    }
}

// GPUバッファ（頂点/インデックスバッファ）生成
void skinned_mesh::create_com_objects(ID3D11Device* device, const char* fbx_filename)
{
    for (mesh& mesh : meshes)
    {
        HRESULT hr{ S_OK };
        D3D11_BUFFER_DESC buffer_desc{};
        D3D11_SUBRESOURCE_DATA subresource_data{};
        buffer_desc.ByteWidth = static_cast<UINT>(sizeof(vertex) * mesh.vertices.size());
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        buffer_desc.CPUAccessFlags = 0;
        buffer_desc.MiscFlags = 0;
        buffer_desc.StructureByteStride = 0;
        subresource_data.pSysMem = mesh.vertices.data();
        subresource_data.SysMemPitch = 0;
        subresource_data.SysMemSlicePitch = 0;
        hr = device->CreateBuffer(&buffer_desc, &subresource_data,
            mesh.vertex_buffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
        buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        subresource_data.pSysMem = mesh.indices.data();
        hr = device->CreateBuffer(&buffer_desc, &subresource_data,
            mesh.index_buffer.ReleaseAndGetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

    #if 1 // ※レイキャストを使う場合はここは0かコメントアウトしておく。(vertices)
        mesh.vertices.clear();
        mesh.indices.clear();
    #endif
    }

    HRESULT hr = S_OK;
    D3D11_INPUT_ELEMENT_DESC input_element_desc[]
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT },
    };
    create_vs_from_cso(device, "skinned_mesh_vs.cso", vertex_shader.ReleaseAndGetAddressOf(),
        input_layout.ReleaseAndGetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
    create_ps_from_cso(device, "skinned_mesh_ps.cso", pixel_shader.ReleaseAndGetAddressOf());
    
    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.ByteWidth = sizeof(constants);
    buffer_desc.Usage = D3D11_USAGE_DEFAULT;
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = device->CreateBuffer(&buffer_desc, nullptr, constant_buffer.ReleaseAndGetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

// 描画関数
void skinned_mesh::render(ID3D11DeviceContext* immediate_context,
    const XMFLOAT4X4& world, const XMFLOAT4& material_color)
{
    for (const mesh& mesh : meshes)
    {
        uint32_t stride{ sizeof(vertex) };
        uint32_t offset{ 0 };
        immediate_context->IASetVertexBuffers(0, 1, mesh.vertex_buffer.GetAddressOf(), &stride, &offset);
        immediate_context->IASetIndexBuffer(mesh.index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        immediate_context->IASetInputLayout(input_layout.Get());
        
        immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        
        constants data;
        data.world = world;
        data.material_color = material_color;
        immediate_context->UpdateSubresource(constant_buffer.Get(), 0, 0, &data, 0, 0);
        immediate_context->VSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
        
        D3D11_BUFFER_DESC buffer_desc;
        mesh.index_buffer->GetDesc(&buffer_desc);
        immediate_context->DrawIndexed(buffer_desc.ByteWidth / sizeof(uint32_t), 0, 0);
    }
}