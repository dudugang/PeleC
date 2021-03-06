#ifndef _PROB_H_
#define _PROB_H_

#include <AMReX_Geometry.H>
#include <AMReX_FArrayBox.H>

#include "mechanism.h"

#include "PeleC.H"
#include "IndexDefines.H"
#include "Constants.H"
#include "EOS.H"
#include "Tagging.H"
#include "ProblemDerive.H"
#include "prob_parm.H"

AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
pc_initdata(
  int i,
  int j,
  int k,
  amrex::Array4<amrex::Real> const& state,
  amrex::GeometryData const& geomdata)
{
  // Geometry
  const amrex::Real* prob_lo = geomdata.ProbLo();
  const amrex::Real* dx = geomdata.CellSize();

  const amrex::Real x = prob_lo[0] + (i + 0.5) * dx[0];
  const amrex::Real y = prob_lo[1] + (j + 0.5) * dx[1];
  // const amrex::Real z = prob_lo[2] + (k + 0.5) * dx[2];

  const amrex::Real cost = cos(M_PI / 180.0 * ProbParm::angle);
  const amrex::Real sint = sin(M_PI / 180.0 * ProbParm::angle);
  const amrex::Real xp = x * cost + y * sint;

  if (xp < 0.5 * ProbParm::L) {
    state(i, j, k, URHO) = ProbParm::rhol;
    state(i, j, k, UMX) = 0.0;
    state(i, j, k, UMY) = 0.0;
    state(i, j, k, UMZ) = 0.0;
    state(i, j, k, UEINT) = ProbParm::rhol * ProbParm::eintl;
    state(i, j, k, UEDEN) = ProbParm::rhol * ProbParm::eintl;
    state(i, j, k, UTEMP) = ProbParm::Tl;
    for (int n = 0; n < NUM_SPECIES; n++)
      state(i, j, k, UFS + n) = ProbParm::rhol * ProbParm::massfrac[n];
  } else {
    state(i, j, k, URHO) = ProbParm::rhor;
    state(i, j, k, UMX) = 0.0;
    state(i, j, k, UMY) = 0.0;
    state(i, j, k, UMZ) = 0.0;
    state(i, j, k, UEINT) = ProbParm::rhor * ProbParm::eintr;
    state(i, j, k, UEDEN) = ProbParm::rhor * ProbParm::eintr;
    state(i, j, k, UTEMP) = ProbParm::Tr;
    for (int n = 0; n < NUM_SPECIES; n++)
      state(i, j, k, UFS + n) = ProbParm::rhor * ProbParm::massfrac[n];
  }
}

AMREX_GPU_DEVICE
AMREX_FORCE_INLINE
void
bcnormal(
  const amrex::Real* /*x[AMREX_SPACEDIM]*/,
  const amrex::Real* /*s_int[NVAR]*/,
  amrex::Real* /*s_ext[NVAR]*/,
  const int /*idir*/,
  const int /*sgn*/,
  const amrex::Real /*time*/,
  amrex::GeometryData const& /*geomdata*/)
{
}

void pc_prob_close();

using ProblemTags = EmptyProbTagStruct;
using ProblemDerives = EmptyProbDeriveStruct;

#endif
