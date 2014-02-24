// Action_CheckStructure
#include <cmath>
#include <algorithm> // sort
#include "Action_CheckStructure.h"
#include "CpptrajStdio.h"
#include "Timer.h" // DEBUG

// CONSTRUCTOR
Action_CheckStructure::Action_CheckStructure() : 
  bondoffset_(1.15),
  nonbondcut2_(0.64), // 0.8^2
  bondcheck_(true),
  silent_(false),
  skipBadFrames_(false),
  CurrentParm_(0),
  debug_(0)
{} 

void Action_CheckStructure::Help() {
  mprintf("\t[<mask1>] [reportfile <report>] [noimage] [skipbadframes]\n"
          "\t[offset <offset>] [cut <cut>] [nobondcheck]\n"
          "  Check frames for atomic overlaps and unusual bond lengths\n");
}

// DESTRUCTOR
Action_CheckStructure::~Action_CheckStructure() {
  outfile_.CloseFile();
}

// Action_CheckStructure::Init()
Action::RetType Action_CheckStructure::Init(ArgList& actionArgs, TopologyList* PFL, FrameList* FL,
                          DataSetList* DSL, DataFileList* DFL, int debugIn)
{
  debug_ = debugIn;
  // Get Keywords
  InitImaging( !(actionArgs.hasKey("noimage")) );
  std::string reportFile = actionArgs.GetStringKey("reportfile");
  bondoffset_ = actionArgs.getKeyDouble("offset",1.15);
  double nonbondcut = actionArgs.getKeyDouble("cut",0.8);
  bondcheck_ = !actionArgs.hasKey("nobondcheck");
  skipBadFrames_ = actionArgs.hasKey("skipbadframes");
  // Hidden option, for use when used by other actions
  silent_ = actionArgs.hasKey("silent");

  // Get Masks
  Mask1_.SetMaskString( actionArgs.GetMaskNext() );

  mprintf("    CHECKSTRUCTURE: Checking atoms in mask [%s]",Mask1_.MaskString());
  if (!UseImage()) 
    mprintf(", imaging off");
  if (!reportFile.empty())
    mprintf(", output to %s",reportFile.c_str());
  mprintf(".\n");
  if (!bondcheck_) {
    mprintf("\tChecking inter-atom distances only.\n");
    mprintf("\tWarnings will be printed for non-bond distances < %.2f Ang.\n", nonbondcut);
  } else {
    mprintf("\tChecking inter-atom and bond distances.\n");
    mprintf("\tWarnings will be printed for bond lengths > eq + %.2f Ang\n",
            bondoffset_);
    mprintf("\tand non-bond distances < %.2f Ang.\n",nonbondcut);
    if (skipBadFrames_)
      mprintf("\tFrames with problems will be skipped.\n");
  }
  nonbondcut2_ = nonbondcut * nonbondcut;

  if (outfile_.OpenEnsembleWrite(reportFile, DSL->EnsembleNum()))
    return Action::ERR;

  return Action::OK;
}

/** Set up bond arrays in a sorted list for easy access during loop
  * over all pairs of atoms. Only use bonds for which both atoms are in
  * the mask.
  */
void Action_CheckStructure::SetupBondlist(BondArray const& BndLst, BondParmArray const& Parm,
                                          AtomMask const& cMask)
{
  bond_list bnd;
  for (BondArray::const_iterator bondatom = BndLst.begin();
                                 bondatom != BndLst.end(); ++bondatom) 
  {
    bnd.atom1 = bondatom->A1();
    if ( !cMask.AtomInCharMask(bnd.atom1) ) continue;
    bnd.atom2 = bondatom->A2();
    if ( !cMask.AtomInCharMask(bnd.atom2) ) continue;
    if ( bondatom->Idx() < 0 ) // sanity check
      mprintf("Warning: Bond parameters not present for atoms %i-%i, skipping.\n",
              bondatom->A1()+1, bondatom->A2()+1);
    else {
      bnd.req = Parm[ bondatom->Idx() ].Req() + bondoffset_;
      bnd.req *= bnd.req; // Store squared values.
      bondL_.push_back(bnd);
    }
  }
}

// Action_CheckStructure::Setup()
/** Determine what atoms each mask pertains to for the current parm file.
  * Also determine whether imaging should be performed. Check if parm
  * has bonds. If so, set up a list of the bonds between atoms in mask,
  * along with the expected bond lengths. Store bond lengths as 
  * (req + bondoffset)^2 for easy comparison with calculated distance^2.
  */
