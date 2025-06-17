#include <vector>

/*
struct emitted_wave{
	double wavelenght;
	double intensity;

};*/

struct Sun {
	alignas(4) float distance;
	alignas(4) float radius;

	alignas(4) float angle;

	//std::vector<emitted_wave> spectrum;
};