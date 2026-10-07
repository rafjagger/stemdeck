# Writes ${OUT} with a hash of the googletest files in ${FILES}, but only when
# the hash changed, so an unchanged googletest leaves the header's time alone.
set(content "")
foreach(path IN LISTS FILES)
    file(SHA256 "${path}" digest)
    string(APPEND content "${digest} ")
endforeach()
string(SHA256 stamp "${content}")
set(header "#pragma once\n#define STEMDECK_GTEST_STAMP \"${stamp}\"\n")

set(current "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" current)
endif()
if(NOT current STREQUAL header)
    file(WRITE "${OUT}" "${header}")
endif()
