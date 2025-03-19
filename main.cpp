#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
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
		createGraphicsPipeline();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose( window ))
		{
			glfwPollEvents();
		}
	}

	void cleanup()
	{
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
		system( "pause" );
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

	void createGraphicsPipeline()
	{

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
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}