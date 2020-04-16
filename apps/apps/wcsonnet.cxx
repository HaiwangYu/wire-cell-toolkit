// A wire cell version of the jsonnet cli.

#include <iostream>
#include "WireCellUtil/Persist.h"

using namespace std;
using namespace WireCell;

int main(int argc, char *argv[])
{
    if (argc <= 1)
    {
        cout << "Simple command line Jsonnet->JSON compiler using WCT services\n"
             << "Likely you will need to set WIRECELL_PATH\n\n"
             << "  usage: wcsonnet file.jsonnet\n";
        return 1;
    }

    Persist::Parser parser;
    auto jdat = parser.load(argv[1]);
    cout << jdat << endl;

    return 0;
}
