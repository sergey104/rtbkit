/** augmentor_start_stop_runner.cc                                 -*- C++ -*-

    Runner for our augmentor start/stop.

*/

#include "augmentor_start_stop_time.h"
#include "soa/service/service_utils.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <iostream>
#include <thread>
#include <chrono>


using namespace std;

/******************************************************************************/
/* MAIN                                                                       */
/******************************************************************************/

int main(int argc, char** argv)
{
    using namespace boost::program_options;

    Datacratic::ServiceProxyArguments args;

    options_description options = args.makeProgramOptions();
    options.add_options() ("help,h", "Print this message");

    variables_map vm;
    store(command_line_parser(argc, argv).options(options).run(), vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << options << endl;
        return 1;
    }

    auto serviceProxies = args.makeServiceProxies();
    RTBKIT::StartStopAugmentor augmentor(serviceProxies, "start-stop-time");
    augmentor.init();
    augmentor.start();

    while (true) this_thread::sleep_for(chrono::seconds(10));

    // Won't ever reach this point but this is how you shutdown an augmentor.
    augmentor.shutdown();

    return 0;
}
