cmake_minimum_required(VERSION 2.4...5.0)

project(simulacija_svjetlosti)

cmake_policy(SET CMP0079 NEW)

set (CMAKE_CXX_STANDARD 17)

# Find the Vulkan SDK and the libraries for Vulkan and SDL2.
include(CMake/find_vulkan_sdk.cmake) 

add_subdirectory(src)

if(Vulkan_INCLUDE_FOUND)
  target_include_directories(simulacija_atmosfere PUBLIC "${Vulkan_INCLUDE}")
  target_include_directories(simulacija_atmosfere PUBLIC "${Vulkan_INCLUDE}/SDL2")

  if(Vulkan_LIBRARY_FOUND)
    target_link_libraries(simulacija_atmosfere "${Vulkan_LIBRARY}")
  else()
    if(WIN32)
	  message(FATAL_ERROR "ERROR: Vulkan SDK was found, but somehow it does not contain the vulkan-1 library. Try reinstalling the entire SDK")
	else()
	  message(FATAL_ERROR "ERROR: The vulkan headers were found, but not the vulkan-1 library. Try editing the CMake/find_vulkan_sdk.cmake file to specify its location.")
	endif()
  endif()
  
  if(SDL_FOUND)
    target_link_libraries(simulacija_atmosfere "${SDL_LIBRARY}")
    target_link_libraries(simulacija_atmosfere "${SDL_MAIN_LIBRARY}")
  else()
    if(WIN32)
	  message(FATAL_ERROR "ERROR: Vulkan SDK was found, but the SDL2 component is missing. Try installing it using the maintenance tool.")
	else()
	  message(FATAL_ERROR "ERROR: The vulkan headers were found, but the SDL2 libraries are missing. Try editing the CMake/find_vulkan_sdk.cmake file to specify their location.")
	endif()
  endif()

else()
  message(FATAL_ERROR "ERROR: Vulkan SDK was not found in the environment variables, or it does not contain the include folder.")
endif()