#include <wx/stdpaths.h>
#include <wx/wx.h>

#include "MainWindow.h"

class OwonApp : public wxApp {
public:
  bool OnInit() override {
    SetAppName("Owon1041");
    SetVendorName("MacWake");
    ::wxInitAllImageHandlers(); // required before LoadFile(..., wxBITMAP_TYPE_PNG)
    auto *window = new MainWindow();
    const wxString candidates[] = {
        wxStandardPaths::Get().GetResourcesDir() + "/Owon1041.png",
        "Owon1041.png",
    };
    for (const auto &path : candidates) {
      if (wxFileExists(path)) {
        wxIcon icon;
        if (icon.LoadFile(path, wxBITMAP_TYPE_PNG)) {
          window->SetIcon(icon);
          break;
        }
      }
    }
    window->Show();
    return true;
  }
};

wxIMPLEMENT_APP(OwonApp);
