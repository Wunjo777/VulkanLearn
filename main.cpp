#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

//validation layer list
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation" };//we can add more such as:"VK_LAYER_LUNARG_api_dump"
//device extension list
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT( VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger )
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkCreateDebugUtilsMessengerEXT" );
	if (func != nullptr)
	{
		return func( instance, pCreateInfo, pAllocator, pDebugMessenger );
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT( VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator )
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr( instance, "vkDestroyDebugUtilsMessengerEXT" );
	if (func != nullptr)
	{
		func( instance, debugMessenger, pAllocator );
	}
}

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication
{
public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	// physicalDevice将在销毁 VkInstance 时隐式销毁
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;//logical device
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkSwapchainKHR swapChain;
	//swap chain image handle
	//automatically destroyed after swap chain being destroyed
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkSemaphore imageAvailableSemaphore;//表示已从交换链获取图像并准备好进行渲染
	VkSemaphore renderFinishedSemaphore;//表示渲染已完成并且可以进行呈现
	VkFence inFlightFence;//确保一次只渲染一帧

	void initWindow()
	{
		glfwInit();
		// tell glfw do not use opengl
		glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
		glfwWindowHint( GLFW_RESIZABLE, GLFW_FALSE );

		window = glfwCreateWindow( WIDTH, HEIGHT, "Vulkan", nullptr, nullptr );
	}

	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffer();
		createSyncObjects();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose( window ))
		{
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle( device );
	}

	void cleanup()
	{
		vkDestroySemaphore( device, renderFinishedSemaphore, nullptr );
		vkDestroySemaphore( device, imageAvailableSemaphore, nullptr );
		vkDestroyFence( device, inFlightFence, nullptr );

		vkDestroyCommandPool( device, commandPool, nullptr );
		for (auto framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer( device, framebuffer, nullptr );
		}
		vkDestroyPipeline( device, graphicsPipeline, nullptr );
		vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
		vkDestroyRenderPass( device, renderPass, nullptr );

		for (auto imageView : swapChainImageViews)
		{
			vkDestroyImageView( device, imageView, nullptr );
		}
		vkDestroySwapchainKHR( device, swapChain, nullptr );
		vkDestroyDevice( device, nullptr );

		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT( instance, debugMessenger, nullptr );
		}

		vkDestroySurfaceKHR( instance, surface, nullptr );
		vkDestroyInstance( instance, nullptr );

		glfwDestroyWindow( window );
		//system( "pause" );
		glfwTerminate();
	}

	void createInstance()
	{
		// 检查验证层是否有效
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error( "validation layers requested, but not available!" );
		}
		// 填写appInfo和instanceInfo结构体
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		// 获取扩展
		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		// 为instance创建和销毁过程创建单独的debug messenger
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());//only one validation layer
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo( debugCreateInfo );
			// 通过将指向 VkDebugUtilsMessengerCreateInfoEXT 结构的指针传递到 VkInstanceCreateInfo 的 pNext 扩展字段中，
			// 为 vkCreateInstance 和 vkDestroyInstance 提供专门的调试支持
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;

			createInfo.pNext = nullptr;
		}
		// 创建Vulkan实例
		if (vkCreateInstance( &createInfo, nullptr, &instance ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create instance!" );
		}
	}

	void populateDebugMessengerCreateInfo( VkDebugUtilsMessengerCreateInfoEXT& createInfo )
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;
		// 填写debug messenger create info
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo( createInfo );
		// 创建debug messenger
		if (CreateDebugUtilsMessengerEXT( instance, &createInfo, nullptr, &debugMessenger ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to set up debug messenger!" );
		}
	}

	void createSurface()
	{
		if (glfwCreateWindowSurface( instance, window, nullptr, &surface ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create window surface!" );
		}
	}

	void pickPhysicalDevice()
	{
		// get available device number
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices( instance, &deviceCount, nullptr );

		if (deviceCount == 0)
		{
			throw std::runtime_error( "failed to find GPUs with Vulkan support!" );
		}
		// get available devices
		std::vector<VkPhysicalDevice> devices( deviceCount );
		vkEnumeratePhysicalDevices( instance, &deviceCount, devices.data() );
		// check if device suitable
		for (const auto& device : devices)
		{
			if (isDeviceSuitable( device ))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error( "failed to find a suitable GPU!" );
		}
	}

	void createLogicalDevice()
	{
		// 查找并筛选物理设备支持的队列族中哪些支持指定的特性
		QueueFamilyIndices indices = findQueueFamilies( physicalDevice );

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {
			indices.graphicsFamily.value(),
			indices.presentFamily.value()
		};

		float queuePriority = 1.0f;
		//populate queueCreateInfo for all queueFamilies.
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back( queueCreateInfo );
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		//populate logical device create info
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		//create logical device
		if (vkCreateDevice( physicalDevice, &createInfo, nullptr, &device ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create logical device!" );
		}
		//获得队列句柄
		vkGetDeviceQueue( device, indices.graphicsFamily.value(), 0, &graphicsQueue );
		vkGetDeviceQueue( device, indices.presentFamily.value(), 0, &presentQueue );
	}

	void createSwapChain()
	{
		//get swapChain support details
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport( physicalDevice );

		//choose the specified(the best) stuff
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat( swapChainSupport.formats );
		VkPresentModeKHR presentMode = chooseSwapPresentMode( swapChainSupport.presentModes );
		VkExtent2D extent = chooseSwapExtent( swapChainSupport.capabilities );

		//imageCount, i.e. buffer count in swapChain
		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		//限制imagecount小于最大imagecount
		//capabilities.maxImageCount==0时表示没有最大限制
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		//最终的image count由VULKAN驱动决定，程序员只需要限制image count的范围
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = findQueueFamilies( physicalDevice );
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;//指定数组内两个队列族共享所有权
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;//交换链中图像变换
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR( device, &createInfo, nullptr, &swapChain ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create swap chain!" );
		}
		//get swap chain image count
		vkGetSwapchainImagesKHR( device, swapChain, &imageCount, nullptr );
		swapChainImages.resize( imageCount );
		//get swap chain image handle
		vkGetSwapchainImagesKHR( device, swapChain, &imageCount, swapChainImages.data() );

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	void createImageViews()
	{
		//调整视图数量等于交换链图像数量
		swapChainImageViews.resize( swapChainImages.size() );
		//遍历所有交换链图像，填写create info
		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView( device, &createInfo, nullptr, &swapChainImageViews[i] ) != VK_SUCCESS)
			{
				throw std::runtime_error( "failed to create image views!" );
			}
		}
	}

	void createRenderPass()
	{
		//缓冲区附件
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		//指定颜色和深度数据该如何处理
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//渲染前
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//渲染后
		//指定模板数据该如何处理
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//渲染前
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//渲染后
		//指定内存中像素布局
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//渲染前
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//渲染后

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;//引用attachment discription
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//附件在使用此引用的子过程期间具有的布局

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef; //此数组中附件的索引直接从片段着色器使用 layout( location = 0 ) out vec4 outColor 指令引用！

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;//上一个执行的subpass，（渲染通道之前或之后的隐式子通道）
		dependency.dstSubpass = 0;//当前subpass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//需等待的操作发生的阶段
		dependency.srcAccessMask = 0;//需等待的操作（0表示任意操作，我们不关心）
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//当前subpass要执行的操作处于的阶段
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;//当前subpas要执行的操作

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass( device, &renderPassInfo, nullptr, &renderPass ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create render pass!" );
		}
	}

	void createGraphicsPipeline()
	{
		//管线可编程功能：
		auto vertShaderCode = readFile( "shaders/vert.spv" );
		auto fragShaderCode = readFile( "shaders/frag.spv" );

		VkShaderModule vertShaderModule = createShaderModule( vertShaderCode );
		VkShaderModule fragShaderModule = createShaderModule( fragShaderCode );

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;//告诉vulkan该shader插入管线的哪个阶段
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
		//管线固定功能：
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};//顶点输入（在vs硬编码了）
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};//输入汇编
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;//图元拓扑
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};//视口和裁剪矩形（动态状态）
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer{};//光栅化器
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;//确定如何为几何图形生成片元
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		//每个帧缓冲区的混合配置
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;//确定传递的通道
		colorBlendAttachment.blendEnable = VK_FALSE;
		//全局混合配置
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		if (vkCreatePipelineLayout( device, &pipelineLayoutInfo, nullptr, &pipelineLayout ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create pipeline layout!" );
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines( device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create graphics pipeline!" );
		}

		vkDestroyShaderModule( device, fragShaderModule, nullptr );
		vkDestroyShaderModule( device, vertShaderModule, nullptr );
	}

	void createFramebuffers()
	{
		swapChainFramebuffers.resize( swapChainImageViews.size() );

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = {
				swapChainImageViews[i]//来自交换链
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;//只能将帧缓冲与它兼容的渲染通道一起使用！
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer( device, &framebufferInfo, nullptr, &swapChainFramebuffers[i] ) != VK_SUCCESS)
			{
				throw std::runtime_error( "failed to create framebuffer!" );
			}
		}
	}

	void createCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies( physicalDevice );

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();//命令池绑定一个队列族

		if (vkCreateCommandPool( device, &poolInfo, nullptr, &commandPool ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create command pool!" );
		}
	}

	void createCommandBuffer()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers( device, &allocInfo, &commandBuffer ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to allocate command buffers!" );
		}
	}
	//把要执行的命令写入命令缓冲区
	void recordCommandBuffer( VkCommandBuffer commandBuffer, uint32_t imageIndex )//要写入的当前交换链图像的索引
	{
		//beginInfo指定有关此特定命令缓冲区用法的一些详细信息
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//判断是否可以开始记录命令缓冲的函数，返回VK_SUCCESS表示可以开始，其他返回值表示gpu内存不足等
		if (vkBeginCommandBuffer( commandBuffer, &beginInfo ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to begin recording command buffer!" );
		}
		//渲染通道的详细信息
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		VkClearValue clearColor = { {{0.5f, 0.5f, 0.5f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;
		//开始写入命令缓冲区（用于写入的函数以vkCmd开头）
		vkCmdBeginRenderPass( commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

		vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline );
		//动态状态的视口和裁剪矩形在此处设置
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport( commandBuffer, 0, 1, &viewport );

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor( commandBuffer, 0, 1, &scissor );
		
		vkCmdDraw( commandBuffer, 3, 1, 0, 0 );//显示发出一个draw call

		vkCmdEndRenderPass( commandBuffer );

		if (vkEndCommandBuffer( commandBuffer ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to record command buffer!" );
		}
	}

	void createSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;//创建处于已发出信号状态的信号量，确保第一帧能开始绘制

		if (vkCreateSemaphore( device, &semaphoreInfo, nullptr, &imageAvailableSemaphore ) != VK_SUCCESS ||
			 vkCreateSemaphore( device, &semaphoreInfo, nullptr, &renderFinishedSemaphore ) != VK_SUCCESS ||
			 vkCreateFence( device, &fenceInfo, nullptr, &inFlightFence ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create synchronization objects for a frame!" );
		}

	}

	void drawFrame()
	{
		vkWaitForFences( device, 1, &inFlightFence, VK_TRUE, UINT64_MAX );
		vkResetFences( device, 1, &inFlightFence );

		uint32_t imageIndex;//the return value from vkAcquireNextImageKHR()
		vkAcquireNextImageKHR( device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex );

		vkResetCommandBuffer( commandBuffer, /*VkCommandBufferResetFlagBits*/ 0 );
		recordCommandBuffer( commandBuffer, imageIndex );

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		//等待渲染semaphore
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };//与waitStage索引相对应
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		//发出呈现semaphore
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit( graphicsQueue, 1, &submitInfo, inFlightFence ) != VK_SUCCESS)//发出栅栏信号，表示命令缓冲区已可以重用
		{
			throw std::runtime_error( "failed to submit draw command buffer!" );
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR( presentQueue, &presentInfo );
	}

	VkShaderModule createShaderModule( const std::vector<char>& code )
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		//数据存储在 std::vector 中，其中默认分配器已经确保数据满足最坏情况下的对齐要求
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;//shader module句柄
		if (vkCreateShaderModule( device, &createInfo, nullptr, &shaderModule ) != VK_SUCCESS)
		{
			throw std::runtime_error( "failed to create shader module!" );
		}

		return shaderModule;
	}


	//color format and color space
	VkSurfaceFormatKHR chooseSwapSurfaceFormat( const std::vector<VkSurfaceFormatKHR>& availableFormats )
	{
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	//vsync
	VkPresentModeKHR chooseSwapPresentMode( const std::vector<VkPresentModeKHR>& availablePresentModes )
	{
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent( const VkSurfaceCapabilitiesKHR& capabilities )
	{
		//currentExtent:硬件提供的当前交换链推荐尺寸
		//max uint32_t 是一个特殊标记，表示推荐尺寸不适用，交换链尺寸由应用程序决定
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize( window, &width, &height );

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp( actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width );
			actualExtent.height = std::clamp( actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height );

			return actualExtent;
		}
	}


	SwapChainSupportDetails querySwapChainSupport( VkPhysicalDevice device )
	{
		SwapChainSupportDetails details;
		//populate basic surface functionalities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR( device, surface, &details.capabilities );
		//populate surface format(pixel format/color space)
		//get count
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, nullptr );
		//get data
		if (formatCount != 0)
		{
			details.formats.resize( formatCount );
			vkGetPhysicalDeviceSurfaceFormatsKHR( device, surface, &formatCount, details.formats.data() );
		}
		//populate present mode
		//get count
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, nullptr );
		//get data
		if (presentModeCount != 0)
		{
			details.presentModes.resize( presentModeCount );
			vkGetPhysicalDeviceSurfacePresentModesKHR( device, surface, &presentModeCount, details.presentModes.data() );
		}

		return details;
	}

	bool isDeviceSuitable( VkPhysicalDevice device )
	{
		QueueFamilyIndices indices = findQueueFamilies( device );

		bool extensionsSupported = checkDeviceExtensionSupport( device );

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			//populate SwapChainSupportDetails
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport( device );
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	bool checkDeviceExtensionSupport( VkPhysicalDevice device )
	{
		//get extension count
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );
		//get extension data
		std::vector<VkExtensionProperties> availableExtensions( extensionCount );
		vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, availableExtensions.data() );

		std::set<std::string> requiredExtensions( deviceExtensions.begin(), deviceExtensions.end() );

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase( extension.extensionName );
		}

		return requiredExtensions.empty();
	}

	QueueFamilyIndices findQueueFamilies( VkPhysicalDevice device )
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, nullptr );

		std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
		vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamilyCount, queueFamilies.data() );

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			//判断队列族是否支持绘图命令
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			//查找队列族索引i对应的队列族是否具有向窗口表面呈现能力
			vkGetPhysicalDeviceSurfaceSupportKHR( device, i, surface, &presentSupport );

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	std::vector<const char*> getRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );

		std::vector<const char*> extensions( glfwExtensions, glfwExtensions + glfwExtensionCount );

		if (enableValidationLayers)
		{
			extensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
		}

		return extensions;
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

		std::vector<VkLayerProperties> availableLayers( layerCount );
		vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp( layerName, layerProperties.layerName ) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	static std::vector<char> readFile( const std::string& filename )
	{
		std::ifstream file( filename, std::ios::ate | std::ios::binary );

		if (!file.is_open())
		{
			throw std::runtime_error( "failed to open file!" );
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer( fileSize );

		file.seekg( 0 );
		file.read( buffer.data(), fileSize );

		file.close();

		return buffer;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData )
	{
		// output error message
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}
};

int main()
{
	HelloTriangleApplication app;

	try
	{
		app.run();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		system( "pause" );
		return EXIT_FAILURE;
	}
	system( "pause" );
	return EXIT_SUCCESS;
}