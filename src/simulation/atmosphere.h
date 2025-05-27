struct Atmosphere{
	float surface_pressure_pa;
	float average_density_height; // Na ovoj visini se nalazi prosje�na gusto�a atmosfere

	float upper_limit;

	float temperature; // Aproksimacija - cijela atmosfera je konstantne temperature zasad
	float molar_mass; // Potrebno za ra�unanje gusto�e i koli�ine �estica za Rayleighovu jednad�bu

	float refractivity;
	float atom_radius;
};

struct Planet{
	float radius;
	//float gravity;

	Atmosphere atmosphere;

};