#include "renderEngine.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}


void RenderEngine::init(){


	// Inicijalizacija SDL prozora
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	_window = SDL_CreateWindow(
		"Glavni prozor",         //window title
		SDL_WINDOWPOS_UNDEFINED, //window position x
		SDL_WINDOWPOS_UNDEFINED, //window position y
		_windowExtent.width,     //window width in pixels
		_windowExtent.height,    //window height in pixels
		window_flags
	);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	// Inicijalizacija Vulkan instance i dohvaæanje grafièkog procesora
	init_vulkan();

	// Postavljanje GPU memorije za spremanje struktura potrebnih sjenèaru
	allocate_compute_buffers();
	allocate_compute_images();
	
	

	// Postavljanje struktura za opis grafièkog protoènog sustava, uèitavanje sjenèara i opis podataka
	init_compute_descriptors();
	init_compute_pipelines();

	// Postavljanje struktura za prikaz slike
	init_swapchain();


	// Struktura za prijenos GPU naredbi
	init_command_buffers();

	
	// Sinkronizacijske strukture
	init_sync_structures();


	init_render_pass();
	init_framebuffers();
	init_imgui();

}


void RenderEngine::init_vulkan(){


	vkb::InstanceBuilder builder;

	vkb::Result<vkb::Instance> instance = builder.set_app_name("Vulkan Renderer")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build();


	vkb::Instance vkb_instance = instance.value();


	// Spremanje instance kako bi se mogla korisiti i poèistiti poslije
	_instance = vkb_instance.instance;
	_debug_messenger = vkb_instance.debug_messenger;

	// Dohvaæanje SDL prozora
	SDL_Vulkan_CreateSurface(_window, _instance, &_window_surface);

	VkPhysicalDeviceDescriptorIndexingFeatures descriptorFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES, 
		VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,
		VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE,VK_FALSE};
	descriptorFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	descriptorFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptorFeatures.pNext = nullptr;

	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.shaderFloat64 = VK_TRUE;

	// Biranje grafièkog procesora
	vkb::PhysicalDeviceSelector selector{ vkb_instance };
	vkb::PhysicalDevice physicalDevice = selector
		.add_required_extension("VK_KHR_swapchain")
		.add_required_extension("VK_EXT_descriptor_indexing")
		.add_required_extension_features<VkPhysicalDeviceDescriptorIndexingFeatures>(descriptorFeatures)
		.set_required_features(deviceFeatures)
		.set_minimum_version(1, 1)
		.set_surface(_window_surface)
		.select()
		.value();



	// Prijenos fizièkog opisnika u logièki
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	vkb::Device vkbDevice = deviceBuilder.build().value();

	_device = vkbDevice.device;
	_physical_GPU = physicalDevice.physical_device;


	VkPhysicalDeviceProperties GPU_info;
	vkGetPhysicalDeviceProperties(_physical_GPU, &GPU_info);
	std::cout << "Koristeni graficki procesor: " << GPU_info.deviceName << "\n";

	// Biranje naredbenih redova
	std::vector<VkQueueFamilyProperties> queue_families = physicalDevice.get_queue_families();
	std::vector<vkb::CustomQueueDescription> graphics_queue_descriptions;
	std::vector<vkb::CustomQueueDescription> compute_queue_descriptions;

	for (uint32_t i = 0; i < static_cast<uint32_t>(queue_families.size()); i++) {
		if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

			graphics_queue_descriptions.push_back(vkb::CustomQueueDescription(
				i, queue_families[i].queueCount, std::vector<float>(queue_families[i].queueCount, 1.0f)));
		}
		if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {

			compute_queue_descriptions.push_back(vkb::CustomQueueDescription(
				i, queue_families[i].queueCount, std::vector<float>(queue_families[i].queueCount, 1.0f)));
		}
	}
	if (compute_queue_descriptions.size() == 0) {
		std::cerr << "Izabrani GPU uredaj nema redove za komputaciju. Program æe se sada unistiti\n";
		return;
	}


	// Biranje prvog reda koji podržava grafièke naredbe
	vkGetDeviceQueue(vkbDevice.device, graphics_queue_descriptions[0].index, 0, &_graphics_queue);
	_graphics_queue_family = graphics_queue_descriptions[0].index;
	// I komputacijske
	vkGetDeviceQueue(vkbDevice.device, compute_queue_descriptions[0].index, 0, &_compute_queue);
	_compute_queue_family = compute_queue_descriptions[0].index;


	// Initialize the memory allocator
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = _physical_GPU;
	allocatorInfo.device = _device;
	allocatorInfo.instance = _instance;
	vmaCreateAllocator(&allocatorInfo, &_allocator);

	_main_deletion_queue.push_function([=]() {
		vmaDestroyAllocator(_allocator);
		});

	_frames = new Frame[_max_frames_in_flight];
}

void RenderEngine::init_imgui(){
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForVulkan(_window);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = _instance;
	init_info.PhysicalDevice = _physical_GPU;
	init_info.Device = _device;
	init_info.QueueFamily = _graphics_queue_family;
	init_info.Queue = _graphics_queue;
	init_info.DescriptorPoolSize = 16;
	init_info.Subpass = 0;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.CheckVkResultFn = check_vk_result;
	init_info.RenderPass = _renderPass;
	ImGui_ImplVulkan_Init(&init_info);



	ImGui::PushFontSize(18.0f);



	_main_deletion_queue.push_function([=]() {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		
		});

}


