// Action_Outtraj 
#include "Action_Outtraj.h"
#include "CpptrajStdio.h"

void Action_Outtraj::Help() {
  mprintf("\t<filename> [ trajout args ]\n"
          "\t[maxmin <dataset> min <min> max <max>] ...\n"
          "  Like 'trajout', but coordinate output occurs during actions rather than at the end.\n");
}

// Action_Outtraj::Init()
Action::RetType Action_Outtraj::Init(ArgList& actionArgs, ActionInit& init, int debugIn)
{
  // Set up output traj
  outtraj_.SetDebug(debugIn);
  std::string trajfilename = actionArgs.GetStringNext();
  if (trajfilename.empty()) {
    mprinterr("Error: No filename given.\nError: Usage: ");
    Help();
    return Action::ERR;
  }
  associatedParm_ = init.DSL().GetTopology(actionArgs);
  if (associatedParm_ == 0) {
    mprinterr("Error: Could not get associated topology for %s\n",trajfilename.c_str());
    return Action::ERR;
  }
  // If maxmin, get the name of the dataset as well as the max and min values.
  double lastmin = 0.0;
  double lastmax = 0.0;
  while ( actionArgs.Contains("maxmin") ) {
    std::string datasetName = actionArgs.GetStringKey("maxmin");
    if (!datasetName.empty()) {
      DataSet* dset = init.DSL().GetDataSet(datasetName);
      if (dset==0) {
        mprintf("Error: maxmin: Could not get dataset %s\n",datasetName.c_str());
        return Action::ERR;
      } else {
        // Currently only allow int, float, or double datasets
        if (dset->Type() != DataSet::INTEGER &&
            dset->Type() != DataSet::FLOAT &&
            dset->Type() != DataSet::DOUBLE) 
        {
          mprinterr("Error: maxmin: Only int, float, or double dataset (%s) supported.\n",
                  datasetName.c_str());
          return Action::ERR;
        }
        Dsets_.push_back( (DataSet_1D*)dset );
        Max_.push_back( actionArgs.getKeyDouble("max",lastmax) );
        Min_.push_back( actionArgs.getKeyDouble("min",lastmin) );
        lastmax = Max_.back();
        lastmin = Min_.back();
      }
    } else {
      mprinterr("Error: maxmin Usage: maxmin <setname> max <max> min <min>\n");
      return Action::ERR;
    }
  }
  // Initialize output trajectory with remaining arguments
  if ( outtraj_.InitEnsembleTrajWrite(trajfilename, actionArgs.RemainingArgs(), 
                                      TrajectoryFile::UNKNOWN_TRAJ, init.DSL().EnsembleNum()) ) 
    return Action::ERR;
  isSetup_ = false;

  mprintf("    OUTTRAJ: Writing frames associated with topology '%s'\n", associatedParm_->c_str());
  for (unsigned int ds = 0; ds < Dsets_.size(); ++ds)
    mprintf("\tmaxmin: Printing trajectory frames based on %g <= %s <= %g\n",
            Min_[ds], Dsets_[ds]->legend(), Max_[ds]);

  return Action::OK;
} 

// Action_Outtraj::Setup()
Action::RetType Action_Outtraj::Setup(ActionSetup& setup) {
  if (associatedParm_->Pindex() != setup.Top().Pindex())
    return Action::SKIP;
  if (!isSetup_) { // TODO: Trajout IsOpen?
    if (outtraj_.SetupTrajWrite(setup.TopAddress(), setup.CoordInfo(), setup.Nframes()))
      return Action::ERR;
    outtraj_.PrintInfo(0);
    isSetup_ = true;
  }
  return Action::OK;
}

// Action_Outtraj::DoAction()
/** If a dataset was specified for maxmin, check if this structure
  * satisfies the criteria; if so, write. Otherwise just write.
  */
Action::RetType Action_Outtraj::DoAction(int frameNum, ActionFrame& frm) {
  // If dataset defined, check if frame is within max/min
  if (!Dsets_.empty()) {
    for (unsigned int ds = 0; ds < Dsets_.size(); ++ds)
    {
      double dVal = Dsets_[ds]->Dval(frameNum);
      //mprintf("DBG: maxmin[%u]: dVal = %f, min = %f, max = %f\n",ds,dVal,Min_[ds],Max_[ds]);
      // If value from dataset not within min/max, exit now.
      if (dVal < Min_[ds] || dVal > Max_[ds]) return Action::OK;
    }
  }
  if ( outtraj_.WriteSingle(frameNum, frm.Frm()) != 0 ) 
    return Action::ERR;
  return Action::OK;
}

// Action_Outtraj::Print()
/** Close trajectory. Indicate how many frames were actually written.
  */
void Action_Outtraj::Print() {
  mprintf("  OUTTRAJ: [%s] Wrote %i frames.\n",outtraj_.Traj().Filename().base(),
          outtraj_.Traj().NframesWritten());
  outtraj_.EndTraj();
}
