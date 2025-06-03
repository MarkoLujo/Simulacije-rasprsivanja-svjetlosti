struct Atmosphere{
	float surface_pressure_pa;
	float average_density_height; // Na ovoj visini se nalazi prosjeèna gustoæa atmosfere
	float average_density_height_aerosol; // Visina prosjeène gustoæe aerosola u atmosferi
	float mie_asymmetry_const;

	float upper_limit;

	float temperature; // Aproksimacija - cijela atmosfera je konstantne temperature zasad
	float molar_mass; // Potrebno za raèunanje gustoæe i kolièine èestica za Rayleighovu jednadžbu

	float refractivity;
	float atom_radius;
};

struct Planet{
	float radius;
	//float gravity;

	Atmosphere atmosphere;

};