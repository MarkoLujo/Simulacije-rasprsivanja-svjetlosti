

struct Atmosphere{
	float surface_pressure_pa;
	float half_distance; // Pri usponu od ovoliko metara tlak se prepolovi

	float upper_limit;
};

struct Planet{
	float radius;
	//float gravity;

	Atmosphere atmosphere;

};