void RenderEngine::allocate_compute_buffers(){

	// Alociraju se 2 uniformna spremnika


	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = _max_uniform_memory / 2;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;


	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	for (int i = 0; i < _max_frames_in_flight; i++){
		VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
			&_frames[i]._camera_uniform_buffer._buffer,
			&_frames[i]._camera_uniform_buffer._allocation,
			nullptr));

		VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
			&_frames[i]._atmosphere_uniform_buffer._buffer,
			&_frames[i]._atmosphere_uniform_buffer._allocation,
			nullptr));

		_main_deletion_queue.push_function([=]() {
			vmaDestroyBuffer(_allocator, _frames[i]._camera_uniform_buffer._buffer, _frames[i]._camera_uniform_buffer._allocation);
			vmaDestroyBuffer(_allocator, _frames[i]._atmosphere_uniform_buffer._buffer, _frames[i]._atmosphere_uniform_buffer._allocation);
			});
	}


}
void RenderEngine::allocate_compute_images(){

	// Alociranje rezultantne slike komputacijskog sjenèara
	VmaAllocationCreateInfo vmaallocInfo = {};


	VkImageCreateInfo imageCinfo = {};
	imageCinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCinfo.pNext = nullptr;
	imageCinfo.arrayLayers = 1;
	imageCinfo.flags = 0;
	imageCinfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCinfo.imageType = VK_IMAGE_TYPE_2D;
	imageCinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCinfo.mipLevels = 1;
	imageCinfo.pQueueFamilyIndices = &(_compute_queue_family, _graphics_queue_family);
	imageCinfo.queueFamilyIndexCount = 2;
	imageCinfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCinfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	imageCinfo.extent = { _screen_size_x, _screen_size_y, 1 };



	for (unsigned int i = 0; i < _max_frames_in_flight; i++) {

		imageCinfo.pQueueFamilyIndices = &(_compute_queue_family, _graphics_queue_family);
		imageCinfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCinfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageCinfo.extent = { _screen_size_x, _screen_size_y, 1 };

		VK_CHECK(vmaCreateImage(_allocator, &imageCinfo, &vmaallocInfo,
			&_frames[i]._output_image._image,
			&_frames[i]._output_image._allocation,
			nullptr));


		_image_deletion_queue.push_function([=]() {
			vmaDestroyImage(_allocator, _frames[i]._output_image._image, _frames[i]._output_image._allocation);
			});

		VkImageViewCreateInfo viewCInfo = {};
		viewCInfo.image = _frames[i]._output_image._image;
		viewCInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCInfo.pNext = nullptr;
		viewCInfo.flags = 0;
		viewCInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewCInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1 };

		VK_CHECK(vkCreateImageView(_device, &viewCInfo, nullptr, &_frames[i]._output_image_view));

		_image_deletion_queue.push_function([=]() {
			vkDestroyImageView(_device, _frames[i]._output_image_view, nullptr);
			});

	}
}
void RenderEngine::init_compute_descriptors(){

	// Opisnik 1. uniformnog spremnika
	VkDescriptorSetLayoutBinding computeBinding0 = {};
	computeBinding0.binding = 0;
	computeBinding0.descriptorCount = 1;
	computeBinding0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	computeBinding0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	// 2.
	VkDescriptorSetLayoutBinding computeBinding1 = computeBinding0;
	computeBinding1.binding = 1;
	computeBinding1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	// Opisnik slike
	VkDescriptorSetLayoutBinding computeBinding2 = computeBinding0;
	computeBinding2.binding = 2;
	computeBinding2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;


	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingInfo;
	bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingInfo.pNext = nullptr;
	VkDescriptorBindingFlags flags[3] = {0,
										 0,
										 0};
	bindingInfo.pBindingFlags = flags;
	bindingInfo.bindingCount = 3;

	// Opisnik cijelog seta
	VkDescriptorSetLayoutCreateInfo setinfo = {};
	setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setinfo.pNext = &bindingInfo;

	setinfo.bindingCount = 3;
	setinfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

	VkDescriptorSetLayoutBinding bindings[3] = { computeBinding0, computeBinding1, computeBinding2};
	setinfo.pBindings = bindings;

	vkCreateDescriptorSetLayout(_device, &setinfo, nullptr, &_compute_set_layout);

	_main_deletion_queue.push_function([=]() {
		vkDestroyDescriptorSetLayout(_device, _compute_set_layout, nullptr);
		});


	
	std::vector<VkDescriptorPoolSize> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2*_max_frames_in_flight },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1*_max_frames_in_flight }
	};


	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pool_info.maxSets = _max_frames_in_flight;
	pool_info.poolSizeCount = (uint32_t)sizes.size();
	pool_info.pPoolSizes = sizes.data();

	vkCreateDescriptorPool(_device, &pool_info, nullptr, &_compute_pool);

	_main_deletion_queue.push_function([=]() {
		vkDestroyDescriptorPool(_device, _compute_pool, nullptr);
		});

	for (unsigned int i = 0; i < _max_frames_in_flight; i++) {    

		// Prosljeðivanje informacija opisnicima
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _compute_pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &_compute_set_layout;

		vkAllocateDescriptorSets(_device, &allocInfo, &_frames[i]._compute_descriptor_set);

		// Alokacija uniformnih opisnika
		VkDescriptorBufferInfo binfo = {};
		binfo.buffer = _frames[i]._camera_uniform_buffer._buffer;
		binfo.offset = 0;
		binfo.range = sizeof(shader_input_buffer_1);

		VkWriteDescriptorSet setWrite = {};
		setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		setWrite.pNext = nullptr;

		// Pristupa se s 0. bindingom u sjenèaru
		setWrite.dstBinding = 0;
		setWrite.dstSet = _frames[i]._compute_descriptor_set;
		setWrite.descriptorCount = 1;
		setWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setWrite.pBufferInfo = &binfo;

		vkUpdateDescriptorSets(_device, 1, &setWrite, 0, nullptr);

		// Nakon Update naredbe, strukti binfo i setwrite mogu se ponovno iskoristiti za opisivanje sljedeæeg spremnika
		binfo.buffer = _frames[i]._atmosphere_uniform_buffer._buffer;
		binfo.offset = 0;
		binfo.range = sizeof(shader_input_buffer_1);

		// Pristupa se s 1. bindingom u sjenèaru
		setWrite.dstBinding = 1;
		setWrite.dstSet = _frames[i]._compute_descriptor_set;
		setWrite.descriptorCount = 1;
		setWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setWrite.pBufferInfo = &binfo;
		vkUpdateDescriptorSets(_device, 1, &setWrite, 0, nullptr);


		// Alokacija opisnika slike
		VkDescriptorImageInfo iinfo = {};
		iinfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		iinfo.imageView = _frames[i]._output_image_view;
		iinfo.sampler = nullptr;

		setWrite.dstBinding = 2;
		setWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		setWrite.pImageInfo = &iinfo;
		setWrite.pBufferInfo = nullptr;
		vkUpdateDescriptorSets(_device, 1, &setWrite, 0, nullptr);
		

	}
}



