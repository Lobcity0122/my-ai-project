#include "gltf_model.h"
#define TINYGLTF_IMPLEMENTATION
#include "tinygltf-release/tiny_gltf.h"
#include "misc.h"
#include <stack>
#include <functional>

bool null_load_image_data(tinygltf::Image*, const int, std::string*, std::string*,
    int, int, const unsigned char*, int, void*)
{
    return true;
}

gltf_model::gltf_model(ID3D11Device* device, const std::string& filename) : filename(filename)
{
    tinygltf::TinyGLTF tiny_gltf;
    tiny_gltf.SetImageLoader(null_load_image_data, nullptr);
    tinygltf::Model gltf_model;
    std::string error, warning;
    bool succeeded{ false };
    if (filename.find(".glb") != std::string::npos)
    {
        succeeded = tiny_gltf.LoadBinaryFromFile(&gltf_model, &error, &warning, filename.c_str());
    }
    else if (filename.find(".gltf") != std::string::npos)
    {
        succeeded = tiny_gltf.LoadASCIIFromFile(&gltf_model, &error, &warning, filename.c_str());
    }
    _ASSERT_EXPR_A(warning.empty(), warning.c_str());
    _ASSERT_EXPR_A(error.empty(), error.c_str());
    _ASSERT_EXPR_A(succeeded, "Failed to load glTF file");
    for (std::vector<tinygltf::Scene>::const_reference gltf_scene : gltf_model.scenes)
    {
        scene& scene{ scenes.emplace_back() };
        scene.name = gltf_scene.name;
        scene.nodes = gltf_scene.nodes;
    }
    default_scene = gltf_model.defaultScene < 0 ? 0 : gltf_model.defaultScene;
    fetch_nodes(gltf_model);
    fetch_meshes(device, gltf_model);
}

void gltf_model::fetch_nodes(const tinygltf::Model& gltf_model)
{
    for (std::vector<tinygltf::Node>::const_reference gltf_node : gltf_model.nodes)
    {
        node& node{ nodes.emplace_back() };
        node.name = gltf_node.name;
        node.skin = gltf_node.skin;
        node.mesh = gltf_node.mesh;
        node.children = gltf_node.children;
        if (!gltf_node.matrix.empty())
        {
            DirectX::XMFLOAT4X4 matrix;
            for (size_t row = 0; row < 4; row++)
            {
                for (size_t column = 0; column < 4; column++)
                {
                    matrix(row, column) = static_cast<float>(gltf_node.matrix.at(4 * row + column));
                }
            }
            DirectX::XMVECTOR S, T, R;
            bool succeed = DirectX::XMMatrixDecompose(&S, &R, &T, DirectX::XMLoadFloat4x4(&matrix));
            _ASSERT_EXPR(succeed, L"Failed to decompose matrix.");
            DirectX::XMStoreFloat3(&node.scale, S);
            DirectX::XMStoreFloat4(&node.rotation, R);
            DirectX::XMStoreFloat3(&node.translation, T);
        }
        else
        {
            if (gltf_node.scale.size() > 0)
            {
                node.scale.x = static_cast<float>(gltf_node.scale.at(0));
                node.scale.y = static_cast<float>(gltf_node.scale.at(1));
                node.scale.z = static_cast<float>(gltf_node.scale.at(2));
            }
            if (gltf_node.translation.size() > 0)
            {
                node.translation.x = static_cast<float>(gltf_node.translation.at(0));
                node.translation.y = static_cast<float>(gltf_node.translation.at(1));
                node.translation.z = static_cast<float>(gltf_node.translation.at(2));
            }
            if (gltf_node.rotation.size() > 0)
            {
                node.rotation.x = static_cast<float>(gltf_node.rotation.at(0));
                node.rotation.y = static_cast<float>(gltf_node.rotation.at(1));
                node.rotation.z = static_cast<float>(gltf_node.rotation.at(2));
                node.rotation.w = static_cast<float>(gltf_node.rotation.at(3));
            }
        }
    }
    cumulate_transforms(nodes);
}

