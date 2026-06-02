#pragma once
#include <string>

std::string wrap_v2(const std::string& source, bool keep_comments = false,
                    bool preserve_case = false);
