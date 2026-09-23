#include <metal_stdlib>
#include <metal_atomic>
#include <metal_texture>

using namespace metal;

constant uint INVALID = 0xFFFFFFFFu;
constant uint MAX_NODES = 8u;

struct VertexIn {
    float4 position [[attribute(0)]];
};

struct FragmentIn {
    float4 position [[position]];
};

struct SceneUniforms {
    float4x4 projection_view;
    packed_float3 camera_position;
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
    device atomic_uint& counter                [[buffer(0)]],
    device Node* nodes                         [[buffer(1)]],
    constant MeshUniforms& mesh_uniforms       [[buffer(3)]],
    FragmentIn in                              [[stage_in]]
) -> void {
    // helper threads shouldnt aollocate nodes so ignore please
    if (simd_is_helper_thread()) {
        return;
    }

    const uint2 pixel   = uint2(in.position.xy);

    // basically returns size of subgroup
    const uint count  = simd_sum(1u);
    // gives each thread a unique value starting from 1
    // works by summing 1u for each thread before this one
    const uint offset = simd_prefix_exclusive_sum(1u);

    uint base = 0u;
    // elect the big boss thread and reserve a block of indices at once
    // also returns the first index reserved
    if (simd_is_first()) {
        base = atomic_fetch_add_explicit(&counter, count, memory_order_relaxed);
    }

    // this will give every thread the base index, which we can add their unique number to :D
    base = simd_broadcast_first(base);

    // check if we over buffer size, in which case we gotta stop
    const uint capacity = MAX_NODES * heads.get_width() * heads.get_height();
    const uint index = base + offset;
    if (index >= capacity) {
        return;
    }

    const uint old   = heads.atomic_exchange(pixel, uint4(index)).r;

    nodes[index].color = mesh_uniforms.color;
    nodes[index].depth = in.position.z;
    nodes[index].next  = old;
}

// == BLEND ==


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
    float4 color = float4(0.0);

    bool has_upper = false;
    float upper_depth = 0.0;
    uint upper_index = 0u;

    while (true) {
        uint index = first_index;

        uint best_index = INVALID;
        float best_depth = -INFINITY;

        uint steps = 0u;

        while (
            index != INVALID and
            index < node_capacity and
            steps < node_capacity
        ) {
            const Node node = nodes[index];

            bool below_upper =
                !has_upper or
                node.depth < upper_depth or
                (node.depth == upper_depth and index < upper_index);

            bool better =
                best_index == INVALID or
                node.depth > best_depth or
                (node.depth == best_depth and index > best_index);

            if (below_upper and better) {
                best_index = index;
                best_depth = node.depth;
            }

            index = node.next;
            ++steps;
        }

        if (best_index == INVALID) {
            break;
        }

        const Node node = nodes[best_index];

        const float remaining = 1.0 - node.color.a;

        color.rgb =
            node.color.rgb * node.color.a +
            color.rgb * remaining;

        color.a =
            node.color.a +
            color.a * remaining;

        upper_depth = node.depth;
        upper_index = best_index;
        has_upper = true;
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
