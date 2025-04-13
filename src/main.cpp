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

		100 * 1000, // Maksimalna razina

		270, // Aproksimacija da je cijela atmosfera na -3 Celzijeva stupnja
		0.0289652, // Približna molarna masa zraka (u kg/mol)
		1.0002793, // Refraktivni indeks
		364 * powf(10, -12) // Kinetièki radijus atoma dušika
	};


	double pi = 3.141592654;
	// Konstantan dio jednadžbe koji ne ovisi o valnoj duljini, treba se podijeliti s valnom duljinom na 4 potenciju
	double t1 = (8*pi*(16*pi*pi*pi*pi)/3);
	double t2 = ((earth_atmosphere.refractivity*earth_atmosphere.refractivity-1)/(earth_atmosphere.refractivity*earth_atmosphere.refractivity+2));
	double atom_size = 2 * pi * earth_atmosphere.atom_radius;
	double t3 = (atom_size*atom_size);

	double rayleigh_cross_section_const_2 = t1 * t2 * t2 * t3 * t3 * t3;

	double rayleigh_cross_section_red = rayleigh_cross_section_const_2 / ((532 * 0.000000001   * 532 * 0.000000001   * 532 * 0.000000001   * 532 * 0.000000001));


	double rayleigh_cross_section_const = (8*pi*(16*pi*pi*pi*pi)/3) * 
		((earth_atmosphere.refractivity*earth_atmosphere.refractivity-1)/(earth_atmosphere.refractivity*earth_atmosphere.refractivity+2)) *
		((earth_atmosphere.refractivity*earth_atmosphere.refractivity-1)/(earth_atmosphere.refractivity*earth_atmosphere.refractivity+2)) *
		(earth_atmosphere.atom_radius*earth_atmosphere.atom_radius*earth_atmosphere.atom_radius*earth_atmosphere.atom_radius*earth_atmosphere.atom_radius*earth_atmosphere.atom_radius);


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