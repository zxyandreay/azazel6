#include "psprecomp/common.hpp"
#include <iostream>
#ifdef PORTABLEKIT_HAS_RENDERER
#include "gpu/vulkan_renderer.hpp"
#include "clear_vertices.hpp"
#endif

int portablekit_renderer_contracts() {
#ifndef PORTABLEKIT_HAS_RENDERER
    throw psprecomp::Error("Renderer contract probe was not explicitly built with Vulkan");
#else
    using namespace portablekit::gpu;
    psprecomp::GuestMemory memory;
    constexpr auto vertices = 0x08004000u, target = 0x04000000u;
    // Fifteen independently authored stripes exercise actual packed-color
    // writes and row stride, so a blank window cannot satisfy this contract.
    for (std::uint32_t i = 0; i < 30; ++i) {
        const auto pair = i / 2, second = i & 1u, at = vertices + i * 12;
        memory.store32(at, pair & 1 ? 0xFF00FF00u : 0xFF0000FFu);
        memory.store16(at + 4, static_cast<std::uint16_t>((pair + second) * 32));
        memory.store16(at + 6, static_cast<std::uint16_t>(second * 272));
        memory.store16(at + 8, 16); memory.store16(at + 10, 0);
    }
    for (std::uint32_t y = 0; y < 272; ++y)
        for (std::uint32_t x = 0; x < 512; ++x) memory.store16(target + (y * 512 + x) * 2, 0xA55Au);
    DrawCall call{};
    call.vertices = azazel6::decode_clear_vertices(memory, vertices, 30, vertices, vertices + 360);
    call.primitive = PrimitiveType::Sprites; call.primitive_count = 30;
    call.vertex_type = 0x80011Cu; call.vertex_address = vertices;
    call.through = true; call.has_vertex_color = true;
    call.clear_mode = true; call.clear_flags = 7;
    call.target.color_address = target; call.target.color_stride = 512; call.target.color_format = 0;
    call.target.depth_address = 0x04088000u; call.target.depth_stride = 512;
    call.viewport.scissor_x2 = 1023; call.viewport.scissor_y2 = 1023;
    VulkanRenderer renderer;
    std::string error;
    if (!renderer.initialize({"Azazel6 bounded synthetic GE contract"}, error))
        throw psprecomp::Error("Vulkan initialization failed: " + error);
    renderer.begin_frame(); renderer.begin_display_list();
    renderer.submit(call, memory);
    renderer.read_back_framebuffer(target, memory);
    std::uint32_t checked = 0;
    for (std::uint32_t y = 0; y < 272; ++y) {
        for (std::uint32_t x = 0; x < 512; ++x) {
            const auto expected = x >= 480 ? 0xA55Au : ((x / 32) & 1 ? 0x07E0u : 0x001Fu);
            const auto actual = memory.load16(target + (y * 512 + x) * 2);
            if (actual != expected)
                throw psprecomp::Error("Vulkan stripe/readback mismatch at " + std::to_string(x) + "," +
                    std::to_string(y) + ": " + std::to_string(actual) + " != " + std::to_string(expected));
            ++checked;
        }
    }
    renderer.shutdown();
    std::cout << "Synthetic Vulkan GE clear/readback verified " << checked
              << " pixel and padding words; no game boot or depth readback claimed\n";
    return 0;
#endif
}
