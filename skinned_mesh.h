#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#include <vector>
#include <string>
#include <fbxsdk.h>

using namespace Microsoft::WRL;

// シーン全体、ノード階層を管理する構造体
struct scene
{
	// FBXシーンのノード(ボーン、メッシュ、ライト等)を表す構造体
	struct node
	{
		uint64_t unique_id{ 0 };                        // ノード固有の識別ID
		std::string name;                               // ノード名
		FbxNodeAttribute::EType attribute{ FbxNodeAttribute::EType::eUnknown }; // 属性タイプ（メッシュ、ボーン等）
		int64_t parent_index{ -1 };                     // 親ノードの配列インデックス（ルートの場合は -1）
	};

	std::vector<node> nodes;

	// 指定したUniqueIDからノード配列のインデックスを探索するヘルパー関数
	int64_t indexof(uint64_t unique_id) const
	{
		int64_t index{ 0 };
		for (const node& node : nodes)
		{
			if (node.unique_id == unique_id)
			{
				return index;
			}

			++index;
		}
		return -1; // 見つからない場合は－1
	}
};

// スキンメッシュ(3Dキャラクター等)を管理、描画するクラス
class skinned_mesh
{
public:
	// 頂点データ構造体(位置、法線、テクスチャ座標)
	struct vertex
	{
		DirectX::XMFLOAT3 position;                     // 頂点座標
		DirectX::XMFLOAT3 normal{ 0, 1, 0 };            // 法線ベクトル
		DirectX::XMFLOAT2 texcoord{ 0, 0 };             // UV座標
	};

	// 定数バッファ用構造体(ワールド行列やマテリアルカラー等)
	struct constants
	{
		DirectX::XMFLOAT4X4 world;
		DirectX::XMFLOAT4 material_color;
	};

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer;

public:
	// コンストラクタ：FBXファイルを読み込み、シーンやノードツリーを構築する
	skinned_mesh(ID3D11Device * device, const char* fbx_filename, bool triangulate = false);
	virtual ~skinned_mesh() = default;

protected:
	scene scene_view; // シーンビュー(ノード階層情報)
};