#include <metal_stdlib>
#include <metal_atomic>
#include <metal_texture>

using namespace metal;

struct VertexIn {
    float4 position [[attribute(0)]];
};

struct FragmentIn {
    float4 position [[position]];
};

struct SceneUniforms {
    float4x4 projection_view;
    packed_float3 camera_position;
    float _0pad;
};

struct MeshUniforms {
    float4 color;
    float4x4 model;
};

struct Node {
    float4 color;
    float depth;
    uint next;
};

// == GATHER ==

vertex auto vgather(
    constant SceneUniforms& scene_uniforms [[buffer(2)]],
    constant MeshUniforms& mesh_uniforms   [[buffer(3)]],
    VertexIn in                            [[stage_in]]
) -> FragmentIn {
    return FragmentIn {
        .position = scene_uniforms.projection_view * mesh_uniforms.model * in.position
    };
}

fragment auto fgather(
    texture2d<uint, access::read_write> heads  [[texture(0)]],
    device atomic_uint& counter           [[buffer(0)]],
    device Node* nodes                    [[buffer(1)]],
    constant MeshUniforms& mesh_uniforms  [[buffer(3)]],
    FragmentIn in                         [[stage_in]]
) -> void {
    const uint2 pixel = uint2(in.position.xy);
    const uint index  = atomic_fetch_add_explicit(&counter, 1i, memory_order_relaxed);
    const uint old    = heads.atomic_exchange(pixel, uint4(index)).r;

    nodes[index].color = mesh_uniforms.color;
    nodes[index].depth = in.position.z;
    nodes[index].next  = old;
}

// == BLEND ==

constant uint INVALID = 0xFFFFFFFFu;
constant uint MAX_NODES = 8u;

vertex auto vblend(uint vertex_id [[vertex_id]]) -> FragmentIn {
    const float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0),
    };

    return FragmentIn{
        .position = float4(positions[vertex_id], 0.0, 1.0)
    };
}

float4 sorted_color(
    uint first_index,
    const device Node* nodes,
    uint node_capacity
) {
    Node sorted[MAX_NODES];
    uint index = first_index;
    uint node_count = 0u;

    while (
        index != INVALID &&
        index < node_capacity &&
        node_count < MAX_NODES
    ) {
        const Node node = nodes[index];
        uint insertion = node_count;

        while (
            insertion > 0u &&
            sorted[insertion - 1u].depth > node.depth
        ) {
            sorted[insertion] = sorted[insertion - 1u];
            --insertion;
        }

        sorted[insertion] = node;
        ++node_count;
        index = node.next;
    }

    float4 color = float4(0.0);

    for (uint i = node_count; i > 0u; --i) {
        const Node node = sorted[i - 1u];
        const float remaining = 1.0 - node.color.a;

        color.rgb = node.color.rgb * node.color.a
                  + color.rgb * remaining;
        color.a = node.color.a + color.a * remaining;
    }

    return color;
}

fragment auto fblend(
    FragmentIn in                       [[stage_in]],
    texture2d<uint, access::read> heads [[texture(0)]],
    constant uint& node_capacity        [[buffer(0)]],
    const device Node* nodes            [[buffer(1)]]
) -> float4 {
    const uint2 pixel = uint2(in.position.xy);
    const uint first_index = heads.read(pixel).r;

    return sorted_color(first_index, nodes, node_capacity);
}