Action::RetType Action_CheckStructure::Setup(Topology* currentParm, Topology** parmAddress) {
  unsigned int totalbonds = 0;

  CurrentParm_ = currentParm;
  SetupImaging( currentParm->BoxType() );
  bondL_.clear();
  // Setup integer mask.
  if ( currentParm->SetupIntegerMask( Mask1_ ) ) return Action::ERR;
  if (Mask1_.None()) {
    mprintf("Warning: Mask has no atoms.\n");
    return Action::ERR;
  }
  // Check bonds
  if (bondcheck_) {
    // Set up character mask to easily determine whether
    // both atoms of a bond are in the mask.
    AtomMask cMask = Mask1_;
    cMask.ConvertToCharMask();
    SetupBondlist( currentParm->Bonds(),  currentParm->BondParm(), cMask );
    SetupBondlist( currentParm->BondsH(), currentParm->BondParm(), cMask );
    if (bondL_.empty())
      mprintf("Warning: No bond info in parm %s, will not check bonds.\n",CurrentParm_->c_str());
    else {
      // Since in the loop atom1 always < atom2, enforce this with parameters.
      // Sort by atom1, then by atom2
      std::sort( bondL_.begin(), bondL_.end(), bond_list_cmp() );
      // DEBUG
      if (debug_>0) {
        mprintf("DEBUG:\tSorted bond list:\n");
        for (BondListType::const_iterator it = bondL_.begin(); it!=bondL_.end(); it++)
          mprintf("\t%8i %8i %6.2lf\n", it->atom1, it->atom2, it->req);
      }
    }
  }
  totalbonds = bondL_.size();
  // Insert a placeholder. Since atoms -1 -1 dont exist the bond will never
  // actually be accessed. If bond info is present this serves to indicate 
  // to the pair loop there are no more bonds to check. If no bond info is
  // present this serves to skip the bond check entirely.
  bond_list bnd; 
  bnd.atom1 = -1;
  bnd.atom2 = -1;
  bondL_.push_back(bnd);
# ifdef _OPENMP
  BondListType listOfBonds = bondL_;
  bondL_.clear();
  BondListType::const_iterator currentBond = listOfBonds.begin();
  // Loop over all pairs
  bnd.D2 = 0.0;
  bnd.problem = 0;
  for (AtomMask::const_iterator atom1 = Mask1_.begin(); atom1 != Mask1_.end(); ++atom1)
    for (AtomMask::const_iterator atom2 = atom1 + 1; atom2 != Mask1_.end(); ++atom2)
    {
      bnd.atom1 = *atom1;
      bnd.atom2 = *atom2;
      bnd.req = -1.0;
      if ( (*atom1==currentBond->atom1) && (*atom2==currentBond->atom2) )
        // Atoms bonded, set bond eq length.
        bnd.req = (currentBond++)->req;
      bondL_.push_back( bnd );
    }
# endif
  // Print imaging info for this parm
  mprintf("\tChecking atoms in '%s' (%i atoms, %u bonds)",Mask1_.MaskString(),
          Mask1_.Nselected(), totalbonds);
  if (ImagingEnabled())
    mprintf(", imaging on");
  else
    mprintf(", imaging off");
  mprintf(".\n");
  return Action::OK;  
}

