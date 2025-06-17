struct Atmosphere{
	alignas(4) float surface_pressure_pa;
	alignas(4) float average_density_height; // Na ovoj visini se nalazi prosjeèna gustoæa atmosfere
	alignas(4) float average_density_height_aerosol; // Visina prosjeène gustoæe aerosola u atmosferi
	alignas(4) float mie_asymmetry_const;

	alignas(4) float upper_limit;

	alignas(4) float temperature; // Aproksimacija - cijela atmosfera je konstantne temperature zasad
	alignas(4) float molar_mass; // Potrebno za raèunanje gustoæe i kolièine èestica za Rayleighovu jednadžbu

	alignas(4) float refractivity;
	alignas(4) float atom_radius;
};

struct Planet{
	alignas(4) float radius;
	//float gravity;

	Atmosphere atmosphere;

};