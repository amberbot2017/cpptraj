#include "Action_Energy.h"
#include "CpptrajStdio.h"

// CONSTRUCTOR
Action_Energy::Action_Energy() : currentParm_(0) {}


void Action_Energy::Help() {
  mprintf("\t[<name>] [<mask1>] [out <filename>]\n"
          "\t[bond] [angle] [dihedral] [nb14] [nonbond]\n"
          "  Calculate energy for atoms in mask.\n");
}

/// DataSet aspects
static const char* Estring[] = {"bond", "angle", "dih", "vdw14", "elec14", "vdw", "elec", "total"};

/// Calculation types
static const char* Cstring[] = {"Bond", "Angle", "Torsion", "1-4_Nonbond", "Nonbond" };

int Action_Energy::AddSet(Etype typeIn, DataSetList& DslIn, DataFile* outfile,
                          std::string const& setname)
{
  Energy_[typeIn] = DslIn.AddSet(DataSet::DOUBLE, MetaData(setname, Estring[typeIn]));
  if (Energy_[typeIn] == 0) return 1;
  if (outfile != 0) outfile->AddDataSet( Energy_[typeIn] );
  return 0;
}

// Action_Energy::Init()
Action::RetType Action_Energy::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  ENE_.SetDebug( debugIn );
  // Get keywords
  DataFile* outfile = init.DFL().AddDataFile( actionArgs.GetStringKey("out"), actionArgs );
  // Which terms will be calculated?
  Ecalcs_.clear();
  if (actionArgs.hasKey("bond"))     Ecalcs_.push_back(BND);
  if (actionArgs.hasKey("angle"))    Ecalcs_.push_back(ANG);
  if (actionArgs.hasKey("dihedral")) Ecalcs_.push_back(DIH);
  if (actionArgs.hasKey("nb14"))     Ecalcs_.push_back(N14);
  if (actionArgs.hasKey("nonbond"))  Ecalcs_.push_back(NBD);
  // If nothing is selected, select all.
  if (Ecalcs_.empty()) {
    for (int c = 0; c <= (int)NBD; c++)
      Ecalcs_.push_back( (CalcType)c );
  }

  // Get Masks
  Mask1_.SetMaskString( actionArgs.GetMaskNext() );

  // DataSet
  std::string setname = actionArgs.GetStringNext();
  if (setname.empty())
    setname = init.DSL().GenerateDefaultName("ENE");
  Energy_.clear();
  Energy_.resize( (int)TOTAL + 1, 0 );
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc)
  {
    switch (*calc) {
      case BND: if (AddSet(BOND, init.DSL(), outfile, setname)) return Action::ERR; break;
      case ANG: if (AddSet(ANGLE, init.DSL(), outfile, setname)) return Action::ERR; break;
      case DIH: if (AddSet(DIHEDRAL, init.DSL(), outfile, setname)) return Action::ERR; break;
      case N14:
        if (AddSet(V14, init.DSL(), outfile, setname)) return Action::ERR;
        if (AddSet(Q14, init.DSL(), outfile, setname)) return Action::ERR;
        break;
      case NBD:
        if (AddSet(VDW, init.DSL(), outfile, setname)) return Action::ERR;
        if (AddSet(ELEC, init.DSL(), outfile, setname)) return Action::ERR;
        break;
    }
  }
//  if (Ecalcs_.size() > 1) {
    if (AddSet(TOTAL, init.DSL(), outfile, setname)) return Action::ERR;
//  }
      
  mprintf("    ENERGY: Calculating energy for atoms in mask '%s'\n", Mask1_.MaskString());
  mprintf("\tCalculating terms:");
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc)
    mprintf(" %s", Cstring[*calc]);
  mprintf("\n");

  return Action::OK;
}

// Action_Energy::Setup()
/** Set angle up for this parmtop. Get masks etc.
  */
Action::RetType Action_Energy::Setup(ActionSetup& setup) {
  if (setup.Top().SetupCharMask(Mask1_)) return Action::ERR;
  if (Mask1_.None()) {
    mprintf("Warning: Mask '%s' selects no atoms.\n", Mask1_.MaskString());
    return Action::SKIP;
  }
  Mask1_.MaskInfo();
  Imask_ = AtomMask(Mask1_.ConvertToIntMask(), Mask1_.Natom());
  currentParm_ = setup.TopAddress();
  return Action::OK;
}

// Action_Energy::DoAction()
Action::RetType Action_Energy::DoAction(int frameNum, ActionFrame& frm) {
  double Etot = 0.0, ene, ene2;
  for (calc_it calc = Ecalcs_.begin(); calc != Ecalcs_.end(); ++calc)
  {
    switch (*calc) {
      case BND:
        ene = ENE_.E_bond(frm.Frm(), *currentParm_, Mask1_);
        Energy_[BOND]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case ANG:
        ene = ENE_.E_angle(frm.Frm(), *currentParm_, Mask1_);
        Energy_[ANGLE]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case DIH:
        ene = ENE_.E_torsion(frm.Frm(), *currentParm_, Mask1_);
        Energy_[DIHEDRAL]->Add(frameNum, &ene);
        Etot += ene;
        break;
      case N14:
        ene = ENE_.E_14_Nonbond(frm.Frm(), *currentParm_, Mask1_, ene2);
        Energy_[V14]->Add(frameNum, &ene);
        Energy_[Q14]->Add(frameNum, &ene2);
        Etot += (ene + ene2);
        break;
      case NBD:
        ene = ENE_.E_Nonbond(frm.Frm(), *currentParm_, Imask_, ene2);
        Energy_[VDW]->Add(frameNum, &ene);
        Energy_[ELEC]->Add(frameNum, &ene2);
        Etot += (ene + ene2);
        break;
    }
  }

  Energy_[TOTAL]->Add(frameNum, &Etot);
# ifdef USE_SANDERLIB
  // DEBUG
  SANDER_.CalcEnergy(currentParm_, frm.Frm());
  mprintf("ESANDER: Ebond= %12.4f  Eangle= %12.4f  Edih= %12.4f\n"
          "ESANDER: Elec14= %12.4f  Evdw14= %12.4f  Elec= %12.4f  VDW= %12.4f\n",
          SANDER_.Ebond(), SANDER_.Eangle(), SANDER_.Edihedral(),
          SANDER_.Eelec14(), SANDER_.Evdw14(), SANDER_.Eelec(), SANDER_.Evdw());
#endif
  return Action::OK;
}

void Action_Energy::Print() {
  mprintf("Timing for energy: '%s' ('%s')\n", Energy_[TOTAL]->legend(),
           Mask1_.MaskString());
  ENE_.PrintTiming();
}
