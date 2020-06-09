#include <PeleC.H>
#include <AMReX_MLABecLaplacian.H>
#include <AMReX_MLEBABecLap.H>
#include <AMReX_MLMG.H>

#include <cmath>

using namespace amrex;

using std::string;

void
PeleC::solveEF ( Real time,
                 Real dt )
{
   BL_PROFILE("PeleC::solveEF()");

   amrex::Print() << "Solving for electric field \n";

   Real prev_time = state[State_Type].prevTime();

// Get current PhiV
   MultiFab& Ucurr = (time == prev_time) ? get_old_data(State_Type) : get_new_data(State_Type);
   MultiFab phiV_mf(Ucurr,amrex::make_alias,PhiV,1); 

// Setup a dummy charge distribution MF
   MultiFab chargeDistib(grids,dmap,1,0,MFInfo(),Factory());
#ifdef _OPENMP
#pragma omp parallel
#endif
   for (MFIter mfi(chargeDistib,true); mfi.isValid(); ++mfi)
   {   
       const Box& bx = mfi.growntilebox();
       const Array4<Real>& chrg_ar = chargeDistib.array(mfi);
       amrex::ParallelFor(bx,
       [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
       {   
           if (i == 32 && k == 32) {
               chrg_ar(i,j,k) = 1.0e4;
           } else {
               chrg_ar(i,j,k) = 0.0;
           }   
       }); 
   }

/////////////////////////////////////   
// Setup a linear operator
/////////////////////////////////////   

   LPInfo info;
   info.setAgglomeration(1);
   info.setConsolidation(1);
   info.setMetricTerm(false);

// Linear operator (EB aware if need be)   
#ifdef AMREX_USE_EB
   const auto& ebf = &dynamic_cast<EBFArrayBoxFactory const&>((parent->getLevel(level)).Factory());
   MLEBABecLap poissonOP({geom}, {grids}, {dmap}, info, {ebf});
#else
   MLABecLap poissonOP({geom}, {grids}, {dmap}, info);
#endif

// Boundary conditions for the linear operator.
   std::array<LinOpBCType,AMREX_SPACEDIM> bc_lo;
   std::array<LinOpBCType,AMREX_SPACEDIM> bc_hi;
   setBCPhiV(bc_lo,bc_hi);
   poissonOP.setDomainBC(bc_lo,bc_hi);   

// Assume all physical BC homogeneous Neumann for now
   poissonOP.setLevelBC(0, nullptr);

// Setup solver coefficient: general form is (ascal * acoef - bscal * div bcoef grad ) phi = rhs   
// For simple Poisson solve: ascal, acoef = 0 and bscal, bcoef = 1
   MultiFab acoef(grids, dmap, 1, 0, MFInfo(), Factory());
   acoef.setVal(0.0);
   poissonOP.setACoeffs(0, acoef);
   Array<MultiFab,AMREX_SPACEDIM> bcoef;
   for (int idim = 0; idim < AMREX_SPACEDIM; ++idim) {
       bcoef[idim].define(amrex::convert(grids,IntVect::TheDimensionVector(idim)), dmap, 1, 0, MFInfo(), Factory());
       bcoef[idim].setVal(1.0);
   }
   poissonOP.setBCoeffs(0, amrex::GetArrOfConstPtrs(bcoef));   
   Real ascal = 0.0;
   Real bscal = 1.0;
   poissonOP.setScalars(ascal, bscal);

   // set Dirichlet BC for EB
	// TODO : for now set upper y-dir half to 10 and lower y-dir to 0
	//        will have to find a better way to specify EB dirich values 
   MultiFab beta(grids, dmap, 1, 0, MFInfo(), Factory());
   beta.setVal(0.1);
   MultiFab phiV_BC(grids, dmap, 1, 0, MFInfo(), Factory());
#ifdef _OPENMP
#pragma omp parallel
#endif
   for (MFIter mfi(beta,true); mfi.isValid(); ++mfi)
   {   
       const Box& bx = mfi.growntilebox();
       const Array4<Real>& phiV_ar = phiV_BC.array(mfi);
       amrex::ParallelFor(bx,
       [=] AMREX_GPU_DEVICE (int i, int j, int k) noexcept
       {   
           if (j >= 32) {
               phiV_ar(i,j,k) = 10.0;
           } else {
               phiV_ar(i,j,k) = 0.0;
           }   
       }); 
   }

   poissonOP.setEBDirichlet(0,phiV_BC,beta);
    
/////////////////////////////////////   
// Setup a linear operator
/////////////////////////////////////   
   MLMG mlmg(poissonOP);

   // relative and absolute tolerances for linear solve
   const Real tol_rel = 1.e-10;
   const Real tol_abs = 0.0;

   mlmg.setVerbose(2);
       
   // Solve linear system
   phiV_mf.setVal(0.0); // initial guess for phi
   mlmg.solve({&phiV_mf}, {&chargeDistib}, tol_rel, tol_abs);

}

// Setup BC conditions for linear Poisson solve on PhiV. Directly copied from the diffusion one ...
void PeleC::setBCPhiV(std::array<LinOpBCType,AMREX_SPACEDIM> &linOp_bc_lo,
                      std::array<LinOpBCType,AMREX_SPACEDIM> &linOp_bc_hi) {

   const BCRec& bc = get_desc_lst()[State_Type].getBC(PhiV);

   for (int idim = 0; idim < AMREX_SPACEDIM; ++idim)
   {
      if (Geom().isPeriodic(idim))
      {    
         linOp_bc_lo[idim] = linOp_bc_hi[idim] = LinOpBCType::Periodic;
      }    
      else 
      {
         int pbc = bc.lo(idim);  
         if (pbc == EXT_DIR)
         {    
            linOp_bc_lo[idim] = LinOpBCType::Dirichlet;
         } 
         else if (pbc == FOEXTRAP    ||
                  pbc == REFLECT_EVEN )
         {   
            linOp_bc_lo[idim] = LinOpBCType::Neumann;
         }   
         else
         {   
            linOp_bc_lo[idim] = LinOpBCType::bogus;
         }   
         
         pbc = bc.hi(idim);  
         if (pbc == EXT_DIR)
         {    
            linOp_bc_hi[idim] = LinOpBCType::Dirichlet;
         } 
         else if (pbc == FOEXTRAP    ||
                  pbc == REFLECT_EVEN )
         {   
            linOp_bc_hi[idim] = LinOpBCType::Neumann;
         }   
         else
         {   
            linOp_bc_hi[idim] = LinOpBCType::bogus;
         }   
      }
   }

}
