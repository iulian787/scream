#ifndef P3_FUNCTIONS_HPP
#define P3_FUNCTIONS_HPP

#include "share/scream_types.hpp"
#include "share/scream_pack_kokkos.hpp"
#include "p3_constants.hpp"

namespace scream {
namespace p3 {

/*
 * Functions is a stateless struct used to encapsulate a
 * number of functions for p3. We use the ETI pattern for
 * these functions.
 *
 * P3 assumptions:
 *  - Kokkos team policies have a vector length of 1
 */

template <typename ScalarT, typename DeviceT>
struct Functions
{

  //
  // ------- Types --------
  //

  using Scalar = ScalarT;
  using Device = DeviceT;

  template <typename S>
  using BigPack = scream::pack::BigPack<S>;
  template <typename S>
  using SmallPack = scream::pack::SmallPack<S>;
  using IntSmallPack = scream::pack::IntSmallPack;

  using Pack = BigPack<Scalar>;
  using Spack = SmallPack<Scalar>;

  template <typename S>
  using Mask = scream::pack::Mask<BigPack<S>::n>;

  template <typename S>
  using SmallMask = scream::pack::Mask<SmallPack<S>::n>;

  using Smask = SmallMask<Scalar>;

  using KT = KokkosTypes<Device>;

  using G = Globals<Scalar>;
  using C = Constants<Scalar>;

  template <typename S>
  using view_1d = typename KT::template view_1d<S>;
  template <typename S>
  using view_2d = typename KT::template view_2d<S>;

  using view_1d_table = typename KT::template view_1d_table<Scalar, G::MU_R_TABLE_DIM>;
  using view_2d_table = typename KT::template view_2d_table<Scalar, G::VTABLE_DIM0, G::VTABLE_DIM1>;

  using view_itab_table    = typename KT::template view<const Scalar[C::DENSIZE][C::RIMSIZE][C::ISIZE][C::TABSIZE]>;
  using view_itabcol_table = typename KT::template view<const Scalar[C::DENSIZE][C::RIMSIZE][C::ISIZE][C::RCOLLSIZE][C::COLTABSIZE]>;

  template <typename S, int N>
  using view_1d_ptr_array = typename KT::template view_1d_ptr_carray<S, N>;

  using MemberType = typename KT::MemberType;

  //
  // --------- Functions ---------
  //

  // -- Table3

  struct Table3 {
    IntSmallPack dumii, dumjj;
    Spack rdumii, rdumjj;
  };

  struct TableIce {
    IntSmallPack dumi, dumjj, dumii, dumzz;
    Spack dum1, dum4, dum5, dum6;
  };

  // Call from host to initialize the static table entries.
  static void init_kokkos_tables(
    view_2d_table& vn_table, view_2d_table& vm_table, view_1d_table& mu_r_table);

  static void init_kokkos_ice_lookup_tables(
    view_itab_table& itab, view_itabcol_table& itabcol);

  // Map (mu_r, lamr) to Table3 data.
  KOKKOS_FUNCTION
  static void lookup(const Smask& qr_gt_small, const Spack& mu_r, const Spack& lamr,
                     Table3& t);

  KOKKOS_FUNCTION
  static void lookup_ice(const Smask& qiti_gt_small, const Spack& qitot, const Spack& nitot,
                         const Spack& qirim, const Spack& rhop, TableIce& t);

  // Apply Table3 data to the table to return a value. This performs bilinear
  // interpolation within the quad given by {t.dumii, t.dumjj} x {t.dumii+1,
  // t.dumjj+1}.
  KOKKOS_FUNCTION
  static Spack apply_table(const Smask& qr_gt_small, const view_2d_table& table,
                           const Table3& t);

  KOKKOS_FUNCTION
  static Spack apply_table_ice(const Smask& qiti_gt_small, const int& index, const view_itab_table& itab,
                               const TableIce& t);

  // -- Sedimentation time step