void RenderEngine::init_render_pass(){

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = _swapchain_image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VK_CHECK(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass));


	_main_deletion_queue.push_function([=]() {
		vkDestroyRenderPass(_device, _renderPass, nullptr);
		});


}


bool RenderEngine::load_shader_module(const char* filePath, VkShaderModule* outShaderModule)
{
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return false;
	}

	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	size_t fileSize = (size_t)file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beginning
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char*)buffer.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();


	//create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	//codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;

}

void RenderEngine::init_compute_pipelines(){

	// Uèitavanje kompilirane datoteke sjenèara
	VkShaderModule mainShader;
	char shaderName1[] = "./src/shaders/main_shader.spv";
	if (!load_shader_module(shaderName1, &mainShader)) {
		std::cerr << "Glavni komputacijski sjencar ('" << shaderName1 << "') nije uspio biti ucitan :(\n";
	}
	else {
		std::cout << "Uspjesno ucitan glavni komputacijski sjencar\n";
	}

	
	PipelineBuilder pipelineBuilder;

	// Gradnja protoène strukture
	VkPipelineLayoutCreateInfo compute_pipeline_layout_info;
	compute_pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	compute_pipeline_layout_info.pNext = nullptr;
	compute_pipeline_layout_info.flags = 0;
	compute_pipeline_layout_info.setLayoutCount = 0;
	compute_pipeline_layout_info.pushConstantRangeCount = 0;
	compute_pipeline_layout_info.pPushConstantRanges = nullptr;
	compute_pipeline_layout_info.setLayoutCount = 1;
	compute_pipeline_layout_info.pSetLayouts = &_compute_set_layout;


	VK_CHECK(vkCreatePipelineLayout(_device, &compute_pipeline_layout_info, nullptr, &_compute_pipeline_Layout));


	pipelineBuilder._pipelineLayout = _compute_pipeline_Layout;


	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;

	info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	info.module = mainShader;
	info.pName = "main";

	pipelineBuilder._shaderStages.push_back(info);
	_default_compute_pipeline = pipelineBuilder.build_compute_pipeline(_device);


	// Èišæenje sjenèara - nakon što je dodan u protok, može se odmah izbrisati
	vkDestroyShaderModule(_device, mainShader, nullptr);


	_main_deletion_queue.push_function([=]() {
		vkDestroyPipeline(_device, _default_compute_pipeline, nullptr);

		vkDestroyPipelineLayout(_device, _compute_pipeline_Layout, nullptr);
		});

}

