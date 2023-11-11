#pragma once

#include <boost/json.hpp>
#include <string>
#include <ostream>

namespace watchList { namespace boost_ext
{
    void pretty_print(std::ostream& os, boost::json::value const& jv, std::string* indent = nullptr);  
} }