void gltf_model::cumulate_transforms(std::vector<node>& nodes)
{
    using namespace DirectX;
    std::stack<XMFLOAT4X4> parent_global_transforms;
    std::function<void(int)> traverse{ [&](int node_index)->void
    {
        node& node{ nodes.at(node_index) };
        XMMATRIX S{ XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z) };
        XMMATRIX R{ XMMatrixRotationQuaternion(
            XMVectorSet(node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w)) };
        XMMATRIX T{ XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z) };
        XMStoreFloat4x4(&node.global_transform, S * R * T * XMLoadFloat4x4(&parent_global_transforms.top()));
        for (int child_index : node.children)
        {
            parent_global_transforms.push(node.global_transform);
            traverse(child_index);
            parent_global_transforms.pop();
        }
    } };
    for (std::vector<int>::value_type node_index : scenes.at(default_scene).nodes)
    {
        parent_global_transforms.push({ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 });
        traverse(node_index);
        parent_global_transforms.pop();
    }
}

DXGI_FORMAT convert_format(const tinygltf::Accessor& accessor)
{
    switch (accessor.type)
    {
    case TINYGLTF_TYPE_SCALAR:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return DXGI_FORMAT_R8_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return DXGI_FORMAT_R16_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return DXGI_FORMAT_R32_UINT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    case TINYGLTF_TYPE_VEC2:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    case TINYGLTF_TYPE_VEC3:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    case TINYGLTF_TYPE_VEC4:
        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return DXGI_FORMAT_R8G8B8A8_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return DXGI_FORMAT_R16G16B16A16_UINT;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return DXGI_FORMAT_R32G32B32A32_UINT;
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            _ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
            return DXGI_FORMAT_UNKNOWN;
        }
    default:
        _ASSERT_EXPR(FALSE, L"This accessor type is not supported.");
        return DXGI_FORMAT_UNKNOWN;
    }
}

void gltf_model::fetch_meshes(ID3D11Device* device, const tinygltf::Model& gltf_model)
{
    HRESULT hr;
    size_t gltf_buffer_count = gltf_model.buffers.size();
    buffers.resize(gltf_buffer_count);
    for (size_t gltf_buffer_index = 0; gltf_buffer_index < gltf_buffer_count; ++gltf_buffer_index)
    {
        const tinygltf::Buffer& gltf_buffer = gltf_model.buffers.at(gltf_buffer_index);
        D3D11_BUFFER_DESC buffer_desc{};
        buffer_desc.ByteWidth = static_cast<UINT>(gltf_buffer.data.size());
        buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER | D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA subresource_data{};
        subresource_data.pSysMem = gltf_buffer.data.data();
        hr = device->CreateBuffer(&buffer_desc, &subresource_data, buffers.at(gltf_buffer_index).GetAddressOf());
        _ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
    }
    for (std::vector<tinygltf::Mesh>::const_reference gltf_mesh : gltf_model.meshes)
    {
        mesh& mesh{ meshes.emplace_back() };
        mesh.name = gltf_mesh.name;
        for (std::vector<tinygltf::Primitive>::const_reference gltf_primitive : gltf_mesh.primitives)
        {
            mesh::primitive& primitive{ mesh.primitives.emplace_back() };
            primitive.material = gltf_primitive.material;
            if (gltf_primitive.indices > -1)
            {
                const tinygltf::Accessor& gltf_accessor{ gltf_model.accessors.at(gltf_primitive.indices) };
                const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };
                primitive.index_buffer_view.format = convert_format(gltf_accessor);
                primitive.index_buffer_view.buffer = gltf_buffer_view.buffer;
                primitive.index_buffer_view.stride_in_bytes = gltf_accessor.ByteStride(gltf_buffer_view);
                primitive.index_buffer_view.byte_offset = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
                primitive.index_buffer_view.count = gltf_accessor.count;
            }
            for (std::map<std::string, int>::const_reference gltf_attribute : gltf_primitive.attributes)
            {
                const tinygltf::Accessor& gltf_accessor{ gltf_model.accessors.at(gltf_attribute.second) };
                const tinygltf::BufferView& gltf_buffer_view{ gltf_model.bufferViews.at(gltf_accessor.bufferView) };
                buffer_view vertex_buffer_view{};
                vertex_buffer_view.format = convert_format(gltf_accessor);
                vertex_buffer_view.buffer = gltf_buffer_view.buffer;
                vertex_buffer_view.stride_in_bytes = gltf_accessor.ByteStride(gltf_buffer_view);
                vertex_buffer_view.byte_offset = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
                vertex_buffer_view.count = gltf_accessor.count;
                primitive.vertex_buffer_views.emplace(std::make_pair(gltf_attribute.first, vertex_buffer_view));
            }
        }
    }
}
