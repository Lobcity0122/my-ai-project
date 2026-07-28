#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#include <vector>
#include <string>
#include <fbxsdk.h>

using namespace Microsoft::WRL;

// ゲームエンジン側で保持する独自シーン・ノード構造体
struct scene
{
	struct node
	{
		uint64_t unique_id{ 0 };
		std::string name;
		FbxNodeAttribute::EType attribute{ FbxNodeAttribute::EType::eUnknown };
		int64_t parent_index{ -1 };
	};

	std::vector<node> nodes;

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
		return -1;
	}
};

class skinned_mesh
{
public:
	struct vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 normal{ 0, 1, 0 };
		DirectX::XMFLOAT2 texcoord{ 0, 0 };
	};

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
	skinned_mesh(ID3D11Device * device, const char* fbx_filename, bool triangulate = false);
	virtual ~skinned_mesh() = default;

protected:
	scene scene_view;
};