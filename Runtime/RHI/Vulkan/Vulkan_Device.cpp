/*
Copyright(c) 2016-2019 Panos Karabelas

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

//= IMPLEMENTATION ===============
#include "../RHI_Implementation.h"
#include "Vulkan_Helper.h"
#ifdef API_GRAPHICS_VULKAN 
//================================

//= INCLUDES ==================
#include "../RHI_Device.h"
#include "../../Math/Vector4.h"
#include "../../Logging/Log.h"
#include "../../Core/Settings.h"
#include <string>
//=============================

//= NAMESPACES ================
using namespace std;
using namespace Directus::Math;
//=============================

namespace Directus
{
	RHI_Device::RHI_Device()
	{
		m_rhi_context = new RHI_Context();

		// Create instance
		VkApplicationInfo app_info	= {};
		{
			app_info.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
			app_info.pApplicationName	= ENGINE_VERSION;		
			app_info.pEngineName		= ENGINE_VERSION;
			app_info.engineVersion		= VK_MAKE_VERSION(1, 0, 0);
			app_info.applicationVersion	= VK_MAKE_VERSION(1, 0, 0);
			app_info.apiVersion			= VK_API_VERSION_1_1;

			VkInstanceCreateInfo create_info	= {};
			create_info.sType					= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			create_info.pApplicationInfo		= &app_info;
			create_info.enabledExtensionCount	= static_cast<uint32_t>(m_rhi_context->extensions_device_physical.size());
			create_info.ppEnabledExtensionNames	= m_rhi_context->extensions_device_physical.data();
			create_info.enabledLayerCount		= 0;

			if (m_rhi_context->validation_enabled)
			{
				if (vulkan_helper::check_validation_layers(m_rhi_context))
				{
					create_info.enabledLayerCount	= static_cast<uint32_t>(m_rhi_context->validation_layers.size());
					create_info.ppEnabledLayerNames = m_rhi_context->validation_layers.data();
				}
				else
				{
					LOG_ERROR("Validation layer was requested, but not available.");
				}
			}

			if (vkCreateInstance(&create_info, nullptr, &m_rhi_context->instance) != VK_SUCCESS)
			{
				LOG_ERROR("Failed to create instance.");
				return;
			}
		}

		// Callback
		if (m_rhi_context->validation_enabled)
		{
			VkDebugUtilsMessengerCreateInfoEXT create_info	= {};
			create_info.sType								= VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			create_info.messageSeverity						= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			create_info.messageType							= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			create_info.pfnUserCallback						= vulkan_helper::debug_callback::callback;

			if (vulkan_helper::debug_callback::create(m_rhi_context, &create_info) != VK_SUCCESS)
			{
				LOG_ERROR("Failed to setup debug callback");
			}
		}

		// Device Physical
		{
			uint32_t device_count = 0;
			vkEnumeratePhysicalDevices(m_rhi_context->instance, &device_count, nullptr);
			if (device_count == 0) 
			{
				LOG_ERROR("Failed to enumerate physical devices.");
				return;
			}
			std::vector<VkPhysicalDevice> physical_devices(device_count);
			vkEnumeratePhysicalDevices(m_rhi_context->instance, &device_count, physical_devices.data());
			
			if (!vulkan_helper::physical_device::choose(m_rhi_context, physical_devices)) 
			{
				LOG_ERROR("Failed to find a suitable device.");
				return;
			}
		}

		// Device
		VkPhysicalDeviceFeatures device_features = {};
		VkDeviceCreateInfo create_info = {};
		{
			VkDeviceQueueCreateInfo queue_create_info	= {};
			queue_create_info.sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info.queueFamilyIndex			= m_rhi_context->indices.graphics_family.value();
			queue_create_info.queueCount				= 1;

			auto queue_priority = 1.0f;
			queue_create_info.pQueuePriorities = &queue_priority;

			create_info.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			create_info.pQueueCreateInfos		= &queue_create_info;
			create_info.queueCreateInfoCount	= 1;
			create_info.pEnabledFeatures		= &device_features;
			create_info.enabledExtensionCount	= static_cast<uint32_t>(m_rhi_context->extensions_device.size());
			create_info.ppEnabledExtensionNames = m_rhi_context->extensions_device.data();

			if (m_rhi_context->validation_enabled)
			{
			    create_info.enabledLayerCount	= static_cast<uint32_t>(m_rhi_context->validation_layers.size());
			    create_info.ppEnabledLayerNames	= m_rhi_context->validation_layers.data();
			}
			else 
			{
			    create_info.enabledLayerCount = 0;
			}

			if (vkCreateDevice(m_rhi_context->device_physical, &create_info, nullptr, &m_rhi_context->device) != VK_SUCCESS) 
			{
				LOG_ERROR("Failed to create device.");
			}
		}

		// Present Queue
		{
			vector<VkDeviceQueueCreateInfo> queue_create_infos;
			set<uint32_t> unique_queue_families = { m_rhi_context->indices.graphics_family.value(), m_rhi_context->indices.present_family.value() };
			auto queue_priority = 1.0f;
			for (auto queue_family : unique_queue_families) 
			{
				 VkDeviceQueueCreateInfo queue_create_info = {};
				 queue_create_info.sType				= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				 queue_create_info.queueFamilyIndex		= queue_family;
				 queue_create_info.queueCount			= 1;
				 queue_create_info.pQueuePriorities		= &queue_priority;
				 queue_create_infos.push_back(queue_create_info);
			}

			create_info.queueCreateInfoCount	= static_cast<uint32_t>(queue_create_infos.size());
			create_info.pQueueCreateInfos		= queue_create_infos.data();

			vkGetDeviceQueue(m_rhi_context->device, m_rhi_context->indices.present_family.value(), 0, &m_rhi_context->present_queue);
		}

		auto version_major	= to_string(VK_VERSION_MAJOR(app_info.apiVersion));
		auto version_minor	= to_string(VK_VERSION_MINOR(app_info.apiVersion));
		auto version_path	= to_string(VK_VERSION_PATCH(app_info.apiVersion));
		Settings::Get().m_versionGraphicsAPI = "Vulkan " + version_major + "." + version_minor + "." + version_path;
		LOG_INFO(Settings::Get().m_versionGraphicsAPI);
		m_initialized = true;
	}

	RHI_Device::~RHI_Device()
	{	
		if (m_rhi_context->validation_enabled)
		{
			vulkan_helper::debug_callback::destroy(m_rhi_context);
		}
		vkDestroyInstance(m_rhi_context->instance, nullptr);
		vkDestroyDevice(m_rhi_context->device, nullptr);
		safe_delete(m_rhi_context);
	}

	bool RHI_Device::Draw(unsigned int vertex_count) const
	{
		return true;
	}

	bool RHI_Device::DrawIndexed(const unsigned int index_count, const unsigned int index_offset, const unsigned int vertex_offset) const
	{
		return true;
	}

	bool RHI_Device::ClearRenderTarget(void* render_target, const Vector4& color) const
	{
		return true;
	}

	bool RHI_Device::ClearDepthStencil(void* depth_stencil, const unsigned int flags, const float depth, const unsigned int stencil) const
	{
		return true;
	}

	bool RHI_Device::SetVertexBuffer(const RHI_VertexBuffer* buffer) const
	{
		return true;
	}

	bool RHI_Device::SetIndexBuffer(const RHI_IndexBuffer* buffer) const
	{
		return true;
	}

	bool RHI_Device::SetVertexShader(const RHI_Shader* shader) const
	{
		return true;
	}

	bool RHI_Device::SetPixelShader(const RHI_Shader* shader) const
	{
		return true;
	}

	bool RHI_Device::SetConstantBuffers(unsigned int start_slot, unsigned int buffer_count, const void* buffer, RHI_Buffer_Scope scope) const
	{
		return true;
	}

	bool RHI_Device::SetSamplers(unsigned int start_slot, unsigned int sampler_count, const void* samplers) const
	{
		return true;
	}

	bool RHI_Device::SetRenderTargets(unsigned int render_target_count, const void* render_targets, void* depth_stencil) const
	{
		return true;
	}

	bool RHI_Device::SetTextures(unsigned int start_slot, unsigned int resource_count, const void* textures) const
	{
		return true;
	}

	bool RHI_Device::SetViewport(const RHI_Viewport& viewport) const
	{
		return true;
	}

	bool RHI_Device::SetScissorRectangle(const Math::Rectangle& rectangle) const
	{
		return true;
	}

	bool RHI_Device::SetDepthStencilState(const RHI_DepthStencilState* depth_stencil_state) const
	{
		return true;
	}

	bool RHI_Device::SetBlendState(const RHI_BlendState* blend_state) const
	{
		return true;
	}

	bool RHI_Device::SetPrimitiveTopology(const RHI_PrimitiveTopology_Mode primitive_topology) const
	{
		return true;
	}

	bool RHI_Device::SetInputLayout(const RHI_InputLayout* input_layout) const
	{
		return true;
	}

	bool RHI_Device::SetRasterizerState(const RHI_RasterizerState* rasterizer_state) const
	{
		return true;
	}

	void RHI_Device::BeginMarker(const std::string& name)
	{

	}

	void RHI_Device::EndMarker()
	{

	}

	bool RHI_Device::ProfilingCreateQuery(void** query, const RHI_Query_Type type) const
	{
		return true;
	}

	bool RHI_Device::ProfilingQueryStart(void* query_object) const
	{
		return true;
	}

	bool RHI_Device::ProfilingGetTimeStamp(void* query_object) const
	{
		return true;
	}

	float RHI_Device::ProfilingGetDuration(void* query_disjoint, void* query_start, void* query_end) const
	{
		return 0.0f;
	}

	void RHI_Device::ProfilingReleaseQuery(void* query_object)
	{

	}

	void RHI_Device::WaitIdle()
	{
		vkDeviceWaitIdle(m_rhi_context->device);
	}
}
#endif