// Action_CheckStructure::CheckFrame()
int Action_CheckStructure::CheckFrame(int frameNum, Frame const& currentFrame) {
  Matrix_3x3 ucell, recip;
  int Nproblems = 0;
  // Get imaging info for non-orthogonal box
  if (ImageType()==NONORTHO) currentFrame.BoxCrd().ToRecip(ucell, recip);
#ifdef _OPENMP
  Timer firstloop, secondloop;
  static const int NONE = 0; //FIXME Use bitmasks?
  static const int OVERLAP = 1;
  static const int BOND = 2;
  static const int BOTH = 3;
  int idx, bondLsize;
  bondLsize = (int)bondL_.size();
  firstloop.Start();
# ifdef _OPENMP
# pragma omp parallel private(idx) reduction(+: Nproblems)
  {
# pragma omp for
# endif
    for (idx = 0; idx < bondLsize; ++idx) {
      bondL_[idx].problem = NONE;
      bondL_[idx].D2 = DIST2(currentFrame.XYZ(bondL_[idx].atom1),
                             currentFrame.XYZ(bondL_[idx].atom2),
                             ImageType(), currentFrame.BoxCrd(), ucell, recip);
      if (bondL_[idx].req > 0.0) { // Check bond length
        if (bondL_[idx].D2 > bondL_[idx].req) {
          ++Nproblems;
          bondL_[idx].problem = BOND;
        }
      }
      if (bondL_[idx].D2 < nonbondcut2_) { // Always check overlap
        ++Nproblems;
        bondL_[idx].problem += OVERLAP;
      }
    }
# ifdef _OPENMP
  } // END pragma OMP parallel
  firstloop.Stop();
  secondloop.Start();
# endif
  // Second loop for writing out results sequentially
  for (idx = 0; idx < bondLsize; ++idx) {
    int problem = bondL_[idx].problem;
    if (problem > NONE) {
      int atom1 = bondL_[idx].atom1;
      int atom2 = bondL_[idx].atom2;
      double Dist = sqrt(bondL_[idx].D2);
      if (problem == BOND || problem == BOTH)
        outfile_.Printf(
                "%i\t Warning: Unusual bond length %i:%s to %i:%s (%.2lf)\n", frameNum,
                atom1+1, CurrentParm_->TruncResAtomName(atom1).c_str(), 
                atom2+1, CurrentParm_->TruncResAtomName(atom2).c_str(), Dist);
      if (problem == OVERLAP || problem == BOTH) 
      outfile_.Printf(
              "%i\t Warning: Atoms %i:%s and %i:%s are close (%.2lf)\n", frameNum,
              atom1+1, CurrentParm_->TruncResAtomName(atom1).c_str(),
              atom2+1, CurrentParm_->TruncResAtomName(atom2).c_str(), Dist);
    }
  }
  secondloop.Stop();
  firstloop.WriteTiming(1,"FirstLoop");
  secondloop.WriteTiming(1, "SecondLoop");
#else
  // Begin loop
  BondListType::const_iterator currentBond = bondL_.begin();
  int lastidx = Mask1_.Nselected() - 1;
  Timer firstloop;
  firstloop.Start();
  for (int maskidx1 = 0; maskidx1 < lastidx; maskidx1++) {
    int atom1 = Mask1_[maskidx1];
    for (int maskidx2 = maskidx1 + 1; maskidx2 < Mask1_.Nselected(); maskidx2++) {
      int atom2 = Mask1_[maskidx2];
      // Get distance^2
      double D2 = DIST2(currentFrame.XYZ(atom1), currentFrame.XYZ(atom2),
                        ImageType(), currentFrame.BoxCrd(), ucell, recip);
      //mprintf("DEBUG:\t%i-%i D^2= %f\n", atom1+1, atom2+1, D2);
      if ( (atom1==currentBond->atom1) && (atom2==currentBond->atom2) ) {
        // Atoms bonded, check bond length.
        // req has been precalced to (req + bondoffset)^2
        double bondmax = currentBond->req;
        // Check for long bond length; distance2 > (req+bondoffset)^2
        if (D2 > bondmax) {
          ++Nproblems;
          outfile_.Printf(
                  "%i\t Warning: Unusual bond length %i:%s to %i:%s (%.2lf)\n", frameNum,
                  atom1+1, CurrentParm_->TruncResAtomName(atom1).c_str(), 
                  atom2+1, CurrentParm_->TruncResAtomName(atom2).c_str(), sqrt(D2));
        }
        // Next bond
        ++currentBond;
      }
      // Always check overlap
      if (D2 < nonbondcut2_) {
        ++Nproblems;
        outfile_.Printf(
                "%i\t Warning: Atoms %i:%s and %i:%s are close (%.2lf)\n", frameNum,
                atom1+1, CurrentParm_->TruncResAtomName(atom1).c_str(), 
                atom2+1, CurrentParm_->TruncResAtomName(atom2).c_str(), sqrt(D2));
      }
    } // END second loop over mask atoms
  } // END first loop over mask atoms
  firstloop.Stop();
  firstloop.WriteTiming(1, "Loop");
#endif
  return Nproblems;
}

// Action_CheckStructure::DoAction()
Action::RetType Action_CheckStructure::DoAction(int frameNum, Frame* 
                                                currentFrame, Frame** frameAddress)
{
  if (CheckFrame(frameNum+1, *currentFrame) > 0 && skipBadFrames_)
    return Action::SUPPRESSCOORDOUTPUT;
  return Action::OK;
}
