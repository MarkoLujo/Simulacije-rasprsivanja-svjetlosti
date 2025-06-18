#include <iostream>
#include "rendering\renderEngine.h"



#ifdef _WIN32
#include <windows.h>
#endif


#undef main
int main(int argc, char* argv){

// Prikazivanje to�ne veli�ine prozora na Windows sustavima
#ifdef _WIN32
	SetProcessDPIAware();
#endif
	
	// Unos po�etnih podataka u engine
	RenderEngine main_engine;
	main_engine._max_uniform_memory = 1 << 16;

	main_engine._screen_size_x = 1600;
	main_engine._screen_size_y = 900;


	main_engine.sun = Sun{
		1.496f * powf(10, 8) * 1000, // Udaljenost (1.496 * 10^8 km)
		1391400.0f * 1000, // Veli�ina

		50.0f, // Kut nagiba

		630, // Valne duljine RGB komponenata svjetlosti
		525,
		440,

		50, // ukupan intenzitet svjetlosti
		{1.0,1.0,1.0,1.0} // boja svjetlosti - bijela
	};

	Atmosphere earth_atmosphere = Atmosphere{
		101325, // Tlak na morskoj razini (u pa)

		8700, // Visina na kojoj se nalazi prosje�na gusto�a atmosfere
		1200, // Visina na kojoj se nalazi prosje�na gusto�a aerosola u atmosferi
		// Razlog za�to ve�i povr�inski tlak daje "manje gustu" atmosferu je zbog na�ina ra�unanja s prosje�nom vrijednosti 
		// ako je tlak na povr�ini visok, po ra�unu tlak svugdje drugdje je puno ni�i (a zrake su �e��e visoko u zraku)


		0.1f, // Relativna koli�ina aerosola
		0.90, // Konstanta asmetrije kuta aerosolnog Mie raspr�ivanja

		100 * 1000, // Maksimalna razina

		270, // Aproksimacija da je cijela atmosfera na -3 Celzijeva stupnja
		1.0002793, // Refraktivni indeks
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


	// Ra�unanje konstanti Rayleighove jednad�be
	double pi = 3.141592654;

	double surface_mole_number =  (earth_atmosphere.surface_pressure_pa * (1*1*1)) / (8.3144621 * earth_atmosphere.temperature);
	//double surface_density = earth_atmosphere.surface_pressure_pa /(earth_atmosphere.temperature * (8.3144621 /*plinska konstanta*/  / earth_atmosphere.molar_mass));
	//double surface_mass = surface_density * (1*1*1);
	double num_air_particles_2 = surface_mole_number  * (6.02214076 * pow(10,23)) /*Avogadrova konstanta*/;
	//double num_air_particles_2 = (surface_mass / earth_atmosphere.molar_mass) * (6.02214076 * pow(10,23)) /*Avogadrova konstanta*/;
	
	// Koli�ina molekula po kubnom metru na povr�ini
	double surface_molecule_density = num_air_particles_2 / (1*1*1);

	double K = 2 * pi * pi * (earth_atmosphere.refractivity*earth_atmosphere.refractivity - 1) * (earth_atmosphere.refractivity*earth_atmosphere.refractivity - 1) / surface_molecule_density / 3.0f;

	main_engine.K = K;

	main_engine.sample_amount_in = 10;
	main_engine.sample_amount_out = 10;


	// Pokretanje sustava za prikaz
	main_engine.init();
	main_engine.run();

	// Izvr�ava se kada se prozor zatvori
	main_engine.cleanup();

	return 0;
}
#define main SDL_main