VkPipeline RenderEngine::PipelineBuilder::build_compute_pipeline(VkDevice device) {


	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = _shaderStages.data()[0];
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;


	VkPipeline newPipeline;
	if (vkCreateComputePipelines(
		device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		std::cerr << "Greska pri stvaranju protocnog sustava\n";
		return VK_NULL_HANDLE;
	}
	else
	{
		return newPipeline;
	}
}


void RenderEngine::init_swapchain(){
	vkb::SwapchainBuilder swapchainBuilder{ _physical_GPU, _device, _window_surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()

		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		// Puno istodobnih frameova može uzrokovati latenciju
		.set_desired_min_image_count(_max_frames_in_flight)

		.set_desired_extent(_windowExtent.width, _windowExtent.height)
		// Traženi format i moguænosti slike
		.set_desired_format({VK_FORMAT_R32G32B32A32_SFLOAT, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
		.set_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		.build()
		.value();


	// Spremanje swapchaina
	_swapchain = vkbSwapchain.swapchain;
	_swapchain_extent = vkbSwapchain.extent;
	_swapchain_images = vkbSwapchain.get_images().value();
	_swapchain_image_views = vkbSwapchain.get_image_views().value(); 

	_swapchain_image_format = vkbSwapchain.image_format;

	// Dodavanje funkcija za èišæenje
	_swapchain_deletion_queue.push_function([=]() {
		vkDestroySwapchainKHR(_device, _swapchain, nullptr);
		});

	for (int i = 0; i < _swapchain_images.size(); i++) {

		_swapchain_deletion_queue.push_function([=]() {
			vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
			});
	}
}


void RenderEngine::init_framebuffers(){

	_framebuffers.resize(_swapchain_images.size());

	for (size_t i = 0; i < _swapchain_image_views.size(); i++) {
		VkImageView attachments[] = {
			_swapchain_image_views[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = _renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = _swapchain_extent.width;
		framebufferInfo.height = _swapchain_extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}

		_main_deletion_queue.push_function([=]() {
			vkDestroyFramebuffer(_device, _framebuffers[i], nullptr);
			});
	}



}

void RenderEngine::init_command_buffers(){

	// "Bazen" (pool) za alokaciju komandnih spremnika - ovaj je za komputacijske naredbe
	VkCommandPoolCreateInfo commandPoolInfo_compute;
	commandPoolInfo_compute.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo_compute.pNext = nullptr;

	commandPoolInfo_compute.queueFamilyIndex = _compute_queue_family;
	commandPoolInfo_compute.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	// Za grafièke naredbe
	VkCommandPoolCreateInfo commandPoolInfo_graphics;
	commandPoolInfo_graphics.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo_graphics.pNext = nullptr;

	commandPoolInfo_graphics.queueFamilyIndex = _graphics_queue_family;
	commandPoolInfo_graphics.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;


	// Alociranje komandnih spremnika iz bazena
	for (unsigned int i = 0; i < _max_frames_in_flight; i++) {

		// Komputacijski
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo_compute, nullptr, &_frames[i]._compute_command_pool));

		VkCommandBufferAllocateInfo cmdAllocInfo_compute;
		cmdAllocInfo_compute.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo_compute.pNext = nullptr;

		cmdAllocInfo_compute.commandPool =_frames[i]._compute_command_pool;
		cmdAllocInfo_compute.commandBufferCount = 1;
		cmdAllocInfo_compute.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo_compute, &_frames[i]._compute_command_buffer));

		_main_deletion_queue.push_function([=]() {
			vkFreeCommandBuffers(_device, _frames[i]._compute_command_pool, 1, &_frames[i]._compute_command_buffer);
			vkDestroyCommandPool(_device, _frames[i]._compute_command_pool, nullptr);
			});


		// Grafièki (za prikazivanje slika nakon izraèuna)
		VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo_graphics, nullptr, &_frames[i]._graphics_command_pool));

		VkCommandBufferAllocateInfo cmdAllocInfo_graphics;
		cmdAllocInfo_graphics.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdAllocInfo_graphics.pNext = nullptr;

		cmdAllocInfo_graphics.commandPool =_frames[i]._graphics_command_pool;
		cmdAllocInfo_graphics.commandBufferCount = 1;
		cmdAllocInfo_graphics.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;


		VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo_graphics, &_frames[i]._graphics_command_buffer));

		_main_deletion_queue.push_function([=]() {
			vkFreeCommandBuffers(_device, _frames[i]._graphics_command_pool, 1, &_frames[i]._graphics_command_buffer);
			vkDestroyCommandPool(_device, _frames[i]._graphics_command_pool, nullptr);
			});
	}


}


void RenderEngine::init_sync_structures(){

	VkFenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	// Signalna zastava omoguæuje èekanje da ograda bude dovršena prije korištenja GPU naredbe po prvi put

	VkSemaphoreCreateInfo semaphoreCreateInfo;
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	for (unsigned int i = 0; i < _max_frames_in_flight; i++) {    


		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._compute_fence));
		VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._gui_fence));

		_main_deletion_queue.push_function([=]() {
			vkDestroyFence(_device, _frames[i]._compute_fence, nullptr);
			vkDestroyFence(_device, _frames[i]._gui_fence, nullptr);
			});


		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._compute_finish_semaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._gui_finish_semaphore));
		VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._present_semaphore));


		_main_deletion_queue.push_function([=]() {
			vkDestroySemaphore(_device, _frames[i]._compute_finish_semaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._gui_finish_semaphore, nullptr);
			vkDestroySemaphore(_device, _frames[i]._present_semaphore, nullptr);

			});

	}

}






void RenderEngine::run(){


	while (!should_quit){
	

		// Procesiranje korisnièkog inputa
		handle_input();

		process_movement();

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		show_gui();


		if (prev_frame_atmosphere.surface_pressure_pa != main_planet.atmosphere.surface_pressure_pa || 
			prev_frame_atmosphere.temperature != main_planet.atmosphere.temperature || 
			prev_frame_atmosphere.molar_mass != main_planet.atmosphere.molar_mass || 
			prev_frame_atmosphere.refractivity != main_planet.atmosphere.refractivity
			){
			recalculate_K();
		
		}


		ImGui::Render();
		compute();

		prev_frame_atmosphere = main_planet.atmosphere;

	}


}

void RenderEngine::recalculate_K(){

	double surface_density = main_planet.atmosphere.surface_pressure_pa /(main_planet.atmosphere.temperature * (8.3144621 /*plinska konstanta*/  / main_planet.atmosphere.molar_mass));
	double surface_mass = surface_density * (1*1*1);
	double num_air_particles_2 = (surface_mass / main_planet.atmosphere.molar_mass)  * (6.02214076 * pow(10,23)) /*Avogadrova konstanta*/;
	double surface_molecule_density = num_air_particles_2 / (1*1*1);

	double pi = 3.141592654;
	K = 2 * pi * pi * (main_planet.atmosphere.refractivity*main_planet.atmosphere.refractivity - 1) * (main_planet.atmosphere.refractivity*main_planet.atmosphere.refractivity - 1) / surface_molecule_density / 3.0f;

}

