#ifndef EAMXX_MAM_DRY_DEPOSITION_FUNCTIONS_HPP
#define EAMXX_MAM_DRY_DEPOSITION_FUNCTIONS_HPP

#include <ekat/kokkos/ekat_subview_utils.hpp>
#include <mam4xx/aero_config.hpp>
#include <mam4xx/convproc.hpp>
#include <mam4xx/mam4.hpp>

namespace scream {

namespace {
void compute_tendencies(
    // inputs
    const int ncol, const int nlev, const double dt,
    const MAMDryDep::const_view_1d obklen,
    const MAMDryDep::const_view_1d surfric,
    const MAMDryDep::const_view_1d landfrac,
    const MAMDryDep::const_view_1d icefrac,
    const MAMDryDep::const_view_1d ocnfrac,
    const MAMDryDep::const_view_1d friction_velocity,
    const MAMDryDep::const_view_1d aerodynamical_resistance,
    MAMDryDep::view_3d qtracers,  // FIXME: Make it a constant view
    MAMDryDep::view_1d fraction_landuse_[MAMDryDep::n_land_type],
    const MAMDryDep::const_view_3d dgncur_awet_,
    const MAMDryDep::const_view_3d wet_dens_,
    const mam_coupling::DryAtmosphere dry_atm,
    const mam_coupling::AerosolState dry_aero,

    // input-outputs
    MAMDryDep::view_2d qqcw_[mam4::aero_model::pcnst],

    // outputs
    MAMDryDep::view_3d d_qtracers_dt, MAMDryDep::view_2d aerdepdrycw,
    MAMDryDep::view_2d aerdepdryis,

    // work arrays
    MAMDryDep::view_2d rho_,
    MAMDryDep::view_2d vlc_dry_[mam4::AeroConfig::num_modes()]
                               [MAMDryDep::aerosol_categories_],
    MAMDryDep::view_1d vlc_trb_[mam4::AeroConfig::num_modes()]
                               [MAMDryDep::aerosol_categories_],
    MAMDryDep::view_2d vlc_grv_[mam4::AeroConfig::num_modes()]
                               [MAMDryDep::aerosol_categories_],
    MAMDryDep::view_2d dqdt_tmp_[mam4::aero_model::pcnst]) {
  auto printb = [](const std::string &name, const double &val) {
    std::cout << name << ":" << std::setprecision(15) << val << std::endl;
  };
  static constexpr int num_aero_modes = mam_coupling::num_aero_modes();
  // FIXME: WHY we are using a new policy here?? can't we get it from the
  // run_impl??
  const auto policy =
      ekat::ExeSpaceUtils<MAMDryDep::KT::ExeSpace>::get_default_team_policy(
          1, nlev);
  Kokkos::parallel_for(
      policy, KOKKOS_LAMBDA(const MAMDryDep::KT::MemberType &team) {
        static constexpr int num_aero_species =
            mam_coupling::num_aero_species();

        const int icol = team.league_rank();

        Kokkos::parallel_for(
            Kokkos::TeamVectorRange(team, nlev), [&](const int lev) {
              for(int mode = 0; mode < num_aero_modes; ++mode) {
                int icnst = mam4::ConvProc::numptrcw_amode(mode);
                qtracers(icol, lev, icnst) =
                    dry_aero.int_aero_nmr[mode](icol, lev);
                for(int species = 0; species < num_aero_species; ++species) {
                  icnst = mam4::ConvProc::lmassptrcw_amode(species, mode);
                  if(-1 < icnst) {
                    qtracers(icol, lev, icnst) =
                        dry_aero.int_aero_mmr[mode][species](icol, lev);
                  }
                }
              }
            });  // parallel_for for nlevs
        team.team_barrier();

        mam4::Atmosphere atm    = atmosphere_for_column(dry_atm, icol);
        mam4::Prognostics progs = aerosols_for_column(dry_aero, icol);
        mam4::ConstColumnView dgncur_awet[num_aero_modes];
        mam4::ConstColumnView wet_dens[num_aero_modes];

        for(int i = 0; i < num_aero_modes; ++i) {
          dgncur_awet[i] = ekat::subview(dgncur_awet_, icol, i);
          wet_dens[i]    = ekat::subview(wet_dens_, icol, i);
        }

        mam4::ColumnView rho;
        rho = ekat::subview(rho_, icol);

        static constexpr int n_land_type = MAMDryDep::n_land_type;
        Real fraction_landuse[n_land_type];
        for(int i = 0; i < n_land_type; ++i) {
          fraction_landuse[i] = fraction_landuse_[i](icol);
        }

        // FIXME: why mam4::ColumnView didn;t work here, why use
        // Kokkos::View<Real *>. Solution: Use ColumnView in drydep.hpp as well.
        static constexpr int nmodes = mam4::AeroConfig::num_modes();
        mam4::ColumnView vlc_dry[nmodes][MAMDryDep::aerosol_categories_],
            vlc_grv[nmodes][MAMDryDep::aerosol_categories_];

        Real vlc_trb[nmodes][MAMDryDep::aerosol_categories_];

        for(int i = 0; i < nmodes; ++i) {
          for(int j = 0; j < MAMDryDep::aerosol_categories_; ++j) {
            vlc_dry[i][j] = ekat::subview(vlc_dry_[i][j], icol);
            vlc_trb[i][j] = vlc_trb_[i][j](icol);
            vlc_grv[i][j] = ekat::subview(vlc_grv_[i][j], icol);
          }
        }
        static constexpr int pcnst = mam4::aero_model::pcnst;
        mam4::ColumnView qqcw[pcnst];
        mam4::ColumnView dqdt_tmp[pcnst];
        for(int i = 0; i < pcnst; ++i) {
          qqcw[i]     = ekat::subview(qqcw_[i], icol);
          dqdt_tmp[i] = ekat::subview(dqdt_tmp_[i], icol);
        }
        // Extract Prognostics
        Kokkos::parallel_for(
            Kokkos::TeamThreadRange(team, nlev), KOKKOS_LAMBDA(int kk) {
              for(int m = 0; m < nmodes; ++m) {
                qqcw[mam4::ConvProc::numptrcw_amode(m)][kk] =
                    progs.n_mode_c[m][kk];
                for(int a = 0; a < mam4::AeroConfig::num_aerosol_ids(); ++a)
                  if(-1 < mam4::ConvProc::lmassptrcw_amode(a, m))
                    qqcw[mam4::ConvProc::lmassptrcw_amode(a, m)][kk] =
                        progs.q_aero_c[m][a][kk];
              }
            });  // parallel_for nlevs
        bool ptend_lq[pcnst];
        printb("bef:qqcw:", qqcw[23](63));
        mam4::aero_model_drydep(
            // inputs
            team, fraction_landuse, atm.temperature, atm.pressure,
            atm.interface_pressure, atm.hydrostatic_dp,
            ekat::subview(qtracers, icol), dgncur_awet, wet_dens, obklen[icol],
            surfric[icol], landfrac[icol], icefrac[icol], ocnfrac[icol],
            friction_velocity[icol], aerodynamical_resistance[icol], dt,
            // input-outputs
            qqcw,
            // outputs
            ekat::subview(d_qtracers_dt, icol), ptend_lq,
            ekat::subview(aerdepdrycw, icol), ekat::subview(aerdepdryis, icol),
            // work arrays
            rho, vlc_dry, vlc_trb, vlc_grv, dqdt_tmp);
        printb("aft:qqcw:", qqcw[23](63));
      });  // parallel_for for ncols
}

void populated_fraction_landuse(MAMDryDep::view_1d flu[11], const int ncol) {
  Real temp[11] = {0.28044346587077795E-003, 0.26634987180780171E-001,
                   0.16803558403621365E-001, 0.18076055155371872E-001,
                   0.00000000000000000E+000, 0.00000000000000000E+000,
                   0.91803784897907303E+000, 0.17186036997038400E-002,
                   0.00000000000000000E+000, 0.00000000000000000E+000,
                   0.18448503115578840E-001};

  for(int icol = 0; icol < ncol; ++icol) {
    for(int kk = 0; kk < 11; ++kk) {
      flu[kk](icol) = temp[kk];
    }
  }
}

}  // namespace
}  // namespace scream

#endif
