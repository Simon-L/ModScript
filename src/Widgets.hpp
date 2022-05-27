struct LoadScriptItem : MenuItem {
	Lune *module;
	void onAction(const event::Action& e) override {
		module->loadScript();
	}
};

struct AutoReloadItem : MenuItem {
	Lune *module;
	void onAction(const event::Action& e) override {
		module->autoReload ^= true;
		module->reloadTimer.reset();
	}
	void step() override {
		rightText = CHECKMARK(module->autoReload);
	}
};

struct HoveredNameLabel : MenuLabel {
	std::string* name;
	void step() override {
		text = *name;
	}
};

struct HoveredParameterLabel : MenuLabel {
	std::string* name;
	int64_t* id;
	void step() override {
		if (*id < 0)
			text = string::f("Parameter: %s Id: -/-", name->c_str());
		else
			text = string::f("Parameter: %s Id: %ld", name->c_str(), *id);
	}
};

struct HoveredIdItem : MenuItem {
	int64_t* id;
	void onAction(const event::Action& e) override {
		glfwSetClipboardString(APP->window->win, string::f("0x%lx", *id).c_str());
	}
	void step() override {
		text = string::f("Id: 0x%lx", *id);
	}
};

struct UserScriptItem : MenuItem {
	Lune* module;
	std::string newPath;

	UserScriptItem(Lune* module, std::string name){
		this->text = name;
		this->module = module;
		this->newPath = module->scriptsDir + PATH_SEPARATOR + name;
		this->rightText = CHECKMARK(module->path == this->newPath);
	}

	void onAction(const event::Action& e) override {
		module->path = newPath;
		module->setScript();
	}
};

struct UserScriptsMenu : MenuItem {
	Lune* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		if (module) {
			menu->addChild(createMenuItem("Unload script", "",
				[=]() {
					module->path = "";
					module->script = "";
					module->setScript();
				}
			));
			menu->addChild(new MenuSeparator);
			for (size_t i = 0; i < module->scriptFiles.size(); i++) {
				UserScriptItem* it = new UserScriptItem(module, module->scriptFiles[i]);
				menu->addChild(it);
			}
		}
		return menu;
	}
};