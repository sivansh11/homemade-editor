#include "pipeline.hpp"

#include "core/core.hpp"
#include "core/log.hpp"

#include <shaderc/shaderc.hpp>
#include <shaderc/glslc/src/file_includer.h>
#include <shaderc/libshaderc_util/include/libshaderc_util/file_finder.h>

#include <fstream>

namespace gfx::vulkan {

namespace utils {

static std::vector<char> read_file(const std::filesystem::path& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        ERROR("Failed to open file {}", filename.string());
        std::terminate();
    }

    size_t file_size = (size_t) file.tellg();
    std::vector<char> buffer(file_size);

    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return buffer;
}

} // namespace utils

VkShaderModule load_shader_module(core::ref<gfx::vulkan::context_t> context, const std::filesystem::path& shader_path) {
    auto code = utils::read_file(shader_path);
    
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = code.size();
    shader_module_create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shader_module;
    auto result = vkCreateShaderModule(context->device(), &shader_module_create_info, nullptr, &shader_module);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create shader module");
        std::terminate();
    }
    return shader_module;
}

// in future return an optional
core::ref<shader_t> shader_builder_t::build(core::ref<gfx::vulkan::context_t> context, shader_type_t shader_type, const std::string& name, const std::string& code) {
    shaderc_shader_kind shaderc_kind{};
    if (shader_type == shader_type_t::e_vertex) shaderc_kind = shaderc_vertex_shader;
    if (shader_type == shader_type_t::e_fragment) shaderc_kind = shaderc_fragment_shader;
    if (shader_type == shader_type_t::e_compute) shaderc_kind = shaderc_compute_shader;
    if (shader_type == shader_type_t::e_geometry) shaderc_kind = shaderc_geometry_shader;

    static shaderc::Compiler shaderc_compiler{};
    static shaderc::CompileOptions shaderc_compile_options{};
    static shaderc_util::FileFinder file_finder;
    static bool once = []() {
        // shaderc_compile_options.SetOptimizationLevel(shaderc_optimization_level_performance);
        shaderc_compile_options.SetOptimizationLevel(shaderc_optimization_level_zero);
        shaderc_compile_options.SetGenerateDebugInfo();
        shaderc_compile_options.SetIncluder(std::make_unique<glslc::FileIncluder>(&file_finder));
        return true;
    }();

    auto preprocess = shaderc_compiler.PreprocessGlsl(code, shaderc_kind, name.c_str(), shaderc_compile_options);
    std::string preprocessed_code = { preprocess.begin(), preprocess.end() };

    auto shader_module = shaderc_compiler.CompileGlslToSpv(preprocessed_code, shaderc_kind, name.c_str(), shaderc_compile_options);
    if (shader_module.GetCompilationStatus() != shaderc_compilation_status_success) {
        ERROR("{}", shader_module.GetErrorMessage());
        std::terminate();
    }
    return core::make_ref<shader_t>(context, std::vector<uint32_t>{shader_module.begin(), shader_module.end()}, name, shader_type);
}   

shader_t::shader_t(core::ref<context_t> context, const std::vector<uint32_t>& shader_module_code, const std::string& name, shader_type_t shader_type) 
  : _context(context),
    _shader_type(shader_type),
    _name(name) {
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = shader_module_code.size() * 4;
    shader_module_create_info.pCode = shader_module_code.data();

    auto result = vkCreateShaderModule(context->device(), &shader_module_create_info, nullptr, &_shader_module);
    if (result != VK_SUCCESS) {
        ERROR("Failed to create shader module");
        std::terminate();
    }
    INFO("Created shader module {}", _name);
}

shader_t::~shader_t() {
    vkDestroyShaderModule(_context->device(), _shader_module, nullptr);
    INFO("Destroyed shader module {}", _name);
}

// pipeline_builder_t& pipeline_builder_t::
pipeline_builder_t::pipeline_builder_t() {
    pipeline_depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipeline_depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    pipeline_depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
    pipeline_depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
    pipeline_depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    pipeline_depth_stencil_state_create_info.minDepthBounds = 0.0f; // Optional
    pipeline_depth_stencil_state_create_info.maxDepthBounds = 1.0f; // Optional
    pipeline_depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;
    pipeline_depth_stencil_state_create_info.front = {}; // Optional
    pipeline_depth_stencil_state_create_info.back = {}; // Optional
}

