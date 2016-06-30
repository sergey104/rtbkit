#include "appodeal_debug.h"

namespace appodeal_debug {


std::string loadFile(const std::string & filename)
{
    ML::filter_istream stream(filename);

    std::string result;

    while (stream) {
        std::string line;
        getline(stream, line);
        result += line + "\n";
    }

    return result;
}

long int unix_timestamp()
{
    time_t t = std::time(0);
    long int now = static_cast<long int> (t);
    return now;
}

}
