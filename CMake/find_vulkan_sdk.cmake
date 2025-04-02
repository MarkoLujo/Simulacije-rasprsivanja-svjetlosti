if(WIN32)
	find_path(Vulkan_INCLUDE NAMES "vulkan" PATHS "$ENV{VULKAN_SDK}/Include") # find vulkan SDK's include folder using environment variables

	if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4") # if on a 32-bit system (void pointer is 4 bytes)
		find_library(Vulkan_LIBRARY NAMES "vulkan-1" PATHS "$ENV{VULKAN_SDK}/Lib32") # get libraries
		find_library(SDL_LIBRARY NAMES "SDL2" PATHS "$ENV{VULKAN_SDK}/Lib32")
		find_library(SDL_LIBRARY_MAIN NAMES "SDL2main" PATHS "$ENV{VULKAN_SDK}/Lib32")
	elseif("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8") # on a 64-bit system
		find_library(Vulkan_LIBRARY NAMES "vulkan-1" PATHS "$ENV{VULKAN_SDK}/Lib")
		find_library(SDL_LIBRARY NAMES "SDL2" PATHS "$ENV{VULKAN_SDK}/Lib")
		find_library(SDL_LIBRARY_MAIN NAMES "SDL2main" PATHS "$ENV{VULKAN_SDK}/Lib")
	endif()
else()
	find_path(Vulkan_INCLUDE NAMES "vulkan/vulkan.h" PATHS "$ENV{VULKAN_SDK}/include")	
	
	# REPLACE THESE PATHS WITH THE LOCATIONS OF YOUR VULKAN AND SDL LIBRARIES ON LINUX
	find_library(Vulkan_LIBRARY NAMES "libvulkan.so" PATHS "$ENV{VULKAN_SDK}/lib")
	find_library(SDL_LIBRARY NAMES "libSDL2.so" PATHS "$ENV{VULKAN_SDK}/lib")
	find_library(SDL_MAIN_LIBRARY NAMES "libSDL2.so" PATHS "$ENV{VULKAN_SDK}/lib")
	
endif()


# Set variables
if (NOT("${Vulkan_INCLUDE}" STREQUAL "Vulkan_INCLUDE-NOTFOUND"))
	set(Vulkan_INCLUDE_FOUND TRUE)
else()
	set(Vulkan_INCLUDE_FOUND FALSE)
endif()

if (NOT("${Vulkan_LIBRARY}" STREQUAL "Vulkan_LIBRARY-NOTFOUND"))
	set(Vulkan_LIBRARY_FOUND TRUE)
else()
	set(Vulkan_LIBRARY_FOUND FALSE)
endif()

if((NOT("${SDL_LIBRARY}" STREQUAL "SDL_LIBRARY-NOTFOUND")) AND (NOT("${SDL_LIBRARY_MAIN}" STREQUAL "SDL_MAIN_LIBRARY-NOTFOUND")))
	set(SDL_FOUND TRUE)
else()
	set(SDL_FOUND FALSE)
endif()
