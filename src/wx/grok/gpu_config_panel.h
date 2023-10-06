/*
    Copyright (C) 2023 Grok Image Compression Inc.

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#pragma once

static std::vector<std::string> get_gpu_names(boost::filesystem::path binary, boost::filesystem::path filename)
{
    // Execute the GPU listing program and redirect its output to a file
    if (std::system((binary.string() + " > " + filename.string()).c_str()) < 0) {
	    return {};
    }

    std::vector<std::string> gpu_names;
    std::ifstream file(filename.c_str());
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
	GpuList(wxPanel* parent)
		: wxPanel(parent, wxID_ANY)
	{
		_combo_box = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, -1));
		_combo_box->Bind(wxEVT_COMBOBOX, &GpuList::OnComboBox, this);
		update();

		auto sizer = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(_combo_box, 0, wxALIGN_CENTER_VERTICAL);
		SetSizerAndFit(sizer);
	}

	void update()
	{
		auto cfg = Config::instance();
		auto lister_binary = cfg->gpu_binary_location() / "gpu_lister";
		auto lister_file = cfg->gpu_binary_location () / "gpus.txt";
		if (boost::filesystem::exists(lister_binary)) {
			auto gpu_names = get_gpu_names(lister_binary, lister_file);

			_combo_box->Clear();
			for (auto const& name: gpu_names) {
				_combo_box->Append(name);
			}
		}
	}

	void set_selection(int sel)
	{
		if (sel < static_cast<int>(_combo_box->GetCount())) {
			_combo_box->SetSelection(sel);
		}
	}

private:
	void OnComboBox(wxCommandEvent&)
	{
		auto selection = _combo_box->GetSelection();
		if (selection != wxNOT_FOUND) {
			Config::instance()->set_selected_gpu(selection);
		}
	}

	wxComboBox* _combo_box;
	int _selection = 0;
};


class GPUPage : public Page
{
public:
	GPUPage(wxSize panel_size, int border)
		: Page(panel_size, border)
	{}

	wxString GetName() const override
	{
		return _("GPU");
	}

#ifdef DCPOMATIC_OSX
	/* XXX: this icon does not exist */
	wxBitmap GetLargeIcon() const override
	{
		return wxBitmap(icon_path("gpu"), wxBITMAP_TYPE_PNG);
	}
#endif

private:
	void setup() override
	{
		_enable_gpu = new CheckBox(_panel, _("Enable GPU acceleration"));
		_panel->GetSizer()->Add(_enable_gpu, 0, wxALL | wxEXPAND, _border);

		wxFlexGridSizer* table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

		add_label_to_sizer(table, _panel, _("Acceleration binary folder"), true, 0, wxLEFT | wxLEFT | wxALIGN_CENTRE_VERTICAL);
		_binary_location = new wxDirPickerCtrl(_panel, wxDD_DIR_MUST_EXIST);
		table->Add(_binary_location, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("GPU selection"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_gpu_list_control = new GpuList(_panel);
		table->Add(_gpu_list_control, 1, wxEXPAND);

		add_label_to_sizer(table, _panel, _("License server"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_server = new wxTextCtrl(_panel, wxID_ANY);
		table->Add(_server, 1, wxEXPAND | wxALL);

		add_label_to_sizer(table, _panel, _("Port"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_port = new wxSpinCtrl(_panel, wxID_ANY);
		_port->SetRange(0, 65535);
		table->Add(_port);

		add_label_to_sizer(table, _panel, _("License"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_license = new PasswordEntry(_panel);
		table->Add(_license->get_panel(), 1, wxEXPAND | wxALL);

		_enable_gpu->bind(&GPUPage::enable_gpu_changed, this);
		_binary_location->Bind(wxEVT_DIRPICKER_CHANGED, boost::bind (&GPUPage::binary_location_changed, this));
		_server->Bind(wxEVT_TEXT, boost::bind(&GPUPage::server_changed, this));
		_port->Bind(wxEVT_SPINCTRL, boost::bind(&GPUPage::port_changed, this));
		_license->Changed.connect(boost::bind(&GPUPage::license_changed, this));

		setup_sensitivity();
	}

	void setup_sensitivity()
	{
		auto config = Config::instance();

		_binary_location->Enable(config->enable_gpu());
		_gpu_list_control->Enable(config->enable_gpu());
		_server->Enable(config->enable_gpu());
		_port->Enable(config->enable_gpu());
		_license->get_panel()->Enable(config->enable_gpu());
	}

	void config_changed() override
	{
		auto config = Config::instance();

		checked_set(_enable_gpu, config->enable_gpu());
		_binary_location->SetPath(std_to_wx(config->gpu_binary_location().string()));
		_gpu_list_control->update();
		_gpu_list_control->set_selection(config->selected_gpu());
		checked_set(_server, config->gpu_license_server());
		checked_set(_port, config->gpu_license_port());
		checked_set(_license, config->gpu_license());
	}

	void enable_gpu_changed()
	{
		auto config = Config::instance();
		config->set_enable_gpu(_enable_gpu->GetValue());
		setup_sensitivity();
	}

	void binary_location_changed()
	{
		Config::instance()->set_gpu_binary_location(wx_to_std(_binary_location->GetPath()));
		_gpu_list_control->update();
	}

	void server_changed()
	{
		Config::instance()->set_gpu_license_server(wx_to_std(_server->GetValue()));
	}

	void port_changed()
	{
		Config::instance()->set_gpu_license_port(_port->GetValue());
	}

	void license_changed()
	{
		Config::instance()->set_gpu_license(_license->get());
	}

	CheckBox* _enable_gpu = nullptr;
	wxDirPickerCtrl* _binary_location = nullptr;
	GpuList* _gpu_list_control = nullptr;
	wxTextCtrl* _server = nullptr;
	wxSpinCtrl* _port = nullptr;
	PasswordEntry* _license = nullptr;
};
