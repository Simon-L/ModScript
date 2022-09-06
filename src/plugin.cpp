#include "plugin.hpp"


Plugin* pluginInstance;
std::mutex luaMutex;

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelLune);
	p->addModel(modelLuneHelper);
	p->addModel(modelPleineLune);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