void RenderEngine::show_gui(){

	if (!_config_mode){
		if (!_hide_GUI){
			ImGui::Begin("Opis", 0, 
				ImGuiWindowFlags_NoBackground | 
				ImGuiWindowFlags_AlwaysAutoResize | 
				ImGuiWindowFlags_NoCollapse | 
				ImGuiWindowFlags_NoDecoration | 
				ImGuiWindowFlags_NoMove

			);
			const ImVec2 pos {0,0};
		
			ImGui::SetWindowPos("Opis", pos);
			const ImVec4 textCol {1,1,1,1};
			ImGui::TextColored(textCol, "Pritisnite \"ESC\" za mijenjanje parametara, ili \"F1\" za skrivanje/pokazivanje ovog teksta ");
		
			ImGui::End();
		}
	}
	else{
		ImGui::Begin("Kontrole", 0, 
			ImGuiWindowFlags_AlwaysAutoResize 
		);

		ImGui::SeparatorText("Kontrole simulacije");

		ImGui::SliderFloat("Brzina kamere", &camera_speed, 0.025,  100, "%.3f", ImGuiSliderFlags_Logarithmic);

		ImGui::InputInt("Broj iteracija zrake", &sample_amount_in,1,10);
		ImGui::InputInt("Broj iteracija tlaka", &sample_amount_out,1,10);

		ImGui::Checkbox("Rayleigh simulacija", &do_rayleigh);
		ImGui::Checkbox("Aerosolna Mie simulacija", &do_mie);


		ImGui::SeparatorText("Kontrole planeta");
		ImGui::InputFloat("Radijus planeta", &main_planet.radius, 1000, 1000 * 100, "%.0f");
		//ImGui::SliderFloat("Radijus planeta", &main_planet.radius, 6371 * 500,  6371 * 2000, "%.0f", ImGuiSliderFlags_Logarithmic);
		
		ImGui::SeparatorText("Kontrole atmosfere");
		ImGui::SliderFloat("Povrsinski tlak (pa)", &main_planet.atmosphere.surface_pressure_pa, 101325/16.0f,  101325*4.0f, "%.0f", ImGuiSliderFlags_Logarithmic);		
		ImGui::SliderFloat("Visina prosjecne gustoce zraka (m)", &main_planet.atmosphere.average_density_height, 100,  20000, "%.0f");
		ImGui::SliderFloat("Visina prosjecne gustoce aerosola", &main_planet.atmosphere.average_density_height_aerosol, 100,  20000, "%.0f");

		ImGui::SliderFloat("Relativna kolicina aerosola", &aerosol_density_mul, 0.0f,  20.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Mie asimetrija", &main_planet.atmosphere.mie_asymmetry_const, -0.99,  0.99, "%.3f");

		ImGui::SliderFloat("Gornja granica atmosfere", &main_planet.atmosphere.upper_limit, 100 * 1000,  100 * 10000, "%.0f");


		ImGui::SeparatorText("Kontrole Sunca");
		ImGui::SliderFloat("Udaljenost", &sun.distance, 1.496f*powf(10, 4)*1000,  1.496f*powf(10, 10)*1000, "%.0f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Velicina", &sun.radius, 1391400.0f,  1391400.0f * 100000, "%.0f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Kut", &sun.angle, 0.0f,  360.0f, "%.0f");

		ImGui::SeparatorText("Kontrole svjetla");

		ImGui::SliderFloat("Intenzitet svjetlosti", &light_intensity, 0.0f,  200.0f, "%.2f");
		ImGui::SliderFloat3("Boja svjetlosti", &light_color[0], 0.0f,  1.0f, "%.2f");

		ImGui::SliderFloat("Valna duljina crvene komponente (nm)", &r_wavelen, 10,  2000, "%.2f");
		ImGui::SliderFloat("Valna duljina zelene komponente (nm)", &g_wavelen, 10,  2000, "%.2f");
		ImGui::SliderFloat("Valna duljina plave komponente (nm)", &b_wavelen, 10,  2000, "%.2f");


		ImGui::End();
		

		ImGui::Begin("Upute", 0, 
			ImGuiWindowFlags_AlwaysAutoResize | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_NoDecoration | 
			ImGuiWindowFlags_NoMove
		);
		const ImVec2 pos2 {5,750};

		ImGui::SetWindowPos("Upute", pos2);

		ImGui::Text("Opis koristenja:");
		ImGui::Text("Kretnje misem - okretanje kamere");
		ImGui::Text("WASD, Q/E - pomicanje kamere");
		ImGui::Text("+/- na numpadu - pomicanje sunca");

		ImGui::Text("F1 - skrivanje GUI-a izvan ovog moda");
		ImGui::Text("ESC - ulaz/izlaz i konfiguracijskog moda (ovog)");

		ImGui::End();

	}

	

}


void RenderEngine::handle_input(){


	SDL_Event e;
	while ((SDL_PollEvent(&e) != 0) && !should_quit)
	{

		ImGui_ImplSDL2_ProcessEvent(&e);



		if (e.type == SDL_QUIT){

		}

		switch (e.type)
		{
		// Zatvaranje procesa pri kliku na X gumb ili alt-f4
		case (SDL_QUIT):
		{
			should_quit = true;
		} break;
		case (SDL_KEYDOWN):
		{
			switch (e.key.keysym.sym)
			{
			case (SDLK_KP_PLUS):
			{
				sun_movement = 1;

			} break;
			case (SDLK_KP_MINUS):
			{
				sun_movement = -1;
			} break;

			default:
				break;
			}

			switch (e.key.keysym.scancode)
			{
			case (SDL_SCANCODE_W):
			{
				main_camera_movement.v_z = -1;
			} break;
			case (SDL_SCANCODE_A):
			{
				main_camera_movement.v_x = -1;
			} break;
			case (SDL_SCANCODE_S):
			{
				main_camera_movement.v_z = 1;
			} break;
			case (SDL_SCANCODE_D):
			{
				main_camera_movement.v_x = 1;
			} break;

			case (SDL_SCANCODE_Q):
			{
				main_camera_movement.v_y = 1;
			} break;
			case (SDL_SCANCODE_E):
			{
				main_camera_movement.v_y = -1;
			} break;



			case (SDL_SCANCODE_ESCAPE):
			{
				_config_mode = !_config_mode;

				if (_config_mode){
					SDL_SetRelativeMouseMode(SDL_FALSE);
				}
				else{	
					SDL_SetRelativeMouseMode(SDL_TRUE);
				}


			} break;
			case (SDL_SCANCODE_F1):
			{
				_hide_GUI = !_hide_GUI;

			} break;

			default:
				break;
			}



			break;
		}
		case (SDL_KEYUP):
		{
			switch (e.key.keysym.sym)
			{
			case (SDLK_KP_PLUS):
			{
				if (sun_movement == 1) sun_movement = 0;

			} break;
			case (SDLK_KP_MINUS):
			{
				if (sun_movement == -1) sun_movement = 0;
			} break;

			default:
				break;
			}


			switch (e.key.keysym.scancode)
			{
			case (SDL_SCANCODE_W):
			{
				if (main_camera_movement.v_z == -1) main_camera_movement.v_z = 0;
			} break;
			case (SDL_SCANCODE_A):
			{
				if (main_camera_movement.v_x == -1) main_camera_movement.v_x = 0;
			} break;
			case (SDL_SCANCODE_S):
			{
				if (main_camera_movement.v_z == 1) main_camera_movement.v_z= 0;
			} break;
			case (SDL_SCANCODE_D):
			{
				if (main_camera_movement.v_x == 1) main_camera_movement.v_x = 0;
			} break;

			case (SDL_SCANCODE_Q):
			{
				if (main_camera_movement.v_y == 1) main_camera_movement.v_y = 0;
			} break;
			case (SDL_SCANCODE_E):
			{
				if (main_camera_movement.v_y == -1) main_camera_movement.v_y = 0;
			} break;

			default:
				break;
			}
			
		} break;
		case (SDL_MOUSEMOTION):
		{
			if (!_config_mode){
				main_camera_movement.yaw -= e.motion.xrel / 1000.0f;
				main_camera_movement.pitch -= e.motion.yrel / 1000.0f;
			}	
		} break;
		default:
			break;
		}

	}


}

void RenderEngine::process_movement(){
	main_camera.front = glm::vec3(0.0f, 0.0f, 1.0f);
	main_camera.up = glm::vec3(0.0f, 1.0f, 0.0f);
	main_camera.right = glm::vec3(1.0f, 0.0f, 0.0f);

	glm::mat4 rotate_matrix = glm::mat4(
		glm::vec4(main_camera.right,0),
		glm::vec4(main_camera.up,0),
		glm::vec4(main_camera.front,0),
		glm::vec4(0,0,0,1)
	);

	rotate_matrix = glm::rotate(main_camera_movement.yaw, main_camera.up) * glm::rotate(main_camera_movement.pitch, main_camera.right);

	main_camera.right = rotate_matrix[0];
	main_camera.up = rotate_matrix[1];
	main_camera.front = rotate_matrix[2];


	main_camera.position += camera_speed * 750 * main_camera_movement.v_x * main_camera.right;
	main_camera.position += camera_speed * 750 * main_camera_movement.v_y * main_camera.up;
	main_camera.position += camera_speed * 750 * main_camera_movement.v_z * main_camera.front;

	sun.angle += sun_movement * 0.75f;
	while (sun.angle > 360) sun.angle -= 360;
	while (sun.angle < 0) sun.angle += 360;
}

void RenderEngine::compute(){

	// Nije potrebno ništa raditi ako je prozor minimiziran
	if (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED){
		return;
	}


	// Èekanje na dovršetak naredba prošle slike (tj. prošle X-te, gdje je X maksimalan broj bufferanih slika (obièno 1-3))
	VK_CHECK(vkWaitForFences(_device, 1, &_frames[_current_frame]._compute_fence, true, 1000000000));
	VK_CHECK(vkWaitForFences(_device, 1, &_frames[_current_frame]._gui_fence, true, 1000000000));

	// Postavljanje komandnog spremnika
	VK_CHECK(vkResetCommandBuffer(_frames[_current_frame]._compute_command_buffer, 0));



	// Traženje dohvata slike (u koju æe se output pisati) sa swapchaina, program maksimalno èeka 1 sekundu prije izlaska
	uint32_t swapchainImageIndex;
																					// Ovaj semafor signalizira kada je operacija gotova
	VkResult imageResult = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _frames[_current_frame]._present_semaphore, nullptr, &swapchainImageIndex);

	// Ukoliko slika ne odgovara swapchainu (najèešæe zbog nove velièine prozora), swapchain i prozor bi se trebali postaviti na toènu vrijednost
	if (imageResult == VK_ERROR_OUT_OF_DATE_KHR) {
		recreate_swapchain();
		return;
	}
	else if (imageResult != VK_SUCCESS && imageResult != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Nije bilo moguce dobiti sliku sa swapchaina :(");
	}

	// Postavljanje ogradi za naredbe
	VK_CHECK(vkResetFences(_device, 1, &_frames[_current_frame]._compute_fence));
	VK_CHECK(vkResetFences(_device, 1, &_frames[_current_frame]._gui_fence));


	// Raèunanje novih uniformnih podataka - u ovom sluèaju fiksna pozicija kamere
	shader_input_buffer_1 camera_input;
	
	camera_input.lookDir = glm::mat4(
		glm::vec4(main_camera.right,0),
		glm::vec4(main_camera.up,0),
		glm::vec4(main_camera.front,0),
		glm::vec4(0,0,0,1)
	);
	
	
	camera_input.initPos = glm::vec4(main_camera.position,0);
	camera_input.initDir = glm::vec4(0.0f, 0.0f, -0.51f, 0); // Kamera uvijek gleda relativno ispred sebe 

	camera_input.xPosMultiplier = 0;
	camera_input.yPosMultiplier = 0;

	camera_input.xDirMultiplier = 1.0f/1000.0f;
	camera_input.yDirMultiplier = 1.0f/1000.0f;

	camera_input.sampleAmount_in = sample_amount_in;
	camera_input.sampleAmount_out = sample_amount_out;

	camera_input.mode = 0;
	if (do_rayleigh) camera_input.mode |= 1;
	if (do_mie) camera_input.mode |= 2;

	// Prebacivanje uniformnih podataka na GPU
	void* data;
	vmaMapMemory(_allocator, _frames[_current_frame]._camera_uniform_buffer._allocation, &data);
	memcpy(data, &camera_input, sizeof(shader_input_buffer_1));
	vmaUnmapMemory(_allocator, _frames[_current_frame]._camera_uniform_buffer._allocation);


	shader_input_buffer_2 atmosphere_input;

	atmosphere_input.sun = sun;
	atmosphere_input.planet = main_planet;
	atmosphere_input.K = K;

	atmosphere_input.aerosol_density_mul = aerosol_density_mul;

	atmosphere_input.r_wavelen = r_wavelen;
	atmosphere_input.g_wavelen = g_wavelen;
	atmosphere_input.b_wavelen = b_wavelen;
	atmosphere_input.light_intensity = light_intensity;
	atmosphere_input.light_color = light_color;

	vmaMapMemory(_allocator, _frames[_current_frame]._atmosphere_uniform_buffer._allocation, &data);
	memcpy(data, &atmosphere_input, sizeof(shader_input_buffer_2));
	vmaUnmapMemory(_allocator, _frames[_current_frame]._atmosphere_uniform_buffer._allocation);

	// Postavljanje naredbenog spremnika
	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;

	cmdBeginInfo.pInheritanceInfo = nullptr;
	// Ovaj naredbeni spremnik koristit æe se samo jednom (po frameu)
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;


	// Poèetak spremanja grafièkih naredbi u spremnik
	VK_CHECK(vkBeginCommandBuffer(_frames[_current_frame]._compute_command_buffer, &cmdBeginInfo));

	// Postavljanje izlazne slike na generalno korištenje pomoæu barijere
	VkImageMemoryBarrier imageBarrier = {};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.pNext = NULL;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarrier.image = _frames[_current_frame]._output_image._image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;
	imageBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;

	VkPipelineStageFlagBits srcFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlagBits dstFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	vkCmdPipelineBarrier(_frames[_current_frame]._compute_command_buffer, srcFlags, dstFlags, 0, 0, NULL, 0, NULL, 1,
		&imageBarrier);


	// Izvršavanje komputacijskog sjenèara
	vkCmdBindPipeline(_frames[_current_frame]._compute_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, _default_compute_pipeline);
	vkCmdBindDescriptorSets(_frames[_current_frame]._compute_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, _compute_pipeline_Layout, 0, 1, &_frames[_current_frame]._compute_descriptor_set, 0, 0);
	vkCmdDispatch(_frames[_current_frame]._compute_command_buffer, _screen_size_x / 32 + 1, _screen_size_y / 32 + 1, 1);


	// Konverzija izlazne slike i swapchainove slike kako bi se podaci mogli kopirati s jedne na drugu
	VkImageCopy copyRegion{};

	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.srcSubresource.baseArrayLayer = 0;
	copyRegion.srcSubresource.layerCount = 1;

	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.dstSubresource.baseArrayLayer = 0;
	copyRegion.dstSubresource.layerCount = 1;

	copyRegion.srcOffset = {0,0,0};
	copyRegion.dstOffset = {0,0,0};
	copyRegion.extent = {
		_screen_size_x,
		_screen_size_y,
		1
	};


	VkImageMemoryBarrier imageBarrier2 = imageBarrier;
	imageBarrier2.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarrier2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imageBarrier2.image = _frames[_current_frame]._output_image._image;


	VkImageMemoryBarrier imageBarrier3 = imageBarrier;
	imageBarrier3.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier3.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier3.image = _swapchain_images[swapchainImageIndex];

	VkImageMemoryBarrier imageBarrierArray2[2] = {imageBarrier2, imageBarrier3};

	srcFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dstFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	vkCmdPipelineBarrier(_frames[_current_frame]._compute_command_buffer, srcFlags, dstFlags, 0, 0, NULL, 0, NULL, 2,
		imageBarrierArray2);

	// Kopiranje podataka na sliku swapchaina
	vkCmdCopyImage(_frames[_current_frame]._compute_command_buffer, _frames[_current_frame]._output_image._image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _swapchain_images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);






	// Pretvaranje slika natrag u njihov originalni format
	VkImageMemoryBarrier imageBarrier4 = imageBarrier;
	imageBarrier4.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier4.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageBarrier4.image = _swapchain_images[swapchainImageIndex];

	VkImageMemoryBarrier imageBarrier5 = imageBarrier;
	imageBarrier5.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imageBarrier5.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageBarrier5.image = _frames[_current_frame]._output_image._image;


	VkImageMemoryBarrier imageBarrierArray3[2] = {imageBarrier4, imageBarrier5};


	srcFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	dstFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	vkCmdPipelineBarrier(_frames[_current_frame]._compute_command_buffer, srcFlags, dstFlags, 0, 0, NULL, 0, NULL, 2,
		imageBarrierArray3);


	// Finalizacija komandog spremnika - sada se može slati na GPU za izvedbu
	VK_CHECK(vkEndCommandBuffer(_frames[_current_frame]._compute_command_buffer));

	// CRTANJE GUI-A

	// Postavljanje komandnog spremnika
	VK_CHECK(vkResetCommandBuffer(_frames[_current_frame]._graphics_command_buffer, 0));
	// Poèetak spremanja grafièkih naredbi u spremnik za grafiku
	VK_CHECK(vkBeginCommandBuffer(_frames[_current_frame]._graphics_command_buffer, &cmdBeginInfo));



	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = _renderPass;
	renderPassInfo.framebuffer = _framebuffers[swapchainImageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = _swapchain_extent;

	//VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
	renderPassInfo.clearValueCount = 0;
	//renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(_frames[_current_frame]._graphics_command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	ImDrawData* draw_data = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(draw_data, _frames[_current_frame]._graphics_command_buffer);

	vkCmdEndRenderPass(_frames[_current_frame]._graphics_command_buffer);




	VK_CHECK(vkEndCommandBuffer(_frames[_current_frame]._graphics_command_buffer));





	// Informacije o slanju
	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	submit.pWaitDstStageMask = &waitStage;
	// Èekanje na prezentacijski semafor - kada signalizira da je gotov, swapchain je omoguæio pisanje na sliku.
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_frames[_current_frame]._present_semaphore;
	// Ovaj set naredbi æe aktivirati _compute_finish_semaphore kada je gotov
	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_frames[_current_frame]._compute_finish_semaphore;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &_frames[_current_frame]._compute_command_buffer;

	// Slanje i izvršavanje naredbenog spremnika
	// _compute_fence æe sada blokirati daljnja slanja dok GPU nije gotov.
	VK_CHECK(vkQueueSubmit(_compute_queue, 1, &submit, _frames[_current_frame]._compute_fence));


	// Informacije o slanju crtanja GUI-a
	VkSubmitInfo submit_g = {};
	submit_g.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_g.pNext = nullptr;

	VkPipelineStageFlags waitStage_g = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	submit_g.pWaitDstStageMask = &waitStage_g;
	submit_g.waitSemaphoreCount = 1;
	submit_g.pWaitSemaphores = &_frames[_current_frame]._compute_finish_semaphore;
	submit_g.signalSemaphoreCount = 1;
	submit_g.pSignalSemaphores = &_frames[_current_frame]._gui_finish_semaphore;


	submit_g.commandBufferCount = 1;
	submit_g.pCommandBuffers = &_frames[_current_frame]._graphics_command_buffer;

	// Slanje i izvršavanje naredbenog spremnika
	// _compute_fence æe sada blokirati daljnja slanja dok GPU nije gotov.
	VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit_g, _frames[_current_frame]._gui_fence));


	// Priprema prezentacije na swapchain
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;
	// Èekanje na semafor (tj. da komputacijski sjenèar i kopiranje slika završe)
	presentInfo.pWaitSemaphores = &_frames[_current_frame]._gui_finish_semaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainImageIndex;
	VkResult presentResult = vkQueuePresentKHR(_graphics_queue, &presentInfo);


	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
		recreate_swapchain();
	}
	else if (presentResult != VK_SUCCESS) {
		throw std::runtime_error("Slika nije uspjela biti prezentirana na swapchain :(");
	}

	_current_frame = (_current_frame + 1) % _max_frames_in_flight;

}


