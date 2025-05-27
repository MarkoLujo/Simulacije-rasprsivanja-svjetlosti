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
		8700, // Visina na kojoj se nalazi prosjeèna gustoæa atmosfere

		100 * 1000, // Maksimalna razina

		270, // Aproksimacija da je cijela atmosfera na -3 Celzijeva stupnja
		0.0289652, // Približna molarna masa zraka (u kg/mol)
		1.0002793, // Refraktivni indeks
		364 * powf(10, -12) // Kinetièki radijus atoma dušika
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


	// Raèunanje konstanti Rayleighove jednadžbe

	double pi = 3.141592654;

	// Kolièina molekula po kubnom metru na površini
	double surface_density = earth_atmosphere.surface_pressure_pa /(earth_atmosphere.temperature * (8.3144621 /*plinska konstanta*/  / earth_atmosphere.molar_mass));
	double surface_mass = surface_density * (1*1*1);
	double num_air_particles_2 = (surface_mass / earth_atmosphere.molar_mass)  * (6.02214076 * pow(10,23)) /*Avogadrova konstanta*/;
	double surface_molecule_density = num_air_particles_2 / (1*1*1);

	double K = 2 * pi * pi * (earth_atmosphere.refractivity*earth_atmosphere.refractivity - 1) * (earth_atmosphere.refractivity*earth_atmosphere.refractivity - 1) / surface_molecule_density / 3.0f;


	// Raèunanje Rayleighovog optièkog presjeka atmosfere

	double red_wavelenght   = 630 * 0.000000001; // nanometri
	double green_wavelenght = 525 * 0.000000001;
	double blue_wavelenght  = 440 * 0.000000001;

	/*
	double atmosphere_atom_size = 2 * pi * earth_atmosphere.atom_radius;

	// Konstantan dio jednadžbe koji ne ovisi o valnoj duljini, treba se podijeliti s valnom duljinom na 4 potenciju
	// Razdvajanje zbog floating point gluposti
	double rayleigh_cross_section_const_1 = ((16*pi*pi*pi*pi)/3); // Velik
	double rayleigh_cross_section_const_2 = ((earth_atmosphere.refractivity*earth_atmosphere.refractivity - 1)/(earth_atmosphere.refractivity*earth_atmosphere.refractivity +2)); // Malen

	double rayleigh_cross_section_const_3 = ((atmosphere_atom_size*atmosphere_atom_size) * (6.02214076 * pow(10,23))) * ((atmosphere_atom_size*atmosphere_atom_size) * ((atmosphere_atom_size*atmosphere_atom_size))); // Ultramalen èak i kad je pomnožen s Avogadrovom konstantom

	double rayleigh_cross_section_const = rayleigh_cross_section_const_1 * rayleigh_cross_section_const_2 * rayleigh_cross_section_const_2 * rayleigh_cross_section_const_3;


	// Konstantan dio jednadžbe koji ne ovisi o gustoæi atmosfere
	// "Optièki presjek" - što je valna duljina svjetla manja, to je ovo veæe pa se više raspršuje
	double rayleigh_cross_section_red   = rayleigh_cross_section_const / ((red_wavelenght   * red_wavelenght   * red_wavelenght   * red_wavelenght));
	double rayleigh_cross_section_green = rayleigh_cross_section_const / ((green_wavelenght * green_wavelenght * green_wavelenght * green_wavelenght));
	double rayleigh_cross_section_blue  = rayleigh_cross_section_const / ((blue_wavelenght  * blue_wavelenght  * blue_wavelenght  * blue_wavelenght));


	main_engine.rayleigh_cross_section_red_Av = rayleigh_cross_section_red;
	main_engine.rayleigh_cross_section_green_Av = rayleigh_cross_section_green;
	main_engine.rayleigh_cross_section_blue_Av = rayleigh_cross_section_blue;
	*/

	main_engine.K = K;

	main_engine.sample_amount = 10;

	main_engine.init();


	main_engine.run();


	main_engine.cleanup();

	return 0;
}
#define main SDL_main