if (COMP_NAME STREQUAL elm)
  string(APPEND FFLAGS " -hfp0")
endif()
string(APPEND FFLAGS " -hipa0 -hzero")
# -em -ef generates modulename.mod (lowercase files) to support
# Scorpio installs
string(APPEND FFLAGS " -em -ef")

string(APPEND SLIBS " -L$ENV{PNETCDF_PATH}/lib -lpnetcdf")
set(NETCDF_PATH "$ENV{NETCDF_DIR}")
set(PNETCDF_PATH "$ENV{PNETCDF_DIR}")
set(PIO_FILESYSTEM_HINTS "gpfs")
string(APPEND CXX_LIBS " -lstdc++")
