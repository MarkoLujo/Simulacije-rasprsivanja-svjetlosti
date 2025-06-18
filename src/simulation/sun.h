#include <vector>


struct Sun {
	alignas(4) float distance;
	alignas(4) float radius;

	alignas(4) float angle;

	alignas(4) float r_wavelen;
	alignas(4) float g_wavelen;
	alignas(4) float b_wavelen;
	alignas(4) float light_intensity;
	
	alignas(16) glm::vec4 light_color;
};