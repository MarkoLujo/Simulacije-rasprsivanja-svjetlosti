#include <iostream>
#include "rendering\renderEngine.h"

#ifdef _WIN32
#include <windows.h>
#endif

#undef main
int main(int argc, char* argv){

// Prikazivanje toène velièine prozora na Windows sustavima
#ifdef _WIN32
	SetProcessDPIAware();
#endif
	
	RenderEngine main_engine;
	main_engine._max_uniform_memory = 1 << 16;

	main_engine._screen_size_x = 1600;
	main_engine._screen_size_y = 900;

	main_engine.sun = Sun{
		1.496f * powf(10, 8) * 1000, // Udaljenost (1.496 * 10^8 km)
		1391400.0f * 1000, // Velièina

		50.0f // Kut nagiba
	};

	Atmosphere earth_atmosphere = Atmosphere{
		101325, // Tlak na morskoj razini (u pa)
		5600, // Poluopad tlaka (koliko metara u visinu je potrebno iæi da tlak padne na polovicu prijašnje vrijednosti)

		100 * 1000 // Maksimalna razina
	};

	main_engine.main_planet = Planet{
		6371 * 1000, // Radijus Zemlje je oko 6,371 km
		earth_atmosphere
	};

	main_engine.main_camera.position = glm::vec3(
		0,
		main_engine.main_planet.radius + 10,
		0
	);

	main_engine.sample_amount = 10;

	main_engine.init();


	main_engine.run();


	main_engine.cleanup();

	return 0;
}
#define main SDL_main