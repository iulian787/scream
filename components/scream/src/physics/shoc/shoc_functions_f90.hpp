#ifndef SCREAM_SHOC_FUNCTIONS_F90_HPP
#define SCREAM_SHOC_FUNCTIONS_F90_HPP

#include "ekat/util/scream_utils.hpp"
#include "ekat/scream_types.hpp"

#include "shoc_functions.hpp"

#include <vector>
#include <array>
#include <utility>

//
// Bridge functions to call fortran version of shoc functions from C++
//

namespace scream {
namespace shoc {


///////////////////////////////////////////////////////////////////////////////
// Converted subroutine helpers go here.
struct SHOCGridData{
static constexpr size_t NUM_ARRAYS = 4; 
static constexpr size_t NUM_ARRAYS_i = 2; 

    
 // Inputs 
 Int shcol, nlev, nlevi;
 Real *zt_grid, *zi_grid, *pdel;

 // In/out
 Real *dz_zt, *dz_zi, *rho_zt;


 SHOCGridData(Int shcol_, Int nevl_, Int nlevi_);
 SHOCGridData(const SHOCGridData& rhs);
 
 void init_ptrs();
 void transpose();

 // Internals
 Int m_shcol, m_nlev, m_nlevi, m_total, m_totali; 
 std::vector<Real> m_data;
 std::vector<Real> m_datai;
};
void shoc_grid(Int nlev, SHOCGridData& d);

}  // namespace shoc
}  // namespace scream

#endif
