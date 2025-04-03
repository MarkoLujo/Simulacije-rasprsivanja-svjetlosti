#pragma once

#include "../third-party/VkBootstrap.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vma/vk_mem_alloc.h>

#include <functional>
#include <iostream>
#include <fstream>

#include "camera.h"
#include "../simulation/sun.h"

#define VK_CHECK(x)														\
	{																	\
		VkResult err = x;												\
		if (err)														\
		{																\
			std::cout <<"Detected Vulkan error: " << err << std::endl;	\
			abort();													\
		}																\
	}

struct AllocatedBuffer {
	VkBuffer _buffer;
	VmaAllocation _allocation;
};

struct AllocatedImage {
	VkImage _image;
	VmaAllocation _allocation;
};


// Služi za èuvanje funkcija brisanja Vulkanovih objekata kako bi se mogle izvršiti toènim redoslijedom pri èišæenju
struct DeletionQueue
{
	std::vector<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// Izvršavanje svih funkcija redom
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); // Poziv
		}

		deletors.clear();
	}
};


struct Frame {

	// Bufferi za GPU podatke
	AllocatedBuffer _camera_uniform_buffer;
	AllocatedBuffer _atmosphere_uniform_buffer;

	AllocatedImage _output_image;
	VkImageView _output_image_view;

	// Naredbeni spreminici i njihovi alokacijski bazeni
	VkCommandPool _compute_command_pool;
	VkCommandBuffer _compute_command_buffer;

	VkCommandPool _graphics_command_pool;
	VkCommandBuffer _graphics_command_buffer;

	// Opisnik svih spremnika u pipelineu
	VkDescriptorSet _compute_descriptor_set;

	VkFence _compute_fence;
	VkSemaphore _compute_finish_semaphore;
	VkSemaphore _present_semaphore;
};


// Odgovara¸1. strukturi koju glavni sjenèar prima
// Predstavlja informacije o kameri
struct shader_input_buffer_1 {
	glm::mat4 lookDir;

	glm::vec4 initPos;
	glm::vec4 initDir;

	float xPosMultiplier;
	float yPosMultiplier;

	float xDirMultiplier;
	float yDirMultiplier;

};

struct shader_input_buffer_2 {
	glm::mat4 lookDir;

	glm::vec4 initPos;
	glm::vec4 initDir;

	float xPosMultiplier;
	float yPosMultiplier;

	float xDirMultiplier;
	float yDirMultiplier;

};






class RenderEngine {

public:
	void init();

	void run();

	void cleanup();



	struct SDL_Window* _window;
	VkExtent2D _windowExtent{ 1600 , 900 };

	// Opisnici Vulkan instance
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkSurfaceKHR _window_surface;

	// Fizièki opis ureðaja
	VkPhysicalDevice _physical_GPU;
	// Logièki opis ureðaja
	VkDevice _device;

	// Redovi za èišæenje Vulkanovih struktura
	DeletionQueue _main_deletion_queue;
	DeletionQueue _swapchain_deletion_queue;
	DeletionQueue _image_deletion_queue;


	VmaAllocator _allocator;


	// Maksimalna memorija pridružena sjenèarovom 1. spremniku
	unsigned int _max_uniform_memory;
	

	const unsigned int _max_frames_in_flight = 2;
	unsigned int _current_frame = 0;

	Frame* _frames;


	// Opisnici za pipelineove
	VkDescriptorSetLayout _compute_set_layout;
	VkDescriptorPool _compute_pool;

	VkPipelineLayout _compute_pipeline_Layout;
	VkPipeline _default_compute_pipeline;

	struct PipelineBuilder {
	public:

		std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
		VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
		VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
		VkPipelineLayout _pipelineLayout;

		VkPipeline build_compute_pipeline(VkDevice device);
	};


	// GPU redovi za izvršavanje naredbi
	VkQueue _compute_queue;
	uint32_t _compute_queue_family;

	VkQueue _graphics_queue;
	uint32_t _graphics_queue_family;



	unsigned int _screen_size_x;
	unsigned int _screen_size_y;


	

	// Strukture za prikazivanje
	VkSwapchainKHR _swapchain;
	VkFormat _swapchain_image_format;
	std::vector<VkImage> _swapchain_images;
	std::vector<VkImageView> _swapchain_image_views;

	



	bool should_quit = false;

	// Strukture simulacije

	Camera main_camera;

	struct Camera_movement{
		float v_x = 0;
		float v_y = 0;

		float pitch = 0;
		float yaw = 0;
	};

	Camera_movement main_camera_movement;

	Sun sun;


private:
	

	void init_vulkan();

	void allocate_compute_buffers();
	void allocate_compute_images();
	void init_compute_descriptors();
	void init_compute_pipelines();

	void init_swapchain();
	void cleanup_swapchain();
	void recreate_swapchain();

	void init_command_buffers();
	void init_sync_structures();

	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);

	// Glavni proces pozivanja sjenèara
	void compute();


	void handle_input();
	void process_movement();
};