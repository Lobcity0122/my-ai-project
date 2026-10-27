#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tinygltf-release/tiny_gltf.h"

class gltf_model
{
    std::string filename;
public:
    gltf_model(ID3D11Device* device, const std::string& filename);
    virtual ~gltf_model() = default;

    struct scene
    {
        std::string name;
        std::vector<int> nodes;
    };
    std::vector<scene> scenes;
    int default_scene = 0;

    struct node
    {
        std::string name;
        int skin{ -1 };
        int mesh{ -1 };
        std::vector<int> children;
        DirectX::XMFLOAT4 rotation{ 0, 0, 0, 1 };
        DirectX::XMFLOAT3 scale{ 1, 1, 1 };
        DirectX::XMFLOAT3 translation{ 0, 0, 0 };
        DirectX::XMFLOAT4X4 global_transform{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    };
    std::vector<node> nodes;

    struct buffer_view
    {
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        int buffer{ -1 };
        size_t stride_in_bytes{ 0 };
        size_t byte_offset{ 0 };
        size_t count{ 0 };
    };
    struct mesh
    {
        std::string name;
        struct primitive
        {
            int material;
            std::map<std::string, buffer_view> vertex_buffer_views;
            buffer_view index_buffer_view;
            bool has(const char* attribute) const
            {
                return vertex_buffer_views.find(attribute) != vertex_buffer_views.end()
                    && vertex_buffer_views.at(attribute).buffer > -1;
            }
        };
        std::vector<primitive> primitives;
    };
    std::vector<mesh> meshes;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> buffers;

    void fetch_nodes(const tinygltf::Model& gltf_model);
    void cumulate_transforms(std::vector<node>& nodes);
    void fetch_meshes(ID3D11Device* device, const tinygltf::Model& gltf_model);
};