void RenderEngine::cleanup(){
	
	// Èekanje dok GPU više ne korisiti strukture
	vkDeviceWaitIdle(_device);

	// Red je bitan - strukture ovise jedna o drugoj
	cleanup_swapchain();
	_image_deletion_queue.flush();
	_main_deletion_queue.flush();

	
	vkDestroyDevice(_device, nullptr);
	vkDestroySurfaceKHR(_instance, _window_surface, nullptr);
	vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
	vkDestroyInstance(_instance, nullptr);
	SDL_DestroyWindow(_window);

	delete[] _frames;
}

void RenderEngine::cleanup_swapchain(){
	_swapchain_deletion_queue.flush();
}

// Poziva se kada je potrebno promijeniti velièinu ili format swapchaina (obièno kada se prozor promijeni)
void RenderEngine::recreate_swapchain() {


	int new_screen_size_x;
	int new_screen_size_y;
	SDL_GetWindowSize(_window, &new_screen_size_x, &new_screen_size_y);

	if (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED) return; // Nije potrebno ništa raditi ako je prozor minimiziran



	vkDeviceWaitIdle(_device);
	cleanup_swapchain();

	_screen_size_x = new_screen_size_x;
	_screen_size_y = new_screen_size_y;

	_windowExtent.width = new_screen_size_x;
	_windowExtent.height = new_screen_size_y;

	// Realociranje slika
	_image_deletion_queue.flush();
	allocate_compute_images();


	// Osvježavanje opisnika slika
	for (unsigned int i = 0; i < _max_frames_in_flight; i++) {
		VkWriteDescriptorSet setWrite = {};
		setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		setWrite.pNext = nullptr;


		setWrite.dstBinding = 2;

		setWrite.dstSet = _frames[i]._compute_descriptor_set;
		setWrite.descriptorCount = 1;

		VkDescriptorImageInfo iinfo = {};
		iinfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		iinfo.imageView = _frames[i]._output_image_view;
		iinfo.sampler = nullptr;
		setWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		setWrite.pImageInfo = &iinfo;
		setWrite.pBufferInfo = nullptr;
		vkUpdateDescriptorSets(_device, 1, &setWrite, 0, nullptr);

	}

	init_swapchain();
}