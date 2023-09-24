#pragma once

static std::vector<std::string> get_gpu_names(std::string binary, std::string filename)
{
    // Execute the GPU listing program and redirect its output to a file
    if (std::system((binary + " > " +  filename).c_str()) < 0) {
	    return {};
    }

    std::vector<std::string> gpu_names;
    std::ifstream file(filename);
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
            gpu_names.push_back(line);
        file.close();
    }

    return gpu_names;
}

class GpuList : public wxPanel
{
public:
    GpuList(wxPanel* parent) : wxPanel(parent, wxID_ANY), selection(0) {
        comboBox = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, -1));
        comboBox->Bind(wxEVT_COMBOBOX, &GpuList::OnComboBox, this);
        update();

        wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

        sizer->Add(comboBox, 0, wxALIGN_CENTER_VERTICAL); // Vertically center the comboBox

        this->SetSizerAndFit(sizer);
    }
    void update(void) {
    	auto cfg = Config::instance();
    	auto lister_binary = cfg->gpu_binary_location() + "/" + "gpu_lister";
    	auto lister_file = cfg->gpu_binary_location () + "/" + "gpus.txt";
    	if (boost::filesystem::exists(lister_binary)) {
			auto gpu_names = get_gpu_names(lister_binary, lister_file);

			comboBox->Clear();
			for (const auto& name : gpu_names)
				 comboBox->Append(name);
    	}
    }

    int getSelection(void) {
    	return selection;
    }
    void setSelection(int sel) {
        if ((int)comboBox->GetCount() > sel)
            comboBox->SetSelection(sel);
    }

private:
    void OnComboBox([[maybe_unused]] wxCommandEvent& event) {
        selection = comboBox->GetSelection();
        if (selection != wxNOT_FOUND)
    		Config::instance ()->set_selected_gpu(selection);
    }

    wxComboBox* comboBox;
    int selection;
};

class GPUPage : public Page
{
public:
	GPUPage (wxSize panel_size, int border)
		: Page (panel_size, border),
		  _enable_gpu(nullptr), _binary_location(nullptr), _gpu_list_control(nullptr)
	{}

	wxString GetName () const override
	{
		return _("GPU");
	}

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon () const override
	{
		return wxBitmap(icon_path("tms"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup () override
	{
		auto config = Config::instance ();

		_enable_gpu = new CheckBox (_panel, _("Enable GPU Acceleration"));
		_panel->GetSizer()->Add (_enable_gpu, 0, wxALL | wxEXPAND, _border);

		wxFlexGridSizer* table = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol (1, 1);
		_panel->GetSizer()->Add (table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer (table, _panel, _("Acceleration Binary Folder"), true, 0, wxLEFT | wxLEFT | wxALIGN_CENTRE_VERTICAL);
		_binary_location = new wxDirPickerCtrl (_panel, wxDD_DIR_MUST_EXIST);
		table->Add (_binary_location, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("GPU Selection"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_gpu_list_control = new GpuList(_panel);
		table->Add (_gpu_list_control, 1, wxEXPAND);

		add_label_to_sizer (table, _panel, _("License Server"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_server = new wxTextCtrl (_panel, wxID_ANY);
		table->Add (_server, 1, wxEXPAND | wxALL);

		add_label_to_sizer (table, _panel, _("Port"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_port = new wxSpinCtrl (_panel, wxID_ANY);
		_port->SetRange (0, 65535);
		table->Add (_port);

		add_label_to_sizer (table, _panel, _("License"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_license = new PasswordEntry (_panel);
		table->Add (_license->get_panel(), 1, wxEXPAND | wxALL);

		_enable_gpu->bind(&GPUPage::enable_gpu_changed, this);
		_binary_location->Bind (wxEVT_DIRPICKER_CHANGED, boost::bind (&GPUPage::binary_location_changed, this));
		_server->Bind (wxEVT_TEXT, boost::bind(&GPUPage::server_changed, this));
		_port->Bind (wxEVT_SPINCTRL, boost::bind(&GPUPage::port_changed, this));
		_license->Changed.connect (boost::bind(&GPUPage::license_changed, this));

		_binary_location->Enable(config->enable_gpu());
		_gpu_list_control->Enable(config->enable_gpu());
		_server->Enable(config->enable_gpu());
		_port->Enable(config->enable_gpu());
		_license->get_panel()->Enable(config->enable_gpu());
	}


	void config_changed () override
	{
		auto config = Config::instance ();

		checked_set (_enable_gpu, config->enable_gpu());
		_binary_location->SetPath(config->gpu_binary_location ());
		_gpu_list_control->update();
		_gpu_list_control->setSelection(config->selected_gpu());
		checked_set (_server, config->gpu_license_server());
		checked_set (_port, config->gpu_license_port());
		checked_set (_license, config->gpu_license());
	}

	void enable_gpu_changed ()
	{
		auto config = Config::instance ();

		config->set_enable_gpu (_enable_gpu->GetValue());
		_binary_location->Enable(config->enable_gpu());
		_gpu_list_control->Enable(config->enable_gpu());
		_server->Enable(config->enable_gpu());
		_port->Enable(config->enable_gpu());
		_license->get_panel()->Enable(config->enable_gpu());
	}

	void binary_location_changed ()
	{
		Config::instance()->set_gpu_binary_location (wx_to_std (_binary_location->GetPath ()));
		_gpu_list_control->update();
	}

	void server_changed ()
	{
		Config::instance()->set_gpu_license_server(wx_to_std(_server->GetValue()));
	}

	void port_changed ()
	{
		Config::instance()->set_gpu_license_port(_port->GetValue());
	}

	void license_changed ()
	{
		Config::instance()->set_gpu_license(_license->get());
	}

	CheckBox* _enable_gpu;
	wxDirPickerCtrl* _binary_location;
	GpuList *_gpu_list_control;
	wxTextCtrl* _server;
	wxSpinCtrl* _port;
	PasswordEntry* _license;
};