pipeline_builder_t& pipeline_builder_t::add_shader(const std::filesystem::path& shader_path) {
    shader_paths.push_back(shader_path);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_dynamic_state(VkDynamicState state) {
    dynamic_states.push_back(state);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_descriptor_set_layout(core::ref<descriptor_set_layout_t> descriptor_set_layout) {
    assert(descriptor_set_layout);
    descriptor_set_layouts.push_back(descriptor_set_layout->descriptor_set_layout());
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_push_constant_range(uint64_t offset, uint64_t size, VkShaderStageFlags shader_stage_flag) {
    VkPushConstantRange push_constant_range{};
    push_constant_range.offset = offset;
    push_constant_range.size = size;
    push_constant_range.stageFlags = shader_stage_flag;
    push_constant_ranges.push_back(push_constant_range);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_default_color_blend_attachment_state() {
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    pipeline_color_blend_attachment_states.push_back(color_blend_attachment);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_color_blend_attachment_state(const VkPipelineColorBlendAttachmentState& pipeline_color_blend_attachmen_state) {
    pipeline_color_blend_attachment_states.push_back(pipeline_color_blend_attachmen_state);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_depth_stencil_state(const VkPipelineDepthStencilStateCreateInfo& pipeline_depth_stencil_state) {
    pipeline_builder_t::pipeline_depth_stencil_state_create_info = pipeline_depth_stencil_state;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_polygon_mode(const VkPolygonMode& polygon_mode) {
    pipeline_builder_t::polygon_mode = polygon_mode;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_vertex_input_binding_description(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    VkVertexInputBindingDescription vertex_input_binding_description{};
    vertex_input_binding_description.binding = binding;
    vertex_input_binding_description.stride = stride;
    vertex_input_binding_description.inputRate = input_rate;
    vertex_input_binding_descriptions.push_back(vertex_input_binding_description);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::add_vertex_input_attribute_description(uint32_t binding, uint32_t location, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription vertex_input_attribute_description{};
    vertex_input_attribute_description.binding = binding;
    vertex_input_attribute_description.format = format;
    vertex_input_attribute_description.location = location;
    vertex_input_attribute_description.offset = offset;
    vertex_input_attribute_descriptions.push_back(vertex_input_attribute_description);
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_vertex_input_binding_description_vector(const std::vector<VkVertexInputBindingDescription>& val) {
    vertex_input_binding_descriptions = val;
    return *this;
}

pipeline_builder_t& pipeline_builder_t::set_vertex_input_attribute_description_vector(const std::vector<VkVertexInputAttributeDescription>& val) {
    vertex_input_attribute_descriptions = val;
    return *this;
}   

core::ref<pipeline_t> pipeline_builder_t::build(core::ref<context_t> context, VkRenderPass renderpass) {
    std::vector<VkPipelineShaderStageCreateInfo> pipeline_shader_stage_create_infos{};
    std::vector<core::ref<shader_t>> shaders;

    VkPipelineBindPoint pipeline_bind_point;

    for (auto& shader_path : shader_paths) {
        auto code = utils::read_file(shader_path);

        VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info{};
        pipeline_shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stage_create_info.pName = "main";

        shader_type_t shader_type;
        // todo: make the whole finding thing more robust (only look at the extension not the whole path)
        if (shader_path.string().find("vert") != std::string::npos) {
            pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_type = shader_type_t::e_vertex;
            // TODO: make this more robust
            pipeline_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        } else if (shader_path.string().find("frag") != std::string::npos) {
            pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_type = shader_type_t::e_fragment;
        } else if (shader_path.string().find("comp") != std::string::npos) {
            pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            shader_type = shader_type_t::e_compute;
            // TODO: make this more robust
            pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        }
        // if (shader_path.string().find("rchit") != std::string::npos) {
        //     pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        //     pipeline_bind_point = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        // }
        // if (shader_path.string().find("rgen") != std::string::npos) {
        //     pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        // }
        // if (shader_path.string().find("rmiss") != std::string::npos) {
        //     pipeline_shader_stage_create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        // }
        
        core::ref<shader_t> shader = shader_builder_t{}
            .build(context, shader_type, shader_path.string(), {code.begin(), code.end()});

        pipeline_shader_stage_create_info.module = shader->shader_module();        

        // VkShaderModuleCreateInfo shader_module_create_info{};
        // shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        // shader_module_create_info.codeSize = code.size();
        // shader_module_create_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

        // VkShaderModule shader_module{};
        // if (vkCreateShaderModule(context->device(), &shader_module_create_info, nullptr, &shader_module) != VK_SUCCESS) {
        //     ERROR("Failed to create shader module");
        //     std::terminate();
        // }

        

        
        shaders.push_back(shader);
        pipeline_shader_stage_create_infos.push_back(pipeline_shader_stage_create_info);
    }

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = descriptor_set_layouts.size();
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
    pipeline_layout_info.pushConstantRangeCount = push_constant_ranges.size();
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

    if (vkCreatePipelineLayout(context->device(), &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    if (pipeline_bind_point == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR) {
        
        // hard coded
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> raytracing_shader_group_create_infos_KHR = {
            {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = NULL,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                .generalShader = VK_SHADER_UNUSED_KHR,
                .closestHitShader = 0,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = NULL
            },
            {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = NULL,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = 1,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = NULL
            },
            {
                .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = NULL,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = 2,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = NULL
            },
            {   .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                .pNext = NULL,
                .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                .generalShader = 3,
                .closestHitShader = VK_SHADER_UNUSED_KHR,
                .anyHitShader = VK_SHADER_UNUSED_KHR,
                .intersectionShader = VK_SHADER_UNUSED_KHR,
                .pShaderGroupCaptureReplayHandle = NULL
            }
        };
        VkRayTracingPipelineCreateInfoKHR raytracing_pipeline_create_info_KHR{};
        raytracing_pipeline_create_info_KHR.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        raytracing_pipeline_create_info_KHR.pNext = NULL;
        raytracing_pipeline_create_info_KHR.flags = 0;
        raytracing_pipeline_create_info_KHR.stageCount = 4;
        raytracing_pipeline_create_info_KHR.pStages = pipeline_shader_stage_create_infos.data();
        raytracing_pipeline_create_info_KHR.groupCount = 4;
        raytracing_pipeline_create_info_KHR.pGroups = raytracing_shader_group_create_infos_KHR.data();
        raytracing_pipeline_create_info_KHR.maxPipelineRayRecursionDepth = 1;
        raytracing_pipeline_create_info_KHR.pLibraryInfo = NULL;
        raytracing_pipeline_create_info_KHR.pLibraryInterface = NULL;
        raytracing_pipeline_create_info_KHR.pDynamicState = NULL;
        raytracing_pipeline_create_info_KHR.layout = pipeline_layout;
        raytracing_pipeline_create_info_KHR.basePipelineHandle = VK_NULL_HANDLE;
        raytracing_pipeline_create_info_KHR.basePipelineIndex = 0;
        
        auto result = vkCreateRayTracingPipelinesKHR(context->device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &raytracing_pipeline_create_info_KHR, NULL, &pipeline);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create raytracing pipeline!");
        }
    } else if (pipeline_bind_point == VK_PIPELINE_BIND_POINT_GRAPHICS) {
        if (renderpass == VK_NULL_HANDLE) {
            ERROR("didnt provide a renderpass");
            std::terminate();
        }
        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = reinterpret_cast<VkDynamicState *>(dynamic_states.data());

        VkPipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.vertexBindingDescriptionCount = vertex_input_binding_descriptions.size();
        vertex_input_info.pVertexBindingDescriptions = vertex_input_binding_descriptions.data();
        vertex_input_info.vertexAttributeDescriptionCount = vertex_input_attribute_descriptions.size();
        vertex_input_info.pVertexAttributeDescriptions = vertex_input_attribute_descriptions.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) context->swapchain_extent().width;
        viewport.height = (float) context->swapchain_extent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = context->swapchain_extent();

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1;
        viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = polygon_mode;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        VkPipelineColorBlendStateCreateInfo color_blending{};
        color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending.logicOpEnable = VK_FALSE;
        color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blending.attachmentCount = pipeline_color_blend_attachment_states.size();
        color_blending.pAttachments = pipeline_color_blend_attachment_states.data();
        color_blending.blendConstants[0] = 0.0f; // Optional
        color_blending.blendConstants[1] = 0.0f; // Optional
        color_blending.blendConstants[2] = 0.0f; // Optional
        color_blending.blendConstants[3] = 0.0f; // Optional

        VkPipelineDepthStencilStateCreateInfo depth_stencil = pipeline_depth_stencil_state_create_info;

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = static_cast<uint32_t>(pipeline_shader_stage_create_infos.size());
        pipeline_info.pStages = pipeline_shader_stage_create_infos.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multisampling;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blending;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.renderPass = renderpass;
        pipeline_info.subpass = 0;

        if (vkCreateGraphicsPipelines(context->device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    } else if (pipeline_bind_point == VK_PIPELINE_BIND_POINT_COMPUTE) {
        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.layout = pipeline_layout;
        pipeline_info.stage = *pipeline_shader_stage_create_infos.data();
        
        if (vkCreateComputePipelines(context->device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }
    }
    
    // for (auto pipeline_shader_stage_create_info : pipeline_shader_stage_create_infos) {
    //     vkDestroyShaderModule(context->device(), pipeline_shader_stage_create_info.module, nullptr);
    // }

    return core::make_ref<pipeline_t>(context, pipeline_layout, pipeline, pipeline_bind_point);
}

pipeline_t::pipeline_t(core::ref<context_t> context, VkPipelineLayout pipeline_layout, VkPipeline pipeline, VkPipelineBindPoint pipeline_bind_point) 
  : _context(context),
    _pipeline_layout(pipeline_layout),
    _pipeline(pipeline),
    _pipeline_bind_point(pipeline_bind_point) {
    INFO("Created pipeline");
}

pipeline_t::~pipeline_t() {
    vkDestroyPipelineLayout(_context->device(), _pipeline_layout, nullptr);
    vkDestroyPipeline(_context->device(), _pipeline, nullptr);
}

void pipeline_t::bind(VkCommandBuffer commandbuffer) {
    vkCmdBindPipeline(commandbuffer, _pipeline_bind_point, _pipeline);
}

} // namespace gfx::vulkan
