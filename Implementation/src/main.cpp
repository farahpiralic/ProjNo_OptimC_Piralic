//  OptimCompare - Project #16: Comparative Analysis of First- and
//  Second-Order Unconstrained Optimization Methods
//  Student: F. Piralić, index 20106
//
//  main.cpp
#include "Application.h"
#include <td/StringConverter.h>
#include <gui/WinMain.h>

int main(int argc, const char* argv[])
{
    Application app(argc, argv);
    // load properties from the OS store (registry / plist / settings scheme)
    auto appProperties = app.getProperties();
    td::String trLang = appProperties->getValue("translation", "EN");
    app.init(trLang);
    return app.run();
}