  // Calculate the first-order upwind step in the region [k_bot,
  // k_top]. Velocity V is input, and flux is workspace and need not be
  // initialized. On input, r contains mixing ratio data at the time step start;
  // on output, it contains mixing ratio data at the time step end.
  // kdir = 1 -> vertical columns are processed from bottom to top, opposite for kdir = -1
  //
  // A subtlety is that this procedure does not do exact upwind of a mixing
  // ratio. That is because the background density rho is assumed to be static;
  // rho does not get advected. Thus, there is an inconsistency between rho and
  // r*rho at the level of |r|.

  // Evolve nfield mixing ratios simultaneously. nfield is a compile-time
  // parameter so the loops over nfield are compiled efficiently. So far the use
  // cases have no need of a runtime version.
  template <int nfield>
  KOKKOS_FUNCTION
  static void calc_first_order_upwind_step(
    const ko::Unmanaged<view_1d<const Spack> >& rho,
    const ko::Unmanaged<view_1d<const Spack> >& inv_rho, // 1/rho
    const ko::Unmanaged<view_1d<const Spack> >& inv_dzq,
    const MemberType& team,
    const Int& nk, const Int& k_bot, const Int& k_top, const Int& kdir, const Scalar& dt_sub,
    const view_1d_ptr_array<Spack, nfield>& flux, // workspace
    const view_1d_ptr_array<Spack, nfield>& V,    // (behaviorally const)
    const view_1d_ptr_array<Spack, nfield>& r);   // in/out

  // Evolve 1 mixing ratio. This is a syntax-convenience version of the above.
  KOKKOS_FUNCTION
  static void calc_first_order_upwind_step(
    const ko::Unmanaged<view_1d<const Spack> >& rho,
    const ko::Unmanaged<view_1d<const Spack> >& inv_rho, // 1/rho
    const ko::Unmanaged<view_1d<const Spack> >& inv_dzq,
    const MemberType& team,
    const Int& nk, const Int& k_bot, const Int& k_top, const Int& kdir, const Scalar& dt_sub,
    const ko::Unmanaged<view_1d<Spack> >& flux,
    const ko::Unmanaged<view_1d<const Spack> >& V,
    const ko::Unmanaged<view_1d<Spack> >& r);

  // This is the main routine. It can be called by the user if kdir is known at
  // compile time. So far it is not, so the above versions are called instead.
  template <Int kdir, int nfield>
  KOKKOS_FUNCTION
  static void calc_first_order_upwind_step(
    const ko::Unmanaged<view_1d<const Spack> >& rho,
    const ko::Unmanaged<view_1d<const Spack> >& inv_rho,
    const ko::Unmanaged<view_1d<const Spack> >& inv_dzq,
    const MemberType& team,
    const Int& nk, const Int& k_bot, const Int& k_top, const Scalar& dt_sub,
    const view_1d_ptr_array<Spack, nfield>& flux,
    const view_1d_ptr_array<Spack, nfield>& V, // (behaviorally const)
    const view_1d_ptr_array<Spack, nfield>& r);

  // -- Find layers

  // Find the bottom and top of the mixing ratio, e.g., qr. It's worth casing
  // these out in two ways: 1 thread/column vs many, and by kdir.
  KOKKOS_FUNCTION
  static Int find_bottom (
    const MemberType& team,
    const ko::Unmanaged<view_1d<const Scalar> >& v, const Scalar& small,
    const Int& kbot, const Int& ktop, const Int& kdir,
    bool& log_present);

  KOKKOS_FUNCTION
  static Int find_top (
    const MemberType& team,
    const ko::Unmanaged<view_1d<const Scalar> >& v, const Scalar& small,
    const Int& kbot, const Int& ktop, const Int& kdir,
    bool& log_present);
};

} // namespace p3
} // namespace scream

// If a GPU build, make all code available to the translation unit; otherwise,
// ETI is used.
#ifdef KOKKOS_ENABLE_CUDA
# include "p3_functions_table3_impl.hpp"
# include "p3_functions_upwind_impl.hpp"
# include "p3_functions_find_impl.hpp"
#endif

#endif
