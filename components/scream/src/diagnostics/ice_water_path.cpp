#include "diagnostics/ice_water_path.hpp"

namespace scream
{

// =========================================================================================
IceWaterPathDiagnostic::IceWaterPathDiagnostic (const ekat::Comm& comm, const ekat::ParameterList& params)
  : AtmosphereDiagnostic(comm,params)
{
  // Nothing to do here
}

// =========================================================================================
void IceWaterPathDiagnostic::set_grids(const std::shared_ptr<const GridsManager> grids_manager)
{
  using namespace ekat::units;
  using namespace ShortFieldTagsNames;

  auto Q = kg/kg;
  Q.set_string("kg/kg");

  const auto& grid_name = m_params.get<std::string>("Grid");
  auto grid  = grids_manager->get_grid(grid_name);
  m_num_cols = grid->get_num_local_dofs(); // Number of columns on this rank
  m_num_levs = grid->get_num_vertical_levels();  // Number of levels per column

  FieldLayout scalar3d_layout_mid { {COL,LEV}, {m_num_cols,m_num_levs} };
  FieldLayout scalar2d_layout_mid { {COL},     {m_num_cols}            };
  constexpr int ps = Pack::n;

  // The fields required for this diagnostic to be computed
  add_field<Required>("pseudo_density", scalar3d_layout_mid, Pa, grid_name, ps);
  add_field<Required>("qi",             scalar3d_layout_mid, Q,  grid_name, "tracers", ps);

  // Construct and allocate the diagnostic field
  FieldIdentifier fid (name(), scalar2d_layout_mid, m, grid_name);
  m_diagnostic_output = Field(fid);
  auto& C_ap = m_diagnostic_output.get_header().get_alloc_properties();
  C_ap.request_allocation();
  m_diagnostic_output.allocate_view();
}
// =========================================================================================
void IceWaterPathDiagnostic::initialize_impl(const RunType /* run_type */)
{
  auto ts = timestamp(); 
  m_diagnostic_output.get_header().get_tracking().update_time_stamp(ts);
}
// =========================================================================================
void IceWaterPathDiagnostic::run_impl(const int /* dt */)
{

  using PC         = scream::physics::Constants<Real>;
  constexpr Real gravit = PC::gravit;
  const auto npacks         = ekat::npack<Pack>(m_num_levs);
  const auto default_policy = ekat::ExeSpaceUtils<KT::ExeSpace>::get_default_team_policy(m_num_cols, npacks);
  const auto& lwp                = m_diagnostic_output.get_view<Real*>();
  const auto& qi_mid             = get_field_in("qi").get_view<const Pack**>();
  const auto& pseudo_density_mid = get_field_in("pseudo_density").get_view<const Pack**>();

  Kokkos::parallel_for("IceWaterPathDiagnostic",
                       default_policy,
                       KOKKOS_LAMBDA(const MemberType& team) {
    const int icol = team.league_rank();
    Kokkos::parallel_reduce(Kokkos::TeamThreadRange(team, m_num_levs), [&] (const Int& idx, Real& lsum) {
      const int jpack = idx / Pack::n;
      const int klev  = idx % Pack::n;
      lsum += qi_mid(icol,jpack)[klev] * pseudo_density_mid(icol,jpack)[klev]/gravit;
    },lwp(icol));
    team.team_barrier();
  });

}
// =========================================================================================
} //namespace scream